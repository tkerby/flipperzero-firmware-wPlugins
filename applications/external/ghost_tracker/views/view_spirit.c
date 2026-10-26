#include "view_spirit.h"

#include <furi.h>
#include <furi_hal.h>
#include <furi_hal_subghz.h>
#include <lib/subghz/devices/cc1101_configs.h>
#include <stdio.h>

/* REAL frequency sweep using the Flipper's actual CC1101 sub-GHz radio -
 * the same chip Radio Scanner uses, and (per Radio Scanner's own source)
 * the SAME technique: furi_hal_subghz_set_async_mirror_pin() mirrors the
 * CC1101's raw demodulated RX bitstream directly onto the speaker's GPIO
 * pin, in hardware, in real time. That's actual RF static playing through
 * the piezo buzzer - not a synthesized tone. We don't touch furi_hal_speaker
 * at all here; the audio path is a totally different, lower-level one.
 *
 * 300.00-348.00MHz is one of the CC1101's hardware-valid receive ranges
 * (furi_hal_subghz_is_frequency_valid). We only RX (never
 * furi_hal_subghz_tx), so there's no transmit-legality concern. */
#define SUBGHZ_FREQ_MIN_HZ  300000000UL
#define SUBGHZ_FREQ_MAX_HZ  348000000UL
#define SUBGHZ_FREQ_STEP_HZ 200000UL /* 200kHz per hop across the ~48MHz band */
#define SUBGHZ_SETTLE_MS    10 /* let the CC1101's AGC settle before trusting RSSI */
#define SUBGHZ_HIT_RSSI_DBM -90.0f /* above this = real RF energy present, not just noise floor */

#define SPIRIT_STEP_MIN_MS 100
#define SPIRIT_STEP_MAX_MS 500
#define SPIRIT_STEP_INC_MS 25

#define SPIRIT_WORKER_STACK_SIZE 2048

typedef struct {
    uint32_t freq_hz; /* real, currently tuned sub-GHz frequency */
    float rssi_dbm;
    bool hit; /* RSSI above threshold: real signal caught, like landing on a station */
    uint32_t step_ms;
    bool muted;
} SpiritViewModel;

struct SpiritView {
    View* view;
    FuriThread* thread;
    volatile bool running;
};

/* Required by furi_hal_subghz_start_async_rx but unused: the actual audio
 * comes from the hardware mirror pin, not from this callback. */
static void spirit_rx_capture_callback(bool level, uint32_t duration, void* context) {
    UNUSED(level);
    UNUSED(duration);
    UNUSED(context);
}

/* Owns the sub-GHz radio exclusively, entirely on this thread. */
static int32_t spirit_worker(void* context) {
    SpiritView* instance = context;

    furi_hal_subghz_reset();
    /* 2FSK/238 preset: same one Radio Scanner uses for audible RX. */
    furi_hal_subghz_load_custom_preset(subghz_device_cc1101_preset_2fsk_dev2_38khz_async_regs);

    uint32_t freq = SUBGHZ_FREQ_MIN_HZ;
    bool rx_active = false;
    /* The mirror pin is the ONLY audio control available with this
     * real-hardware technique: it's a raw digital bitstream, not a PWM
     * amplitude signal, so there's no continuous volume - just pin
     * connected (audible) or disconnected (silent). */
    bool mirror_connected = true;
    furi_hal_subghz_set_async_mirror_pin(&gpio_speaker);

    while(instance->running) {
        freq += SUBGHZ_FREQ_STEP_HZ;
        if(freq > SUBGHZ_FREQ_MAX_HZ) freq = SUBGHZ_FREQ_MIN_HZ;

        if(rx_active) {
            furi_hal_subghz_stop_async_rx();
        }
        furi_hal_subghz_idle();
        uint32_t real_freq = furi_hal_subghz_set_frequency_and_path(freq);
        furi_hal_subghz_start_async_rx(spirit_rx_capture_callback, NULL);
        rx_active = true;

        furi_delay_ms(SUBGHZ_SETTLE_MS);
        float rssi = furi_hal_subghz_get_rssi();
        bool hit = rssi > SUBGHZ_HIT_RSSI_DBM;

        uint32_t step_ms;
        bool muted;
        with_view_model(
            instance->view,
            SpiritViewModel * model,
            {
                model->freq_hz = real_freq;
                model->rssi_dbm = rssi;
                model->hit = hit;
                step_ms = model->step_ms;
                muted = model->muted;
            },
            true);

        bool want_connected = !muted;
        if(want_connected != mirror_connected) {
            /* Reconnecting/disconnecting the mirror is independent of the
             * ongoing async RX capture - toggling it doesn't disturb the
             * sweep or the RSSI readings at all. */
            furi_hal_subghz_set_async_mirror_pin(want_connected ? &gpio_speaker : NULL);
            mirror_connected = want_connected;
        }

        furi_hal_light_set(LightRed, hit ? 255 : 0);

        /* The static keeps playing live via the hardware mirror for the
         * rest of this dwell step - no CPU work needed meanwhile. */
        uint32_t remaining_ms = step_ms > SUBGHZ_SETTLE_MS ? step_ms - SUBGHZ_SETTLE_MS : 0;
        furi_delay_ms(remaining_ms);
    }

    if(rx_active) {
        furi_hal_subghz_stop_async_rx();
    }
    furi_hal_light_set(LightRed, 0);
    furi_hal_subghz_idle();
    furi_hal_subghz_set_async_mirror_pin(NULL);
    furi_hal_subghz_sleep();
    return 0;
}

