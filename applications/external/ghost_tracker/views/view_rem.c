#include "view_rem.h"

#include <furi.h>
#include <furi_hal.h>
#include <stdio.h>

#define REM_POLL_PERIOD_MS 40
#define REM_SAMPLE_COUNT   24 /* raw back-to-back ADC reads per tick, for peak-to-peak jitter */

/* Antenna wiring: physical pin 15 / PC1. STM32WB55 maps PC1 -> ADC channel 2. */
#define REM_ADC_GPIO    (&gpio_ext_pc1)
#define REM_ADC_CHANNEL FuriHalAdcChannel2

#define REM_SENS_MIN     0
#define REM_SENS_MAX     100
#define REM_SENS_DEFAULT 50
#define REM_SENS_STEP    10

/* Raw ADC-count peak-to-peak jitter needed to trigger an alert.
 * Tune these to your actual antenna length/environment.
 *  - At sensitivity 100 (most sensitive): REM_PP_AT_MAX_SENS counts trigger it
 *    (reacts from several cm away).
 *  - At sensitivity 0 (least sensitive): REM_PP_AT_MIN_SENS counts needed
 *    (object has to be a few mm from the antenna). */
#define REM_PP_AT_MAX_SENS 15.0f
#define REM_PP_AT_MIN_SENS 250.0f

#define REM_SPEAKER_ACQUIRE_TIMEOUT_MS 1000
#define REM_ALERT_TONE_HZ              2800.0f
#define REM_BLINK_ON_MS                80
#define REM_BLINK_PERIOD_MS            160
#define REM_WORKER_STACK_SIZE          1024

#define REM_VOL_MIN     0
#define REM_VOL_MAX     100
#define REM_VOL_DEFAULT 80
#define REM_VOL_STEP    10

typedef struct {
    int sensitivity_pct;
    int volume_pct;
    bool alert;
    uint16_t last_peak_to_peak;
    bool speaker_busy;
} RemViewModel;

struct RemView {
    View* view;
    FuriThread* thread;
    volatile bool running;
};

static float rem_threshold_for_sensitivity(int sensitivity_pct) {
    float t = (float)sensitivity_pct / 100.0f;
    return REM_PP_AT_MIN_SENS - t * (REM_PP_AT_MIN_SENS - REM_PP_AT_MAX_SENS);
}

/* Owns the ADC pin AND the speaker exclusively, entirely on this thread.
 * furi_hal_speaker tracks ownership per calling thread: acquiring it here
 * and also starting/stopping tones here (instead of from a FuriTimer
 * callback, which runs on a different system thread) avoids a furi_check
 * crash. The ADC and the RGB LED blink helper aren't thread-restricted,
 * but keeping everything in one worker keeps the lifecycle simple. */
static int32_t rem_worker(void* context) {
    RemView* instance = context;

    furi_hal_gpio_init(REM_ADC_GPIO, GpioModeAnalog, GpioPullNo, GpioSpeedVeryHigh);
    FuriHalAdcHandle* adc_handle = furi_hal_adc_acquire();
    furi_hal_adc_configure_ex(
        adc_handle,
        FuriHalAdcScale2048,
        FuriHalAdcClockSync64,
        FuriHalAdcOversampleNone, /* no averaging: we WANT the raw jitter */
        FuriHalAdcSamplingtime2_5); /* fastest sampling: catches transient coupling */

    bool speaker_ok = furi_hal_speaker_acquire(REM_SPEAKER_ACQUIRE_TIMEOUT_MS);
    with_view_model(
        instance->view, RemViewModel * model, { model->speaker_busy = !speaker_ok; }, true);

    bool alert_active = false;

    while(instance->running) {
        uint16_t min_v = 0xFFFF;
        uint16_t max_v = 0;
        for(int i = 0; i < REM_SAMPLE_COUNT; i++) {
            uint16_t v = furi_hal_adc_read(adc_handle, REM_ADC_CHANNEL);
            if(v < min_v) min_v = v;
            if(v > max_v) max_v = v;
        }
        uint16_t peak_to_peak = max_v - min_v;

        int sensitivity_pct;
        int volume_pct;
        with_view_model(
            instance->view,
            RemViewModel * model,
            {
                sensitivity_pct = model->sensitivity_pct;
                volume_pct = model->volume_pct;
            },
            false);

        float threshold = rem_threshold_for_sensitivity(sensitivity_pct);
        bool now_alert = (float)peak_to_peak >= threshold;

        with_view_model(
            instance->view,
            RemViewModel * model,
            {
                model->alert = now_alert;
                model->last_peak_to_peak = peak_to_peak;
            },
            true);

        if(now_alert && !alert_active) {
            furi_hal_light_blink_start(LightRed, 255, REM_BLINK_ON_MS, REM_BLINK_PERIOD_MS);
            if(speaker_ok) furi_hal_speaker_start(REM_ALERT_TONE_HZ, (float)volume_pct / 100.0f);
        } else if(now_alert && alert_active) {
            /* Live volume updates while the alert is already sounding - safe
             * here since this same thread owns the speaker. */
            if(speaker_ok && furi_hal_speaker_is_mine()) {
                furi_hal_speaker_set_volume((float)volume_pct / 100.0f);
            }
        } else if(!now_alert && alert_active) {
            furi_hal_light_blink_stop();
            if(speaker_ok && furi_hal_speaker_is_mine()) furi_hal_speaker_stop();
        }
        alert_active = now_alert;

        furi_delay_ms(REM_POLL_PERIOD_MS);
    }

    if(alert_active) {
        furi_hal_light_blink_stop();
        if(speaker_ok && furi_hal_speaker_is_mine()) furi_hal_speaker_stop();
    }
    if(speaker_ok) furi_hal_speaker_release();

    furi_hal_adc_release(adc_handle);
    furi_hal_gpio_init(REM_ADC_GPIO, GpioModeAnalog, GpioPullNo, GpioSpeedLow);

    return 0;
}

