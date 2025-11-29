#include "../led_ring_tester_app.h"

typedef struct {
    uint16_t current_led;
    FuriTimer* timer;
} IndividualTestModel;

static void individual_test_draw_callback(Canvas* canvas, void* context) {
    IndividualTestModel* model = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 15, "Individual LED Test");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 10, 30, "Each LED lights up white");
    canvas_draw_str(canvas, 10, 42, "one at a time");

    char led_info[32];
    snprintf(led_info, sizeof(led_info), "Current LED: %d", model->current_led + 1);
    canvas_draw_str(canvas, 10, 54, led_info);
}

static void individual_test_timer_callback(void* context) {
    LedRingTesterApp* app = context;

    with_view_model(
        app->individual_test_view,
        IndividualTestModel* model,
        {
            // Clear all LEDs
            ws2812b_clear(app->led_strip);

            // Light up current LED in white
            RGB white = rgb_create(255, 255, 255, app->config.brightness);
            ws2812b_set_led(app->led_strip, model->current_led, white);
            ws2812b_update(app->led_strip);

            // Move to next LED
            model->current_led++;
            if(model->current_led >= app->config.led_count) {
                model->current_led = 0;
            }
        },
        true);
}

static bool individual_test_input_callback(InputEvent* event, void* context) {
    LedRingTesterApp* app = context;

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        return false; // Let the scene manager handle back
    }

    return true;
}

void led_ring_tester_scene_on_enter_individual_test(void* context) {
    LedRingTesterApp* app = context;

    // Create view if not exists
    if(!app->individual_test_view) {
        app->individual_test_view = view_alloc();
        view_set_context(app->individual_test_view, app);
        view_allocate_model(app->individual_test_view, ViewModelTypeLocking, sizeof(IndividualTestModel));
        view_set_draw_callback(app->individual_test_view, individual_test_draw_callback);
        view_set_input_callback(app->individual_test_view, individual_test_input_callback);
        view_dispatcher_add_view(
            app->view_dispatcher, LedRingTesterViewIndividualTest, app->individual_test_view);
    }

    // Initialize model
    with_view_model(
        app->individual_test_view,
        IndividualTestModel* model,
        {
            model->current_led = 0;
            model->timer = furi_timer_alloc(individual_test_timer_callback, FuriTimerTypePeriodic, app);
            furi_timer_start(model->timer, 500); // 500ms per LED
        },
        false);

    // Start safety timer
    furi_timer_start(app->safety_timer, 120000); // 2 minutes

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRingTesterViewIndividualTest);
}

bool led_ring_tester_scene_on_event_individual_test(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void led_ring_tester_scene_on_exit_individual_test(void* context) {
    LedRingTesterApp* app = context;

    // Stop safety timer
    furi_timer_stop(app->safety_timer);

    // Stop and free timer
    with_view_model(
        app->individual_test_view,
        IndividualTestModel* model,
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
