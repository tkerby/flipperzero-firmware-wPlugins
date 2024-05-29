#include "flipboard_model_i.h"

/**
 * @brief flipboard_model_alloc allocates a FlipboardModel.
 * @param app_name The name of the application.  The name is used to 
 * load and save the key settings, but could also be displayed to the user.
 * @param single_button_mode If true, only one button can be pressed at a time.
 * @param fields The fields of the key settings that apply for this application.
 * @return A pointer to a FlipboardModel.
*/
FlipboardModel*
    flipboard_model_alloc(char* app_name, bool single_button_mode, ActionModelFields fields) {
    FlipboardModel* model = (FlipboardModel*)malloc(sizeof(FlipboardModel));
    model->name = app_name;
    model->resources = resources_alloc();
    model->action_model_fields = fields;
    model->single_button_mode = single_button_mode;
    model->keyboard = flipboard_keyboard_alloc();
    model->speaker = speaker_alloc();
    model->button_monitor = NULL;
    model->gui_refresh_timer = NULL;
    model->leds = flipboard_leds_alloc(model->resources);
    model->backlight = backlight_alloc();
    backlight_on(model->backlight);
    model->custom_data = NULL;

    // NOTE: We will call flipboard_model_load(model); when
    // the app_menu is displaying the splash screen.

    return model;
}

/**
 * @brief flipboard_model_free frees a FlipboardModel.
 * @details flipboard_model_free frees a FlipboardModel.  It also 
 * saves the key settings.
 * @param model The FlipboardModel to free.
*/
void flipboard_model_free(FlipboardModel* model) {
    flipboard_model_save(model, model->action_model_fields);

    if(model->speaker) {
        speaker_free(model->speaker);
    }

    if(model->backlight) {
        backlight_free(model->backlight);
    }

    if(model->button_monitor) {
        button_monitor_free(model->button_monitor);
    }

    if(model->keyboard) {
        flipboard_keyboard_free(model->keyboard);
    }

    if(model->leds) {
        flipboard_leds_free(model->leds);
    }

    if(model->gui_refresh_timer) {
        furi_timer_free(model->gui_refresh_timer);
    }

    if(model->resources) {
        resources_free(model->resources);
    }

    free(model);
}

/**
 * @brief flipboard_model_get_name gets the name of the FlipboardModel.  
 * @details The name is used to load and save the key settings, but could
 * also be displayed to the user.
 * @param model The FlipboardModel.
 * @return The name of the FlipboardModel.
*/
char* flipboard_model_get_name(FlipboardModel* model) {
    return model->name;
}

Resources* flipboard_model_get_resources(FlipboardModel* model) {
    return model->resources;
}

/**
 * @brief flipboard_model_get_single_button_mode gets the single button mode
 *  of the FlipboardModel.  If true, only one button can be pressed at a time.
 * @param model The FlipboardModel.
 * @return True if only one button can be pressed at a time.
*/
bool flipboard_model_get_single_button_mode(FlipboardModel* model) {
    return model->single_button_mode;
}

/**
 * @brief flipboard_model_get_action_model_fields gets the fields of the
 * key settings that apply for this application.
 * @param model The FlipboardModel.
 * @return The fields of the key settings that apply for this application.
*/
ActionModelFields flipboard_model_get_action_model_fields(FlipboardModel* model) {
    return model->action_model_fields;
}

/**
 * @brief flipboard_model_get_action_model gets the ButtonModel for a given key.
 * @brief flipboard_model_get_action_model gets the ButtonModel for a given key. 
 * For single button keys, the valid indexes are 0, 1, 3, 7.  For multi-button keys, the
 * valid indexes are 0-15.  This function may return NULL, if there is no setting.
 * @param model The FlipboardModel.
 * @param key The key.
 * @return The ActionModel for the key.
*/
ActionModel* flipboard_model_get_action_model(FlipboardModel* model, uint8_t key) {
    furi_assert(key < 16);
    return model->action_model[key];
}

/**
 * @brief flipboard_model_get_button_monitor gets the ButtonMonitor for the FlipboardModel.
 * @details flipboard_model_get_button_monitor gets the ButtonMonitor for the FlipboardModel.
 * The ButtonMonitor is used to monitor the FlipBoard button presses.
 * @param model The FlipboardModel.
 * @return The ButtonMonitor for the FlipboardModel.
*/
ButtonMonitor* flipboard_model_get_button_monitor(FlipboardModel* model) {
    return model->button_monitor;
}

