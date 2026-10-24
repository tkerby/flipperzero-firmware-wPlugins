#include "view_emf.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <gui/elements.h>
#include <stdio.h>
#include <math.h>

#define EMF_POLL_PERIOD_MS 60

/* ADC wiring: Hall-effect sensor (SS49E) on physical pin 16 / PC0.
 * STM32WB55 maps PC0 -> ADC channel 1 (PCn -> ADC channel n+1 for this bank). */
#define EMF_ADC_GPIO    (&gpio_ext_pc0)
#define EMF_ADC_CHANNEL FuriHalAdcChannel1

/* A real K-II/EMF meter doesn't look at DC level - it uses an induction
 * coil (which only responds to a CHANGING field) feeding a bandpass filter
 * tuned to the ELF/VLF mains-hum band (~30Hz-20kHz, centered on the local
 * grid frequency). Our Hall sensor is a DC-capable sensor, but we can get
 * the same real effect in software: sample it fast and run a Goertzel
 * filter (a cheap single-bin DFT) tuned to 50Hz AND 60Hz, then take
 * whichever is stronger. That isolates the genuine AC "mains hum" ripple
 * riding on the sensor's output, ignoring slow baseline drift - this is
 * the actual physical quantity a real EMF meter reports, not a raw voltage
 * deviation. */
#define EMF_GPIO_SAMPLE_RATE_HZ   640
#define EMF_GPIO_BUFFER_SIZE      128 /* 200ms capture window -> ~5Hz bin resolution */
#define EMF_GPIO_SAMPLE_PERIOD_US (1000000 / EMF_GPIO_SAMPLE_RATE_HZ)
#define EMF_MAINS_FREQ_A_HZ       50.0f
#define EMF_MAINS_FREQ_B_HZ       60.0f
#define EMF_GOERTZEL_PI           3.14159265358979323846f

/* Calibration thresholds on the AC ripple AMPLITUDE (mV) - tune these to
 * your actual sensor/environment; real sensitivity depends heavily on the
 * specific SS49E unit and how close it sits to wiring/transformers. */
#define EMF_GPIO_THRESH_1_MV 2.0f
#define EMF_GPIO_THRESH_2_MV 6.0f
#define EMF_GPIO_THRESH_3_MV 15.0f
#define EMF_GPIO_THRESH_4_MV 35.0f

/* "Internal" mode: no wiring needed. We do NOT use the NFC chip - its
 * field-detect only reacts to an active 13.56MHz NFC reader nearby (a
 * phone, another reader), not to generic magnetic/electric fields, so it
 * never moved in practice. Instead we reuse the real CC1101 sub-GHz radio
 * (same chip Spirit Box/Radio Scanner use) and read its RSSI across a
 * handful of fixed frequencies as a genuine ambient-RF-noise gauge: real
 * nearby electronics, switching power supplies, and RF devices DO move
 * this reading; a perfectly quiet RF environment sits near the chip's
 * noise floor (~ -100dBm or lower). */
#define EMF_RF_FREQ_MIN_HZ 300000000UL
#define EMF_RF_FREQ_MAX_HZ 348000000UL
#define EMF_RF_STEPS       8
#define EMF_RF_STEP_HZ     ((EMF_RF_FREQ_MAX_HZ - EMF_RF_FREQ_MIN_HZ) / EMF_RF_STEPS)
#define EMF_RF_SETTLE_MS   5

#define EMF_RF_THRESH_1_DBM -95.0f
#define EMF_RF_THRESH_2_DBM -85.0f
#define EMF_RF_THRESH_3_DBM -75.0f
#define EMF_RF_THRESH_4_DBM -60.0f

#define EMF_WORKER_STACK_SIZE 2048

typedef enum {
    EmfSourceInternal, /* ambient RF noise via CC1101 RSSI */
    EmfSourceExternal, /* Hall sensor via GPIO ADC */
} EmfSource;

