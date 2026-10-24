#include "view_splash.h"
#include "ghost_icon.h"

#include <furi.h>

#define SPLASH_TIMEOUT_MS 2000

struct SplashView {
    View* view;
    FuriTimer* timer;
    volatile bool done_sent;
    SplashDoneCallback done_callback;
    void* done_callback_context;
};

static void splash_signal_done(SplashView* instance) {
    /* Guards against both the timer and a key press firing in short
     * succession; the callback itself is expected to be idempotent too,
     * but there's no reason to call it twice on purpose. */
    if(instance->done_sent) return;
    instance->done_sent = true;
    if(instance->done_callback) {
        instance->done_callback(instance->done_callback_context);
    }
}

/* Runs on the timer service thread, not the GUI thread - the done callback
 * (set by the app) must only do thread-safe things from here, i.e. post a
 * custom event via view_dispatcher_send_custom_event rather than call
 * view_dispatcher_switch_to_view directly. */
static void splash_timer_callback(void* context) {
    SplashView* instance = context;
    splash_signal_done(instance);
}

static void splash_view_draw_callback(Canvas* canvas, void* model) {
    UNUSED(model);
    canvas_clear(canvas);

    const uint32_t w = canvas_width(canvas); /* 64px in vertical orientation - keep text SHORT */
    const uint32_t h = canvas_height(canvas);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, w / 2, 14, AlignCenter, AlignTop, "GHOST");
    canvas_draw_str_aligned(canvas, w / 2, 26, AlignCenter, AlignTop, "TRACKER");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, w / 2, 42, AlignCenter, AlignTop, "by Zinetick");

    ghost_icon_draw(canvas, (int)w / 2, (int)h - 44, 30, 32);
}

static bool splash_view_input_callback(InputEvent* event, void* context) {
    SplashView* instance = context;
    UNUSED(event);
    /* Any key dismisses the splash immediately - runs on the GUI thread,
     * same safety requirement as the timer callback applies here too. */
    splash_signal_done(instance);
    return true;
}

static void splash_view_enter_callback(void* context) {
    SplashView* instance = context;
    instance->done_sent = false;
    furi_timer_start(instance->timer, furi_ms_to_ticks(SPLASH_TIMEOUT_MS));
}

static void splash_view_exit_callback(void* context) {
    SplashView* instance = context;
    furi_timer_stop(instance->timer);
}

void splash_view_set_done_callback(
    SplashView* instance,
    SplashDoneCallback callback,
    void* context) {
    instance->done_callback = callback;
    instance->done_callback_context = context;
}

SplashView* splash_view_alloc(void) {
    SplashView* instance = malloc(sizeof(SplashView));
    instance->view = view_alloc();
    instance->done_sent = false;
    instance->done_callback = NULL;
    instance->done_callback_context = NULL;
    instance->timer = furi_timer_alloc(splash_timer_callback, FuriTimerTypeOnce, instance);

    view_set_context(instance->view, instance);
    view_set_orientation(instance->view, ViewOrientationVertical);
    view_set_draw_callback(instance->view, splash_view_draw_callback);
    view_set_input_callback(instance->view, splash_view_input_callback);
    view_set_enter_callback(instance->view, splash_view_enter_callback);
    view_set_exit_callback(instance->view, splash_view_exit_callback);

    return instance;
}

void splash_view_free(SplashView* instance) {
    furi_timer_free(instance->timer);
    view_free(instance->view);
    free(instance);
}

View* splash_view_get_view(SplashView* instance) {
    return instance->view;
}
