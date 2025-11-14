#include "../switch_controller_app.h"
#include <input/input.h>

// Control modes
typedef enum {
    ControlModeDPad = 0,
    ControlModeLeftStick,
    ControlModeRightStick,
    ControlModeCount,
} ControlMode;

// Controller view model
typedef struct {
    SwitchControllerState state;
    bool connected;
    uint8_t selected_button; // For button menu (0 = none, 1-9 = button index)
    ControlMode control_mode; // Current control mode
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

    // Draw control mode
    canvas_set_font(canvas, FontSecondary);
    const char* mode_str = "Mode: ";
    switch(m->control_mode) {
    case ControlModeDPad:
        canvas_draw_str(canvas, 2, 30, "Mode: D-Pad");
        break;
    case ControlModeLeftStick:
        canvas_draw_str(canvas, 2, 30, "Mode: Left Stick");
        break;
    case ControlModeRightStick:
        canvas_draw_str(canvas, 2, 30, "Mode: Right Stick");
        break;
    default:
        break;
    }

    // Draw instructions
    canvas_draw_str(canvas, 2, 42, "Directions: Move/DPad");
    canvas_draw_str(canvas, 2, 51, "OK: A | Back: B");
    canvas_draw_str(canvas, 2, 60, "Long OK: Btns");

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

        // Show mode toggle hint
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 14, 63, "Long Back: Toggle Mode");
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
                // Handle directional inputs based on control mode
                if(event->key == InputKeyUp) {
                    if(event->type == InputTypePress) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_UP;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.ly = 0x0000; // Full up
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.ry = 0x0000; // Full up
                        }
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_NEUTRAL;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.ly = STICK_CENTER;
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.ry = STICK_CENTER;
                        }
                        consumed = true;
                    }
                } else if(event->key == InputKeyDown) {
                    if(event->type == InputTypePress) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_DOWN;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.ly = 0xFFFF; // Full down
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.ry = 0xFFFF; // Full down
                        }
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_NEUTRAL;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.ly = STICK_CENTER;
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.ry = STICK_CENTER;
                        }
                        consumed = true;
                    }
                } else if(event->key == InputKeyLeft) {
                    if(event->type == InputTypePress) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_LEFT;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.lx = 0x0000; // Full left
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.rx = 0x0000; // Full left
                        }
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_NEUTRAL;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.lx = STICK_CENTER;
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.rx = STICK_CENTER;
                        }
                        consumed = true;
                    }
                } else if(event->key == InputKeyRight) {
                    if(event->type == InputTypePress) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_RIGHT;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.lx = 0xFFFF; // Full right
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.rx = 0xFFFF; // Full right
                        }
                        consumed = true;
                    } else if(event->type == InputTypeRelease) {
                        if(model->control_mode == ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_NEUTRAL;
                        } else if(model->control_mode == ControlModeLeftStick) {
                            model->state.lx = STICK_CENTER;
                        } else if(model->control_mode == ControlModeRightStick) {
                            model->state.rx = STICK_CENTER;
                        }
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
                    } else if(event->type == InputTypeLong) {
                        // Toggle control mode
                        model->control_mode = (model->control_mode + 1) % ControlModeCount;
                        // Reset stick positions when switching modes
                        if(model->control_mode != ControlModeDPad) {
                            model->state.hat = SWITCH_HAT_NEUTRAL;
                        }
                        consumed = true;
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
            model->control_mode = ControlModeDPad;
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