typedef struct {
    EmfSource source;
    float display_value; /* dBm (internal) or millivolts (external) */
    int level; /* 0 (normal/green) .. 4 (critical/red) */
} EmfViewModel;

struct EmfView {
    View* view;
    FuriThread* thread;
    volatile bool running;
    volatile EmfSource requested_source;
};

static int emf_level_from_ripple_mv(float ripple_mv) {
    if(ripple_mv >= EMF_GPIO_THRESH_4_MV) return 4;
    if(ripple_mv >= EMF_GPIO_THRESH_3_MV) return 3;
    if(ripple_mv >= EMF_GPIO_THRESH_2_MV) return 2;
    if(ripple_mv >= EMF_GPIO_THRESH_1_MV) return 1;
    return 0;
}

/* Goertzel algorithm: a single-bin DFT, i.e. "how much of target_hz is in
 * this buffer" - far cheaper than a full FFT when we only care about one
 * (or two) known frequencies. Returns an approximate peak amplitude, in the
 * same units as the input samples. */
static float
    emf_goertzel_magnitude(const float* samples, int n, float sample_rate_hz, float target_hz) {
    int k = (int)(0.5f + ((float)n * target_hz) / sample_rate_hz);
    float omega = (2.0f * EMF_GOERTZEL_PI * (float)k) / (float)n;
    float coeff = 2.0f * cosf(omega);
    float q0 = 0.0f, q1 = 0.0f, q2 = 0.0f;

    for(int i = 0; i < n; i++) {
        q0 = coeff * q1 - q2 + samples[i];
        q2 = q1;
        q1 = q0;
    }

    float real_part = q1 - q2 * cosf(omega);
    float imag_part = q2 * sinf(omega);
    return sqrtf(real_part * real_part + imag_part * imag_part) * (2.0f / (float)n);
}

/* Captures a ~200ms burst from the Hall sensor and returns the stronger of
 * the 50Hz/60Hz Goertzel bins - i.e. real AC mains-hum ripple amplitude,
 * regardless of which grid frequency the user's region runs on. */
static float emf_measure_mains_ripple_mv(FuriHalAdcHandle* adc_handle) {
    static float samples[EMF_GPIO_BUFFER_SIZE];

    for(int i = 0; i < EMF_GPIO_BUFFER_SIZE; i++) {
        uint16_t raw = furi_hal_adc_read(adc_handle, EMF_ADC_CHANNEL);
        samples[i] = furi_hal_adc_convert_to_voltage(adc_handle, raw) * 1000.0f;
        furi_delay_us(EMF_GPIO_SAMPLE_PERIOD_US);
    }

    float mag_50 = emf_goertzel_magnitude(
        samples, EMF_GPIO_BUFFER_SIZE, EMF_GPIO_SAMPLE_RATE_HZ, EMF_MAINS_FREQ_A_HZ);
    float mag_60 = emf_goertzel_magnitude(
        samples, EMF_GPIO_BUFFER_SIZE, EMF_GPIO_SAMPLE_RATE_HZ, EMF_MAINS_FREQ_B_HZ);
    return mag_50 > mag_60 ? mag_50 : mag_60;
}

static int emf_level_from_rssi(float rssi_dbm) {
    if(rssi_dbm >= EMF_RF_THRESH_4_DBM) return 4;
    if(rssi_dbm >= EMF_RF_THRESH_3_DBM) return 3;
    if(rssi_dbm >= EMF_RF_THRESH_2_DBM) return 2;
    if(rssi_dbm >= EMF_RF_THRESH_1_DBM) return 1;
    return 0;
}

/* Sets the physical RGB LED to match the active bar level. */
static void emf_update_physical_led(int level) {
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);

    switch(level) {
    case 0:
    case 1:
        furi_hal_light_set(LightGreen, 255);
        break;
    case 2:
        furi_hal_light_set(LightRed, 255);
        furi_hal_light_set(LightGreen, 180);
        break;
    case 3:
        furi_hal_light_set(LightRed, 255);
        furi_hal_light_set(LightGreen, 60);
        break;
    case 4:
    default:
        furi_hal_light_set(LightRed, 255);
        break;
    }
}

