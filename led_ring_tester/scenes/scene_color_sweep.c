#include "../led_ring_tester_app.h"

typedef struct {
    uint16_t hue_offset;
    FuriTimer* timer;
} ColorSweepModel;

static void color_sweep_draw_callback(Canvas* canvas, void* context) {
    UNUSED(context);

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 15, "Color Sweep Test");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 10, 30, "Rainbow animation");
    canvas_draw_str(canvas, 10, 42, "cycling through all");
    canvas_draw_str(canvas, 10, 54, "colors continuously");
}

static void color_sweep_timer_callback(void* context) {
    LedRingTesterApp* app = context;

    with_view_model(
        app->color_sweep_view,
        ColorSweepModel* model,
        {
            // Update each LED with rainbow colors
            for(uint16_t i = 0; i < app->config.led_count; i++) {
                uint16_t hue = (model->hue_offset + (i * 360 / app->config.led_count)) % 360;
                RGB color = rgb_from_hsv(hue, 255, app->config.brightness);
                ws2812b_set_led(app->led_strip, i, color);
            }
            ws2812b_update(app->led_strip);

            // Increment hue offset for animation
            model->hue_offset = (model->hue_offset + 5) % 360;
        },
        false);
}

static bool color_sweep_input_callback(InputEvent* event, void* context) {
    UNUSED(context);

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        return false; // Let the scene manager handle back
    }

    return true;
}

void led_ring_tester_scene_on_enter_color_sweep(void* context) {
    LedRingTesterApp* app = context;

    // Create view if not exists
    if(!app->color_sweep_view) {
        app->color_sweep_view = view_alloc();
        view_set_context(app->color_sweep_view, app);
        view_allocate_model(app->color_sweep_view, ViewModelTypeLocking, sizeof(ColorSweepModel));
        view_set_draw_callback(app->color_sweep_view, color_sweep_draw_callback);
        view_set_input_callback(app->color_sweep_view, color_sweep_input_callback);
        view_dispatcher_add_view(
            app->view_dispatcher, LedRingTesterViewColorSweep, app->color_sweep_view);
    }

    // Initialize model
    with_view_model(
        app->color_sweep_view,
        ColorSweepModel* model,
        {
            model->hue_offset = 0;
            model->timer = furi_timer_alloc(color_sweep_timer_callback, FuriTimerTypePeriodic, app);
            furi_timer_start(model->timer, 50); // 50ms update rate
        },
        false);

    // Start safety timer
    furi_timer_start(app->safety_timer, 120000); // 2 minutes

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRingTesterViewColorSweep);
}

bool led_ring_tester_scene_on_event_color_sweep(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void led_ring_tester_scene_on_exit_color_sweep(void* context) {
    LedRingTesterApp* app = context;

    // Stop safety timer
    furi_timer_stop(app->safety_timer);

    // Stop and free timer
    with_view_model(
        app->color_sweep_view,
        ColorSweepModel* model,
        {
            if(model->timer) {
                furi_timer_stop(model->timer);
                furi_timer_free(model->timer);
                model->timer = NULL;
            }
        },
        false);

    // Turn off all LEDs
    ws2812b_clear(app->led_strip);
    ws2812b_update(app->led_strip);
}