static void spirit_view_draw_callback(Canvas* canvas, void* model) {
    SpiritViewModel* m = model;
    canvas_clear(canvas);

    const uint32_t w = canvas_width(canvas); /* 64px in vertical orientation - keep text SHORT */
    const uint32_t h = canvas_height(canvas);

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, w / 2, 1, AlignCenter, AlignTop, "SPIRIT BOX");
    canvas_draw_str_aligned(canvas, w / 2, 11, AlignCenter, AlignTop, "RF SWEEP");

    char freq_buf[16];
    snprintf(
        freq_buf, sizeof(freq_buf), "%lu.%02lu", m->freq_hz / 1000000, (m->freq_hz / 10000) % 100);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, w / 2, h / 2 - 14, AlignCenter, AlignCenter, freq_buf);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, w / 2, h / 2, AlignCenter, AlignCenter, "MHz");

    /* Dial: a vertical-safe horizontal bar spanning the swept band, with a
     * tick mark at the current frequency's position. */
    int dial_y = h / 2 + 16;
    int dial_w = (int)w - 12;
    int dial_x = 6;
    canvas_draw_line(canvas, dial_x, dial_y, dial_x + dial_w, dial_y);
    float pos = (float)(m->freq_hz - SUBGHZ_FREQ_MIN_HZ) /
                (float)(SUBGHZ_FREQ_MAX_HZ - SUBGHZ_FREQ_MIN_HZ);
    int tick_x = dial_x + (int)(pos * (float)dial_w);
    canvas_draw_box(canvas, tick_x - 1, dial_y - 4, 3, 8);

    char rssi_buf[12];
    snprintf(rssi_buf, sizeof(rssi_buf), "%.0fdBm", (double)m->rssi_dbm);
    int rssi_y = dial_y + 10;
    if(m->hit) {
        /* Inverted pill behind the text instead of extra words: stays
         * compact on the 64px-wide vertical canvas. */
        canvas_draw_box(canvas, dial_x, rssi_y - 1, dial_w, 10);
        canvas_set_color(canvas, ColorWhite);
    }
    canvas_draw_str_aligned(canvas, w / 2, rssi_y, AlignCenter, AlignTop, rssi_buf);
    canvas_set_color(canvas, ColorBlack);

    char step_buf[16];
    snprintf(step_buf, sizeof(step_buf), "%lums %s", m->step_ms, m->muted ? "MUTE" : "");
    canvas_draw_str_aligned(canvas, w / 2, h - 17, AlignCenter, AlignBottom, "+- STEP");
    canvas_draw_str_aligned(canvas, w / 2, h - 9, AlignCenter, AlignBottom, step_buf);
    canvas_draw_str_aligned(canvas, w / 2, h - 1, AlignCenter, AlignBottom, "OK: MUTE");
}

static bool spirit_view_input_callback(InputEvent* event, void* context) {
    SpiritView* instance = context;
    if(event->type != InputTypeShort && event->type != InputTypeRepeat) return false;

    if(event->key == InputKeyUp || event->key == InputKeyDown) {
        with_view_model(
            instance->view,
            SpiritViewModel * model,
            {
                int32_t step = (int32_t)model->step_ms;
                step += (event->key == InputKeyUp) ? SPIRIT_STEP_INC_MS : -SPIRIT_STEP_INC_MS;
                if(step < SPIRIT_STEP_MIN_MS) step = SPIRIT_STEP_MIN_MS;
                if(step > SPIRIT_STEP_MAX_MS) step = SPIRIT_STEP_MAX_MS;
                model->step_ms = (uint32_t)step;
            },
            true);
        /* The worker thread re-reads step_ms every loop iteration, so the
         * new value takes effect on the next hop. */
        return true;
    }

    if(event->key == InputKeyOk) {
        if(event->type != InputTypeShort) return true; /* swallow repeat, avoid flip-flopping */
        with_view_model(
            instance->view, SpiritViewModel * model, { model->muted = !model->muted; }, true);
        return true;
    }

    return false;
}

static void spirit_view_enter_callback(void* context) {
    SpiritView* instance = context;

    uint32_t step_ms = SPIRIT_STEP_MIN_MS + ((SPIRIT_STEP_MAX_MS - SPIRIT_STEP_MIN_MS) / 2);
    with_view_model(
        instance->view,
        SpiritViewModel * model,
        {
            model->freq_hz = SUBGHZ_FREQ_MIN_HZ;
            model->rssi_dbm = -120.0f;
            model->hit = false;
            model->step_ms = step_ms;
            model->muted = false;
        },
        true);

    instance->running = true;
    furi_thread_start(instance->thread);
}

static void spirit_view_exit_callback(void* context) {
    SpiritView* instance = context;
    instance->running = false;
    furi_thread_join(
        instance->thread); /* blocks until the worker releases everything and returns */
}

SpiritView* spirit_view_alloc(void) {
    SpiritView* instance = malloc(sizeof(SpiritView));
    instance->view = view_alloc();
    instance->running = false;

    view_allocate_model(instance->view, ViewModelTypeLocking, sizeof(SpiritViewModel));
    view_set_context(instance->view, instance);
    view_set_orientation(instance->view, ViewOrientationVertical);
    view_set_draw_callback(instance->view, spirit_view_draw_callback);
    view_set_input_callback(instance->view, spirit_view_input_callback);
    view_set_enter_callback(instance->view, spirit_view_enter_callback);
    view_set_exit_callback(instance->view, spirit_view_exit_callback);

    instance->thread =
        furi_thread_alloc_ex("SpiritBoxWorker", SPIRIT_WORKER_STACK_SIZE, spirit_worker, instance);

    return instance;
}

void spirit_view_free(SpiritView* instance) {
    furi_thread_free(instance->thread);
    view_free(instance->view);
    free(instance);
}

View* spirit_view_get_view(SpiritView* instance) {
    return instance->view;
}