static void emf_acquire(EmfSource source, FuriHalAdcHandle** adc_handle) {
    if(source == EmfSourceInternal) {
        furi_hal_subghz_reset();
        furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_ook_650khz_async_regs);
    } else {
        furi_hal_gpio_init(EMF_ADC_GPIO, GpioModeAnalog, GpioPullNo, GpioSpeedVeryHigh);
        *adc_handle = furi_hal_adc_acquire();
        furi_hal_adc_configure(*adc_handle);
    }
}

static void emf_release(EmfSource source, FuriHalAdcHandle** adc_handle) {
    if(source == EmfSourceInternal) {
        furi_hal_subghz_idle();
        furi_hal_subghz_sleep();
    } else {
        furi_hal_gpio_init(EMF_ADC_GPIO, GpioModeAnalog, GpioPullNo, GpioSpeedLow);
        if(*adc_handle) {
            furi_hal_adc_release(*adc_handle);
            *adc_handle = NULL;
        }
    }
    furi_hal_light_set(LightRed, 0);
    furi_hal_light_set(LightGreen, 0);
    furi_hal_light_set(LightBlue, 0);
}

/* Owns the sub-GHz radio AND the ADC exclusively, entirely on this thread -
 * same pattern as Spirit Box / REM Pod, for a consistent, simple lifecycle. */
static int32_t emf_worker(void* context) {
    EmfView* instance = context;
    FuriHalAdcHandle* adc_handle = NULL;

    EmfSource current_source = instance->requested_source;
    emf_acquire(current_source, &adc_handle);

    uint32_t rf_freq = EMF_RF_FREQ_MIN_HZ;

    while(instance->running) {
        EmfSource wanted = instance->requested_source;
        if(wanted != current_source) {
            emf_release(current_source, &adc_handle);
            current_source = wanted;
            emf_acquire(current_source, &adc_handle);
        }

        if(current_source == EmfSourceInternal) {
            rf_freq += EMF_RF_STEP_HZ;
            if(rf_freq > EMF_RF_FREQ_MAX_HZ) rf_freq = EMF_RF_FREQ_MIN_HZ;

            furi_hal_subghz_idle();
            furi_hal_subghz_set_frequency_and_path(rf_freq);
            furi_hal_subghz_rx();
            furi_delay_ms(EMF_RF_SETTLE_MS);
            float rssi = furi_hal_subghz_get_rssi();
            int level = emf_level_from_rssi(rssi);
            emf_update_physical_led(level);
            with_view_model(
                instance->view,
                EmfViewModel * model,
                {
                    model->source = current_source;
                    model->display_value = rssi;
                    model->level = level;
                },
                true);
        } else {
            /* This capture takes ~200ms by itself (EMF_GPIO_BUFFER_SIZE
             * samples at EMF_GPIO_SAMPLE_RATE_HZ), so it already paces this
             * branch - no extra delay needed afterward. */
            float ripple_mv = emf_measure_mains_ripple_mv(adc_handle);
            int level = emf_level_from_ripple_mv(ripple_mv);
            emf_update_physical_led(level);
            with_view_model(
                instance->view,
                EmfViewModel * model,
                {
                    model->source = current_source;
                    model->display_value = ripple_mv;
                    model->level = level;
                },
                true);
            continue;
        }

        furi_delay_ms(EMF_POLL_PERIOD_MS);
    }

    emf_release(current_source, &adc_handle);
    return 0;
}

