#include "../led_ring_tester_app.h"

typedef enum {
    QuickTestStateIndividual,
    QuickTestStateRainbow,
    QuickTestStateRed,
    QuickTestStateGreen,
    QuickTestStateBlue,
    QuickTestStateWhite,
    QuickTestStateBrightness,
    QuickTestStateComplete,
} QuickTestState;

typedef struct {
    QuickTestState state;
    uint32_t step_counter;
    uint16_t current_led;
    uint16_t hue_offset;
    uint8_t brightness;
    bool fading_in;
    FuriTimer* timer;
} QuickTestModel;

static const char* state_names[] = {
    "Individual Test",
    "Rainbow Sweep",
    "All Red",
    "All Green",
    "All Blue",
    "All White",
    "Brightness Fade",
    "Test Complete!",
};

static void quick_test_draw_callback(Canvas* canvas, void* context) {
    QuickTestModel* model = context;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 10, 15, "Quick Test");

    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 10, 30, "Running all tests");
    canvas_draw_str(canvas, 10, 42, "automatically...");

    char state_info[32];
    snprintf(state_info, sizeof(state_info), "Current: %s", state_names[model->state]);
    canvas_draw_str(canvas, 10, 54, state_info);

    if(model->state == QuickTestStateComplete) {
        canvas_draw_str(canvas, 10, 64, "Press Back to exit");
    }
}

static void quick_test_timer_callback(void* context) {
    LedRingTesterApp* app = context;

    with_view_model(
        app->quick_test_view,
        QuickTestModel* model,
        {
            model->step_counter++;

            switch(model->state) {
                case QuickTestStateIndividual:
                    // Individual LED test - 3 seconds (60 steps at 50ms)
                    ws2812b_clear(app->led_strip);
                    RGB white = rgb_create(255, 255, 255, app->config.brightness);
                    ws2812b_set_led(app->led_strip, model->current_led, white);
                    ws2812b_update(app->led_strip);
                    model->current_led = (model->current_led + 1) % app->config.led_count;
                    if(model->step_counter >= 60) {
                        model->state = QuickTestStateRainbow;
                        model->step_counter = 0;
                    }
                    break;

                case QuickTestStateRainbow:
                    // Rainbow sweep - 3 seconds (60 steps)
                    for(uint16_t i = 0; i < app->config.led_count; i++) {
                        uint16_t hue = (model->hue_offset + (i * 360 / app->config.led_count)) % 360;
                        RGB color = rgb_from_hsv(hue, 255, app->config.brightness);
                        ws2812b_set_led(app->led_strip, i, color);
                    }
                    ws2812b_update(app->led_strip);
                    model->hue_offset = (model->hue_offset + 10) % 360;
                    if(model->step_counter >= 60) {
                        model->state = QuickTestStateRed;
                        model->step_counter = 0;
                    }
                    break;

                case QuickTestStateRed:
                    // All red - 1 second (20 steps)
                    {
                        RGB red = rgb_create(255, 0, 0, app->config.brightness);
                        ws2812b_set_all(app->led_strip, red);
                        ws2812b_update(app->led_strip);
                        if(model->step_counter >= 20) {
                            model->state = QuickTestStateGreen;
                            model->step_counter = 0;
                        }
                    }
                    break;

                case QuickTestStateGreen:
                    // All green - 1 second
                    {
                        RGB green = rgb_create(0, 255, 0, app->config.brightness);
                        ws2812b_set_all(app->led_strip, green);
                        ws2812b_update(app->led_strip);
                        if(model->step_counter >= 20) {
                            model->state = QuickTestStateBlue;
                            model->step_counter = 0;
                        }
                    }
                    break;

                case QuickTestStateBlue:
                    // All blue - 1 second
                    {
                        RGB blue = rgb_create(0, 0, 255, app->config.brightness);
                        ws2812b_set_all(app->led_strip, blue);
                        ws2812b_update(app->led_strip);
                        if(model->step_counter >= 20) {
                            model->state = QuickTestStateWhite;
                            model->step_counter = 0;
                        }
                    }
                    break;

                case QuickTestStateWhite:
                    // All white - 1 second
                    {
                        RGB white_color = rgb_create(255, 255, 255, app->config.brightness);
                        ws2812b_set_all(app->led_strip, white_color);
                        ws2812b_update(app->led_strip);
                        if(model->step_counter >= 20) {
                            model->state = QuickTestStateBrightness;
                            model->step_counter = 0;
                            model->brightness = 0;
                            model->fading_in = true;
                        }
                    }
                    break;

                case QuickTestStateBrightness:
                    // Brightness fade - 3 seconds (60 steps)
                    {
                        if(model->fading_in) {
                            model->brightness += 8;
                            if(model->brightness >= 248) {
                                model->brightness = 255;
                                model->fading_in = false;
                            }
                        } else {
                            if(model->brightness >= 8) {
                                model->brightness -= 8;
                            } else {
                                model->brightness = 0;
                            }
                        }
                        RGB fade_white = rgb_create(255, 255, 255, model->brightness);
                        ws2812b_set_all(app->led_strip, fade_white);
                        ws2812b_update(app->led_strip);
                        if(model->step_counter >= 60) {
                            model->state = QuickTestStateComplete;
                            model->step_counter = 0;
                            ws2812b_clear(app->led_strip);
                            ws2812b_update(app->led_strip);
                        }
                    }
                    break;

                case QuickTestStateComplete:
                    // Test complete - turn off LEDs
                    ws2812b_clear(app->led_strip);
                    ws2812b_update(app->led_strip);
                    break;
            }
        },
        true);
}

static bool quick_test_input_callback(InputEvent* event, void* context) {
    UNUSED(context);

    if(event->type == InputTypeShort && event->key == InputKeyBack) {
        return false; // Let the scene manager handle back
    }

    return true;
}

void led_ring_tester_scene_on_enter_quick_test(void* context) {
    LedRingTesterApp* app = context;

    // Create view if not exists
    if(!app->quick_test_view) {
        app->quick_test_view = view_alloc();
        view_set_context(app->quick_test_view, app);
        view_allocate_model(app->quick_test_view, ViewModelTypeLocking, sizeof(QuickTestModel));
        view_set_draw_callback(app->quick_test_view, quick_test_draw_callback);
        view_set_input_callback(app->quick_test_view, quick_test_input_callback);
        view_dispatcher_add_view(
            app->view_dispatcher, LedRingTesterViewQuickTest, app->quick_test_view);
    }

    // Initialize model
    with_view_model(
        app->quick_test_view,
        QuickTestModel* model,
        {
            model->state = QuickTestStateIndividual;
            model->step_counter = 0;
            model->current_led = 0;
            model->hue_offset = 0;
            model->brightness = 0;
            model->fading_in = true;
            model->timer = furi_timer_alloc(quick_test_timer_callback, FuriTimerTypePeriodic, app);
            furi_timer_start(model->timer, 50); // 50ms update rate
        },
        false);

    // Start safety timer
    furi_timer_start(app->safety_timer, 120000); // 2 minutes

    view_dispatcher_switch_to_view(app->view_dispatcher, LedRingTesterViewQuickTest);
}

bool led_ring_tester_scene_on_event_quick_test(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void led_ring_tester_scene_on_exit_quick_test(void* context) {
    LedRingTesterApp* app = context;

    // Stop safety timer
    furi_timer_stop(app->safety_timer);

    // Stop and free timer
    with_view_model(
        app->quick_test_view,
        QuickTestModel* model,
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