/**
 * @brief flipboard_model_get_keyboard gets the Keyboard for the FlipboardModel.
 * @details flipboard_model_get_keyboard gets the Keyboard for the FlipboardModel.
 * The Keyboard is used to send keystrokes to the host.
 * @param model The FlipboardModel.
 * @return The Keyboard for the FlipboardModel.
*/
FlipboardKeyboard* flipboard_model_get_keyboard(FlipboardModel* model) {
    return model->keyboard;
}

/**
 * @brief flipboard_model_get_leds gets the Leds for the FlipboardModel.
 * @details flipboard_model_get_leds gets the Leds for the FlipboardModel. 
 * The Leds are the addressable LEDs on the FlipBoard, that light each button.
 * @param model The FlipboardModel.
 * @return The Leds for the FlipboardModel.
*/
FlipboardLeds* flipboard_model_get_leds(FlipboardModel* model) {
    return model->leds;
}

/**
 * @brief flipboard_model_get_speaker gets the Speaker for the FlipboardModel.
 * @details flipboard_model_get_speaker gets the Speaker for the FlipboardModel.
 * The Speaker is the internal Flipper Zero speaker and is used to play tones.
 * @param model The FlipboardModel.
 * @return The Speaker for the FlipboardModel.
*/
Speaker* flipboard_model_get_speaker(FlipboardModel* model) {
    return model->speaker;
}

/**
 * @brief flipboard_model_get_custom_data gets the custom data for the FlipboardModel.
 * @details flipboard_model_get_custom_data gets the custom data for the FlipboardModel.
 * The custom data is used to store application specific data, use this to further
 * extend the FlipboardModel.
 * @param model The FlipboardModel.
 * @return The custom data for the FlipboardModel.
*/
void* flipboard_model_get_custom_data(FlipboardModel* model) {
    return model->custom_data;
}

/**
 * @brief flipboard_model_set_custom_data sets the custom data for the FlipboardModel.
 * @details flipboard_model_set_custom_data sets the custom data for the FlipboardModel.
 * The custom data is used to store application specific data, use this to further
 * extend the FlipboardModel.  The caller is responsible for freeing any previous data
 * and for freeing the new custom_data.
 * @param model The FlipboardModel.
 * @param custom_data The custom data for the FlipboardModel.
*/
void flipboard_model_set_custom_data(FlipboardModel* model, void* custom_data) {
    // NOTE: The caller is responsible for freeing the previous and new custom_data.
    model->custom_data = custom_data;
}

/**
 * @brief flipboard_model_update_gui is called to redraw the GUI.
 * @details flipboard_model_update_gui forces a redraw of the GUI.
*/
void flipboard_model_update_gui(FlipboardModel* model) {
    UNUSED(model);
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_direct_draw_acquire(gui);
    gui_direct_draw_release(gui);
    furi_record_close(RECORD_GUI);
}

/**
 * @brief flipboard_model_tick is called periodically to update the GUI.
 * @details flipboard_model_tick is called periodically to update the GUI.
 * @param model The FlipboardModel.
*/
static void flipboard_model_tick(void* context) {
    flipboard_model_update_gui((FlipboardModel*)context);
}

/**
 * @brief flipboard_model_get_gui_refresh_speed_ms gets the GUI refresh speed in milliseconds.
 * @details flipboard_model_get_gui_refresh_speed_ms gets the GUI refresh speed in milliseconds.
 * The GUI refresh speed is the rate at which the GUI is updated.  The GUI is updated
 * by calling the flipboard_model_tick callback, which invokes __gui_redraw.
 * @param model The FlipboardModel.
 * @return The GUI refresh speed in milliseconds.
*/
void flipboard_model_set_gui_refresh_speed_ms(FlipboardModel* model, uint32_t update_rate_ms) {
    if(update_rate_ms == 0) {
        if(model->gui_refresh_timer) {
            furi_timer_free(model->gui_refresh_timer);
            model->gui_refresh_timer = NULL;
        }
        flipboard_model_update_gui(model);
        return;
    }

    if(model->gui_refresh_timer == NULL) {
        model->gui_refresh_timer =
            furi_timer_alloc(flipboard_model_tick, FuriTimerTypePeriodic, model);
    }
    furi_timer_start(model->gui_refresh_timer, furi_ms_to_ticks(update_rate_ms));
}