static void rem_view_draw_callback(Canvas* canvas, void* model) {
    RemViewModel* m = model;
    const uint32_t w = canvas_width(canvas);
    const uint32_t h = canvas_height(canvas);

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);

    if(m->alert) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(canvas, w / 2, h / 2 - 10, AlignCenter, AlignCenter, "PROXIMITY");
        canvas_draw_str_aligned(canvas, w / 2, h / 2 + 6, AlignCenter, AlignCenter, "ALERT!");
    } else if(m->speaker_busy) {
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, w / 2, h / 2 - 8, AlignCenter, AlignCenter, "STATUS: CLEAR");
        canvas_draw_str_aligned(
            canvas, w / 2, h / 2 + 8, AlignCenter, AlignCenter, "(speaker busy)");
    } else {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str_aligned(
            canvas, w / 2, h / 2 - 8, AlignCenter, AlignCenter, "STATUS: CLEAR");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str_aligned(
            canvas, w / 2, h / 2 + 10, AlignCenter, AlignCenter, "MONITORING...");
    }

    canvas_set_font(canvas, FontSecondary);
    char sens_buf[20];
    char vol_buf[20];
    snprintf(sens_buf, sizeof(sens_buf), "<> Sens: %d%%", m->sensitivity_pct);
    snprintf(vol_buf, sizeof(vol_buf), "+- Vol: %d%%", m->volume_pct);
    canvas_draw_str_aligned(canvas, w / 2, h - 10, AlignCenter, AlignBottom, sens_buf);
    canvas_draw_str_aligned(canvas, w / 2, h - 1, AlignCenter, AlignBottom, vol_buf);
}

static bool rem_view_input_callback(InputEvent* event, void* context) {
    RemView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(event->key == InputKeyLeft || event->key == InputKeyRight) {
        with_view_model(
            instance->view,
            RemViewModel * model,
            {
                int sens = model->sensitivity_pct;
                sens += (event->key == InputKeyRight) ? REM_SENS_STEP : -REM_SENS_STEP;
                if(sens < REM_SENS_MIN) sens = REM_SENS_MIN;
                if(sens > REM_SENS_MAX) sens = REM_SENS_MAX;
                model->sensitivity_pct = sens;
            },
            true);
        return true;
    }

    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        with_view_model(
            instance->view,
            RemViewModel * model,
            {
                int vol = model->volume_pct;
                vol += (event->key == InputKeyUp) ? REM_VOL_STEP : -REM_VOL_STEP;
                if(vol < REM_VOL_MIN) vol = REM_VOL_MIN;
                if(vol > REM_VOL_MAX) vol = REM_VOL_MAX;
                model->volume_pct = vol;
            },
            true);
        return true;
    }

    return false;
}

static void rem_view_enter_callback(void* context) {
    RemView* instance = context;

    with_view_model(
        instance->view,
        RemViewModel * model,
        {
            model->sensitivity_pct = REM_SENS_DEFAULT;
            model->volume_pct = REM_VOL_DEFAULT;
            model->alert = false;
            model->last_peak_to_peak = 0;
            model->speaker_busy = false;
        },
        true);

    instance->running = true;
    furi_thread_start(instance->thread);
}

static void rem_view_exit_callback(void* context) {
    RemView* instance = context;
    instance->running = false;
    furi_thread_join(
        instance->thread); /* blocks until the worker releases ADC+speaker and returns */
}

RemView* rem_view_alloc(void) {
    RemView* instance = malloc(sizeof(RemView));
    instance->view = view_alloc();
    instance->running = false;

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(RemViewModel));
    view_set_context(instance->view, instance);
    view_set_orientation(instance->view, ViewOrientationHorizontal);
    view_set_draw_callback(instance->view, rem_view_draw_callback);
    view_set_input_callback(instance->view, rem_view_input_callback);
    view_set_enter_callback(instance->view, rem_view_enter_callback);
    view_set_exit_callback(instance->view, rem_view_exit_callback);

    instance->thread =
        furi_thread_alloc_ex("RemPodWorker", REM_WORKER_STACK_SIZE, rem_worker, instance);

    return instance;
}

void rem_view_free(RemView* instance) {
    furi_thread_free(instance->thread);
    view_free(instance->view);
    free(instance);
}

View* rem_view_get_view(RemView* instance) {
    return instance->view;
}