static void emf_view_draw_callback(Canvas* canvas, void* model) {
    EmfViewModel* m = model;
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    const uint32_t w = canvas_width(canvas); /* 64px in vertical orientation - keep text SHORT */
    const uint32_t h = canvas_height(canvas);

    canvas_draw_str_aligned(canvas, w / 2, 1, AlignCenter, AlignTop, "EMF METER");
    canvas_draw_str_aligned(
        canvas,
        w / 2,
        11,
        AlignCenter,
        AlignTop,
        m->source == EmfSourceInternal ? "<RF>" : "<ADC>");

    /* 5 stacked blocks, bottom = green/normal, top = red/critical.
     * Monochrome display: "lit" = filled box, "unlit" = outline only.
     * The physical RGB LED (see emf_update_physical_led) carries the real color. */
    const int block_count = 5;
    const int top = 24;
    const int bottom = (int)h - 22;
    const int gap = 2;
    const int block_h = (bottom - top - gap * (block_count - 1)) / block_count;
    const int block_w = (int)w - 24;
    const int x = ((int)w - block_w) / 2;

    for(int i = 0; i < block_count; i++) {
        int y = top + i * (block_h + gap);
        int severity = block_count - 1 - i; /* top(i=0) => severity 4, bottom(i=4) => severity 0 */
        bool lit = severity <= m->level;
        if(lit) {
            canvas_draw_box(canvas, x, y, block_w, block_h);
        } else {
            canvas_draw_frame(canvas, x, y, block_w, block_h);
        }
    }

    static const char* level_words[5] = {"GREEN", "GREEN", "YELLOW", "ORANGE", "RED"};
    char val_buf[16];
    if(m->source == EmfSourceInternal) {
        snprintf(val_buf, sizeof(val_buf), "%.0fdBm", (double)m->display_value);
    } else {
        snprintf(val_buf, sizeof(val_buf), "%.1fmV", (double)m->display_value);
    }
    canvas_draw_str_aligned(
        canvas, w / 2, h - 10, AlignCenter, AlignBottom, level_words[m->level]);
    canvas_draw_str_aligned(canvas, w / 2, h - 1, AlignCenter, AlignBottom, val_buf);
}

static bool emf_view_input_callback(InputEvent* event, void* context) {
    EmfView* instance = context;
    if(event->type != InputTypeShort) return false;

    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        instance->requested_source = instance->requested_source == EmfSourceInternal ?
                                         EmfSourceExternal :
                                         EmfSourceInternal;
        return true;
    }
    /* Back is intentionally not consumed: it must bubble up to the app's
     * navigation callback so it returns to the main menu. */
    return false;
}

static void emf_view_enter_callback(void* context) {
    EmfView* instance = context;
    instance->requested_source = EmfSourceInternal;
    with_view_model(
        instance->view,
        EmfViewModel * model,
        {
            model->source = EmfSourceInternal;
            model->display_value = 0.0f;
            model->level = 0;
        },
        true);
    instance->running = true;
    furi_thread_start(instance->thread);
}

static void emf_view_exit_callback(void* context) {
    EmfView* instance = context;
    instance->running = false;
    furi_thread_join(
        instance->thread); /* blocks until the worker releases everything and returns */
}

EmfView* emf_view_alloc(void) {
    EmfView* instance = malloc(sizeof(EmfView));
    instance->view = view_alloc();
    instance->running = false;
    instance->requested_source = EmfSourceInternal;

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(EmfViewModel));
    view_set_context(instance->view, instance);
    view_set_orientation(instance->view, ViewOrientationVertical);
    view_set_draw_callback(instance->view, emf_view_draw_callback);
    view_set_input_callback(instance->view, emf_view_input_callback);
    view_set_enter_callback(instance->view, emf_view_enter_callback);
    view_set_exit_callback(instance->view, emf_view_exit_callback);

    instance->thread =
        furi_thread_alloc_ex("EmfMeterWorker", EMF_WORKER_STACK_SIZE, emf_worker, instance);

    return instance;
}

void emf_view_free(EmfView* instance) {
    furi_thread_free(instance->thread);
    view_free(instance->view);
    free(instance);
}

View* emf_view_get_view(EmfView* instance) {
    return instance->view;
}