/**
 * @brief flipboard_model_set_action_model sets the ActionModel for a given action.
 * @details flipboard_model_set_action_model sets the ActionModel for a given action.
 * For single buttons, the valid indexes are 0, 1, 3, 7.  For multi buttons, the valid indexes
 * are 0-15.  The ActionModel is used to configure the action settings.
 * @param model The FlipboardModel.
 * @param index The index of the action.
 * @param button_model The ActionModel for the action.
*/
void flipboard_model_set_action_model(
    FlipboardModel* model,
    uint8_t index,
    ActionModel* action_model) {
    model->action_model[index] = action_model;
}

/**
 * @brief flipboard_model_set_button_monitor sets the ButtonMonitor for the FlipboardModel.
 * @details flipboard_model_set_button_monitor sets the ButtonMonitor for the FlipboardModel.
 * The ButtonMonitor is used to monitor the FlipBoard button presses.
 * @param model The FlipboardModel.
 * @param callback The callback to invoke when a button is pressed.
 * @param context The context to pass to the callback.
 */
void flipboard_model_set_button_monitor(
    FlipboardModel* model,
    ButtonMonitorEventCallback callback,
    void* context) {
    if(model->button_monitor) {
        button_monitor_free(model->button_monitor);
        model->button_monitor = NULL;
    }

    if(callback != NULL) {
        model->button_monitor = button_monitor_alloc();
        button_monitor_set_callback(model->button_monitor, callback, context);
    }
}

/**
 * @brief flipboard_model_play_tone plays a tone on the FlipboardModel speaker.
 * @param model The FlipboardModel.
 * @param action_model The ActionModel for the action.
*/
void flipboard_model_play_tone(FlipboardModel* model, ActionModel* action_model) {
    Speaker* speaker = flipboard_model_get_speaker(model);
    speaker_set_frequency(speaker, action_model_get_frequency(action_model));
}

/**
 * @brief flipboard_model_set_backlight sets the backlight.
 * @details flipboard_model_set_backlight sets the backlight.
 * @param model The FlipboardModel.
 * @param light_on If true, the backlight is turned on, otherwise it is turned off.
*/
void flipboard_model_set_backlight(FlipboardModel* model, bool light_on) {
    if(light_on) {
        backlight_on(model->backlight);
    } else {
        backlight_off(model->backlight);
    }
}

/**
 * @brief flipboard_model_set_colors sets the colors for the FlipboardModel.
 * @details flipboard_model_set_colors sets the colors for the FlipboardModel.
 * The colors are used to set the color of the LEDs for each button.
 * @param model The FlipboardModel.
 * @param action_model The ActionModel for the action.
 * @param new_key The keys that were pressed.
*/
void flipboard_model_set_colors(FlipboardModel* model, ActionModel* action_model, uint8_t new_key) {
    FlipboardLeds* leds = flipboard_model_get_leds(model);
    uint32_t color = action_model ? action_model_get_color_down(action_model) : 0xFFFFFF;
    ActionModel* am1 = flipboard_model_get_action_model(model, SwitchId1);
    ActionModel* am2 = flipboard_model_get_action_model(model, SwitchId2);
    ActionModel* am3 = flipboard_model_get_action_model(model, SwitchId3);
    ActionModel* am4 = flipboard_model_get_action_model(model, SwitchId4);
    uint32_t color1 = am1 ? action_model_get_color_up(am1) : 0x000000;
    uint32_t color2 = am2 ? action_model_get_color_up(am2) : 0x000000;
    uint32_t color3 = am3 ? action_model_get_color_up(am3) : 0x000000;
    uint32_t color4 = am4 ? action_model_get_color_up(am4) : 0x000000;
    color1 = (new_key & LedId1) ? color : color1;
    color2 = (new_key & LedId2) ? color : color2;
    color3 = (new_key & LedId3) ? color : color3;
    color4 = (new_key & LedId4) ? color : color4;
    flipboard_leds_set(leds, LedId1, color1);
    flipboard_leds_set(leds, LedId2, color2);
    flipboard_leds_set(leds, LedId3, color3);
    flipboard_leds_set(leds, LedId4, color4);
    flipboard_leds_update(leds);
}

