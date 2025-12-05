#include "../led_ring_tester_app.h"

typedef struct {
    uint8_t brightness;
    bool fading_in;
    FuriTimer* timer;
} BrightnessModel;

static void brightness_draw_callback(Canvas* canvas, void* context) {
    BrightnessModel* model = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 15, "Brightness Test");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 10, 30, "All LEDs fade in/out");
    canvas_draw_str(canvas, 10, 42, "continuously in white");

    char brightness_info[32];
    snprintf(brightness_info, sizeof(brightness_info), "Brightness: %d%%", (model->brightness * 100) / 255);
    canvas_draw_str(canvas, 10, 54, brightness_info);

    char direction[16];
    snprintf(direction, sizeof(direction), "Mode: %s", model->fading_in ? "Fading In" : "Fading Out");
    canvas_draw_str(canvas, 10, 64, direction);
}

static void brightness_timer_callback(void* context) {
    LedRingTesterApp* app = context;

    with_view_model(
        app->brightness_view,
        BrightnessModel* model,
        {
            // Update brightness
            if(model->fading_in) {
                if(model->brightness < 250) {
                    model->brightness += 5;
                } else {
                    model->brightness = 255;
                    model->fading_in = false;
                }
            } else {
                if(model->brightness > 5) {
                    model->brightness -= 5;
                } else {
                    model->brightness = 0;
                    model->fading_in = true;
                }
            }

            // Set all LEDs to white with current brightness
            RGB white = rgb_create(255, 255, 255, model->brightness);
            ws2812b_set_all(app->led_strip, white);
            ws2812b_update(app->led_strip);
        },
        true);
}

static bool brightness_input_callback(InputEvent* event, void* context) {
    UNUSED(context);

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        return false; // Let the scene manager handle back
    }

    return true;
}

void led_ring_tester_scene_on_enter_brightness(void* context) {
    LedRingTesterApp* app = context;

    // Create view if not exists
    if(!app->brightness_view) {
        app->brightness_view = view_alloc();
        view_set_context(app->brightness_view, app);
        view_allocate_model(app->brightness_view, ViewModelTypeLocking, sizeof(BrightnessModel));
        view_set_draw_callback(app->brightness_view, brightness_draw_callback);
        view_set_input_callback(app->brightness_view, brightness_input_callback);
        view_dispatcher_add_view(
            app->view_dispatcher, LedRingTesterViewBrightness, app->brightness_view);
    }

    // Initialize model
    with_view_model(
        app->brightness_view,
        BrightnessModel* model,
        {
            model->brightness = 0;
            model->fading_in = true;
            model->timer = furi_timer_alloc(brightness_timer_callback, FuriTimerTypePeriodic, app);
            furi_timer_start(model->timer, 30); // 30ms update rate for smooth fading
        },
        false);

    // Start safety timer
    furi_timer_start(app->safety_timer, 120000); // 2 minutes

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRingTesterViewBrightness);
}

bool led_ring_tester_scene_on_event_brightness(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void led_ring_tester_scene_on_exit_brightness(void* context) {
    LedRingTesterApp* app = context;

    // Stop safety timer
    furi_timer_stop(app->safety_timer);

    // Stop and free timer
    with_view_model(
        app->brightness_view,
        BrightnessModel* model,
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
