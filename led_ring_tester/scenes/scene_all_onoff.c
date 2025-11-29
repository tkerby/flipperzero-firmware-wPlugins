#include "../led_ring_tester_app.h"

typedef enum {
    ColorStateRed,
    ColorStateGreen,
    ColorStateBlue,
    ColorStateWhite,
    ColorStateOff,
    ColorStateCount,
} ColorState;

typedef struct {
    ColorState current_color;
    FuriTimer* timer;
} AllOnOffModel;

static const char* color_names[] = {
    "Red",
    "Green",
    "Blue",
    "White",
    "Off",
};

static void all_onoff_draw_callback(Canvas* canvas, void* context) {
    AllOnOffModel* model = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 15, "All On/Off Test");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 10, 30, "All LEDs cycle through:");
    canvas_draw_str(canvas, 10, 42, "Red -> Green -> Blue");
    canvas_draw_str(canvas, 10, 54, "-> White -> Off");

    char color_info[32];
    snprintf(color_info, sizeof(color_info), "Current: %s", color_names[model->current_color]);
    canvas_draw_str(canvas, 10, 64, color_info);
}

static void all_onoff_timer_callback(void* context) {
    LedRingTesterApp* app = context;

    with_view_model(
        app->all_onoff_view,
        AllOnOffModel* model,
        {
            RGB color = {0, 0, 0};

            switch(model->current_color) {
                case ColorStateRed:
                    color = rgb_create(255, 0, 0, app->config.brightness);
                    break;
                case ColorStateGreen:
                    color = rgb_create(0, 255, 0, app->config.brightness);
                    break;
                case ColorStateBlue:
                    color = rgb_create(0, 0, 255, app->config.brightness);
                    break;
                case ColorStateWhite:
                    color = rgb_create(255, 255, 255, app->config.brightness);
                    break;
                case ColorStateOff:
                    color = rgb_create(0, 0, 0, 255); // Off
                    break;
                default:
                    break;
            }

            ws2812b_set_all(app->led_strip, color);
            ws2812b_update(app->led_strip);

            // Move to next color
            model->current_color = (model->current_color + 1) % ColorStateCount;
        },
        true);
}

static bool all_onoff_input_callback(InputEvent* event, void* context) {
    UNUSED(context);

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        return false; // Let the scene manager handle back
    }

    return true;
}

void led_ring_tester_scene_on_enter_all_onoff(void* context) {
    LedRingTesterApp* app = context;

    // Create view if not exists
    if(!app->all_onoff_view) {
        app->all_onoff_view = view_alloc();
        view_set_context(app->all_onoff_view, app);
        view_allocate_model(app->all_onoff_view, ViewModelTypeLocking, sizeof(AllOnOffModel));
        view_set_draw_callback(app->all_onoff_view, all_onoff_draw_callback);
        view_set_input_callback(app->all_onoff_view, all_onoff_input_callback);
        view_dispatcher_add_view(
            app->view_dispatcher, LedRingTesterViewAllOnOff, app->all_onoff_view);
    }

    // Initialize model
    with_view_model(
        app->all_onoff_view,
        AllOnOffModel* model,
        {
            model->current_color = ColorStateRed;
            model->timer = furi_timer_alloc(all_onoff_timer_callback, FuriTimerTypePeriodic, app);
            furi_timer_start(model->timer, 1000); // 1 second per color
        },
        false);

    // Start safety timer
    furi_timer_start(app->safety_timer, 120000); // 2 minutes

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRingTesterViewAllOnOff);
}

bool led_ring_tester_scene_on_event_all_onoff(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void led_ring_tester_scene_on_exit_all_onoff(void* context) {
    LedRingTesterApp* app = context;

    // Stop safety timer
    furi_timer_stop(app->safety_timer);

    // Stop and free timer
    with_view_model(
        app->all_onoff_view,
        AllOnOffModel* model,
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