/**
 * @brief flipboard_model_send_keystrokes sends keystrokes to the host.
 * @details flipboard_model_send_keystrokes sends keystrokes to the host.
 * @param model The FlipboardModel.
 * @param action_model The ActionModel for the button that was pressed.
 * @return True if any "messages" (Msg1-Msg4) were also sent.
*/
bool flipboard_model_send_keystrokes(FlipboardModel* model, ActionModel* action_model) {
    bool sent_messages = false;
    uint8_t keystroke_count = action_model_get_keystrokes_count(action_model);
    uint16_t modifiers = 0;
    for(int i = 0; i < keystroke_count; i++) {
        Keystroke keystroke = action_model_get_keystroke(action_model, i);
        if(keystroke.button_code == 0 || keystroke.count == 0) {
            continue;
        }

        if(keystroke.button_code == 1) {
            if(keystroke.count > 0) {
                furi_delay_ms(100 * keystroke.count);
            }

            modifiers = 0;
            continue;
        } else if(
            keystroke.button_code >= 0xf1 &&
            keystroke.button_code <= 0xf4) { // 0xf1 = Message 1 ... 0xf4 = Message 4
            for(int j = 0; j < keystroke.count; j++) {
                flipboard_model_send_text(model, action_model, keystroke.button_code - 0xf1);
            }
            sent_messages = true;
            continue;
        }

        uint16_t send_modifiers = 0;
        if(keystroke.button_code == HID_KEYBOARD_L_CTRL) {
            send_modifiers = (1 << 8);
        } else if(keystroke.button_code == HID_KEYBOARD_L_SHIFT) {
            send_modifiers = (1 << 9);
        } else if(keystroke.button_code == HID_KEYBOARD_L_ALT) {
            send_modifiers = (1 << 10);
        } else if(keystroke.button_code == HID_KEYBOARD_L_GUI) {
            send_modifiers = (1 << 11);
        } else if(keystroke.button_code == HID_KEYBOARD_R_CTRL) {
            send_modifiers = (1 << 12);
        } else if(keystroke.button_code == HID_KEYBOARD_R_SHIFT) {
            send_modifiers = (1 << 13);
        } else if(keystroke.button_code == HID_KEYBOARD_R_ALT) {
            send_modifiers = (1 << 14);
        } else if(keystroke.button_code == HID_KEYBOARD_R_GUI) {
            send_modifiers = (1 << 15);
        }

        modifiers |= send_modifiers;

        for(uint8_t count = keystroke.count; count != 0; count--) {
            flipboard_keyboard_send_keycode(
                flipboard_model_get_keyboard(model), keystroke.button_code | modifiers);
            flipboard_keyboard_release_keycode(
                flipboard_model_get_keyboard(model), keystroke.button_code);
        }
        flipboard_keyboard_release_all(flipboard_model_get_keyboard(model));

        if(!send_modifiers) {
            modifiers = 0;
        }
    }

    return sent_messages;
}

/**
 * @brief flipboard_model_send_text sends text to the host.
 * @details flipboard_model_send_text sends text to the host.
 * @param model The FlipboardModel.
 * @param action_model The ActionModel for the action.
 * @param message_number The message number to send (0-3).
*/
void flipboard_model_send_text(
    FlipboardModel* model,
    ActionModel* action_model,
    uint8_t message_number) {
    FuriString* message = action_model_get_message(action_model, message_number);
    if(message) {
        flipboard_keyboard_send_text(
            flipboard_model_get_keyboard(model), furi_string_get_cstr(message));
    }
}

/**
 * @brief flipboard_model_reduce reduces the button presses to a single button.
 * @details flipboard_model_reduce reduces the button presses to a single button, if in
 * single button mode (otherwise returns the button combo).
 * @param model The FlipboardModel.
 * @param new_button The button that was pressed.
 * @param left_wins If true, the leftmost button wins, otherwise the rightmost button wins.
 * @return The reduced button.
*/
uint8_t flipboard_model_reduce(FlipboardModel* model, uint8_t new_key, bool left_wins) {
    uint8_t reduced_new_key = new_key;

    if(new_key == 0) {
        return 0;
    }

    if(flipboard_model_get_single_button_mode(model)) {
        uint8_t mask = left_wins ? 0x1 : 0x80;
        while(((new_key & mask) == 0) && mask) {
            if(left_wins) {
                mask = mask << 1;
            } else {
                mask = mask >> 1;
            }
        }
        reduced_new_key = mask;
    }

    return reduced_new_key;
}

void flipboard_model_set_defaults_callback(
    FlipboardModel* model,
    FlipboardModelSetDefaults callback) {
    model->set_defaults = callback;
}

void flipboard_model_set_defaults(FlipboardModel* model) {
    if(model->set_defaults) {
        model->set_defaults(model);
    }
}