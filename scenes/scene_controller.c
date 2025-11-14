#include "../switch_controller_app.h"
#include <input/input.h>

// Controller view model
typedef struct {
    SwitchControllerState state;
    bool connected;
    uint8_t selected_button; // For button menu (0 = none, 1-9 = button index)
} ControllerViewModel;

// Button menu items (for long press OK)
static const char* button_menu_items[] = {
    "X",
    "Y",
    "L",
    "R",
    "ZL",
    "ZR",
    "Plus",
    "Minus",
    "Home",
};

static const uint16_t button_menu_flags[] = {
    SWITCH_BTN_X,
    SWITCH_BTN_Y,
    SWITCH_BTN_L,
    SWITCH_BTN_R,
    SWITCH_BTN_ZL,
    SWITCH_BTN_ZR,
    SWITCH_BTN_PLUS,
    SWITCH_BTN_MINUS,
    SWITCH_BTN_HOME,
};

// Draw callback
static void switch_controller_view_controller_draw_callback(Canvas* canvas, void* model) {
    ControllerViewModel* m = model;

    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);

    if(m->connected) {
        canvas_draw_str(canvas, 2, 10, "Switch Controller");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 20, "Connected");
    } else {
        canvas_draw_str(canvas, 2, 10, "Switch Controller");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 2, 20, "Not Connected");
    }

    // Draw instructions
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 2, 35, "DPad: Up/Down/Left/Right");
    canvas_draw_str(canvas, 2, 44, "OK: A button");
    canvas_draw_str(canvas, 2, 53, "Back: B button");
    canvas_draw_str(canvas, 2, 62, "Long OK: More buttons");

    // Show button menu if active
    if(m->selected_button > 0) {
        canvas_draw_frame(canvas, 10, 15, 108, 40);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 14, 25, "Select Button:");

        canvas_set_font(canvas, FontSecondary);
        uint8_t idx = m->selected_button - 1;
        if(idx > 0) {
            canvas_draw_str(canvas, 14, 35, button_menu_items[idx - 1]);
        }
        canvas_draw_str(canvas, 14, 45, button_menu_items[idx]);
        if(idx < 8) {
            canvas_draw_str(canvas, 14, 55, button_menu_items[idx + 1]);
        }

        // Highlight selected
        canvas_draw_str(canvas, 4, 45, ">");
    }
}

// Input callback
static bool switch_controller_view_controller_input_callback(InputEvent* event, void* context) {
    SwitchControllerApp* app = context;
    bool consumed = false;

    with_view_model(
        app->controller_view,
        ControllerViewModel * model,
        {
            if(model->selected_button > 0) {
                // In button menu
                if(event->type == InputTypePress || event->type == InputTypeRepeat) {
                    if(event->key == InputKeyUp) {
                        if(model->selected_button > 1) {
                            model->selected_button--;
                        }
                        consumed = true;
                    } else if(event->key == InputKeyDown) {
                        if(model->selected_button < 9) {
                            model->selected_button++;
                        }
                        consumed = true;
                    } else if(event->key == InputKeyOk) {
                        // Press selected button
                        uint8_t idx = model->selected_button - 1;
                        model->state.buttons |= button_menu_flags[idx];
                        model->selected_button = 0; // Close menu
                        consumed = true;
                    } else if(event->key == InputKeyBack) {
                        model->selected_button = 0; // Close menu
                        consumed = true;
                    }
                }
            } else {
                // Normal controller mode
                if(event->key == InputKeyUp) {
                    if(event->type == InputTypePress) {
                        model->state.hat = SWITCH_HAT_UP;
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        model->state.hat = SWITCH_HAT_NEUTRAL;
                        consumed = true;
                    }
                } else if(event->key == InputKeyDown) {
                    if(event->type == InputTypePress) {
                        model->state.hat = SWITCH_HAT_DOWN;
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        model->state.hat = SWITCH_HAT_NEUTRAL;
                        consumed = true;
                    }
                } else if(event->key == InputKeyLeft) {
                    if(event->type == InputTypePress) {
                        model->state.hat = SWITCH_HAT_LEFT;
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        model->state.hat = SWITCH_HAT_NEUTRAL;
                        consumed = true;
                    }
                } else if(event->key == InputKeyRight) {
                    if(event->type == InputTypePress) {
                        model->state.hat = SWITCH_HAT_RIGHT;
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        model->state.hat = SWITCH_HAT_NEUTRAL;
                        consumed = true;
                    }
                } else if(event->key == InputKeyOk) {
                    if(event->type == InputTypePress) {
                        model->state.buttons |= SWITCH_BTN_A;
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        model->state.buttons &= ~SWITCH_BTN_A;
                        consumed = true;
                    } else if(event->type == InputTypeLong) {
                        // Open button menu
                        model->selected_button = 1;
                        consumed = true;
                    }
                } else if(event->key == InputKeyBack) {
                    if(event->type == InputTypePress) {
                        model->state.buttons |= SWITCH_BTN_B;
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        model->state.buttons &= ~SWITCH_BTN_B;
                        consumed = false; // Let it go back
                    }
                }
            }

            // Send USB report if connected
            if(model->connected) {
                usb_hid_switch_send_report(&model->state);
            }
        },
        consumed);

    return consumed;
}

// Timer callback to send periodic updates
static void switch_controller_view_controller_timer_callback(void* context) {
    SwitchControllerApp* app = context;

    with_view_model(
        app->controller_view,
        ControllerViewModel * model,
        {
            if(model->connected) {
                usb_hid_switch_send_report(&model->state);
            }
        },
        false);
}

void switch_controller_scene_on_enter_controller(void* context) {
    SwitchControllerApp* app = context;

    // Create controller view if it doesn't exist
    if(!app->controller_view) {
        app->controller_view = view_alloc();
        view_set_context(app->controller_view, app);
        view_allocate_model(
            app->controller_view, ViewModelTypeLocking, sizeof(ControllerViewModel));
        view_set_draw_callback(app->controller_view, switch_controller_view_controller_draw_callback);
        view_set_input_callback(app->controller_view, switch_controller_view_controller_input_callback);
        view_dispatcher_add_view(
            app->view_dispatcher, SwitchControllerViewController, app->controller_view);
    }

    // Initialize USB
    app->usb_mode_prev = furi_hal_usb_get_config();
    if(usb_hid_switch_init()) {
        app->usb_connected = true;
    }

    // Initialize model
    with_view_model(
        app->controller_view,
        ControllerViewModel * model,
        {
            usb_hid_switch_reset_state(&model->state);
            model->connected = app->usb_connected;
            model->selected_button = 0;
        },
        true);

    view_dispatcher_switch_to_view(app->view_dispatcher, SwitchControllerViewController);
}

bool switch_controller_scene_on_event_controller(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void switch_controller_scene_on_exit_controller(void* context) {
    SwitchControllerApp* app = context;

    // Deinitialize USB
    if(app->usb_connected) {
        usb_hid_switch_deinit();
        if(app->usb_mode_prev) {
            furi_hal_usb_set_config(app->usb_mode_prev, NULL);
        }
        app->usb_connected = false;
    }
}
