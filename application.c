#include "application.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void tonuino_submenu_callback(void* context, uint32_t index);
static void tonuino_widget_callback(GuiButtonType result, InputType type, void* context);
static bool tonuino_rapid_write_input_callback(InputEvent* event, void* context);
static void tonuino_rapid_write_update_display(TonuinoApp* app);
static uint32_t tonuino_exit(void* context);
static uint32_t tonuino_back_to_menu(void* context);

static const char* mode_names[] = {
    "",
    "Hoerspiel (Random)",
    "Album (Complete)",
    "Party (Shuffle)",
    "Einzel (Single)",
    "Hoerbuch (Progress)",
    "Admin",
    "Hoerspiel Von-Bis",
    "Album Von-Bis",
    "Party Von-Bis",
    "Hoerbuch Einzel",
    "Repeat Last",
};

void tonuino_build_card_data(TonuinoApp* app) {
    app->card_data.box_id[0] = TONUINO_BOX_ID_0;
    app->card_data.box_id[1] = TONUINO_BOX_ID_1;
    app->card_data.box_id[2] = TONUINO_BOX_ID_2;
    app->card_data.box_id[3] = TONUINO_BOX_ID_3;
    app->card_data.version = TONUINO_VERSION;
    app->card_data.reserved[0] = 0x00;
    app->card_data.reserved[1] = 0x00;
    app->card_data.reserved[2] = 0x00;
    app->card_data.reserved[3] = 0x00;
    app->card_data.reserved[4] = 0x00;
    app->card_data.reserved[5] = 0x00;
    app->card_data.reserved[6] = 0x00;
}

static void tonuino_number_input_callback(void* context, int32_t number) {
    TonuinoApp* app = context;
    
    if(app->current_input_type == TonuinoInputTypeFolder) {
        if(number >= 1 && number <= 99) {
            app->card_data.folder = number;
        }
    } else if(app->current_input_type == TonuinoInputTypeSpecial1) {
        if(number >= 0 && number <= 255) {
            app->card_data.special1 = number;
        }
    } else if(app->current_input_type == TonuinoInputTypeSpecial2) {
        if(number >= 0 && number <= 255) {
            app->card_data.special2 = number;
        }
    }
    
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);
}

static void tonuino_submenu_folder_callback(void* context) {
    TonuinoApp* app = context;
    app->current_input_type = TonuinoInputTypeFolder;
    number_input_set_header_text(app->number_input, "Enter Folder");
    number_input_set_result_callback(
        app->number_input, tonuino_number_input_callback, app, app->card_data.folder, 1, 99);
    view_dispatcher_switch_to_view(app->view_dispatcher, 4);
}

static void tonuino_submenu_about_callback(void* context) {
    TonuinoApp* app = context;
    text_box_reset(app->text_box);
    
    char about_text[512];
    snprintf(about_text, sizeof(about_text),
        "TonUINO Writer\n"
        "for Flipper Zero\n\n"
        "Version: %s\n"
        "Build: %d\n\n"
        "Autor: Thomas\n"
        "Kekeisen-Schanz\n\n"
        "https://thomaskekeisen.de\n"
        "https://bastelsaal.de",
        APP_VERSION, APP_BUILD_NUMBER);
    
    text_box_set_text(app->text_box, about_text);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    view_dispatcher_switch_to_view(app->view_dispatcher, 3);
}

static void tonuino_restore_main_menu(TonuinoApp* app) {
    submenu_reset(app->submenu);
    view_set_previous_callback(submenu_get_view(app->submenu), tonuino_exit);
    submenu_add_item(app->submenu, "Set Folder", 100, tonuino_submenu_callback, app);
    submenu_add_item(app->submenu, "Set Mode", 101, tonuino_submenu_callback, app);
    submenu_add_item(app->submenu, "Set Special 1", 102, tonuino_submenu_callback, app);
    submenu_add_item(app->submenu, "Set Special 2", 103, tonuino_submenu_callback, app);
    submenu_add_item(app->submenu, "View Card", 104, tonuino_submenu_callback, app);
    submenu_add_item(app->submenu, "Read Card", 106, tonuino_submenu_callback, app);
    submenu_add_item(app->submenu, "Write Card", 105, tonuino_submenu_callback, app);
    submenu_add_item(app->submenu, "Rapid Write", 108, tonuino_submenu_callback, app);
    submenu_add_item(app->submenu, "About", 107, tonuino_submenu_callback, app);
}

static void tonuino_submenu_mode_callback(void* context) {
    TonuinoApp* app = context;
    submenu_reset(app->submenu);
    
    view_set_previous_callback(submenu_get_view(app->submenu), tonuino_back_to_menu);
    
    submenu_add_item(app->submenu, "Back", 0, tonuino_submenu_callback, app);
    
    char mode_label[64];
    for(int i = 1; i <= ModeRepeatLast; i++) {
        if(i == app->card_data.mode && app->card_data.mode >= 1 && app->card_data.mode <= ModeRepeatLast) {
            // Add indicator for selected mode (bold-like visual)
            snprintf(mode_label, sizeof(mode_label), "* %s", mode_names[i]);
        } else {
            snprintf(mode_label, sizeof(mode_label), "  %s", mode_names[i]);
        }
        submenu_add_item(
            app->submenu, mode_label, i, tonuino_submenu_callback, app);
    }
    
    if(app->card_data.mode >= 1 && app->card_data.mode <= ModeRepeatLast) {
        submenu_set_selected_item(app->submenu, app->card_data.mode);
    }
    
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);
}

static void tonuino_submenu_special1_callback(void* context) {
    TonuinoApp* app = context;
    app->current_input_type = TonuinoInputTypeSpecial1;
    number_input_set_header_text(app->number_input, "Special 1");
    number_input_set_result_callback(
        app->number_input, tonuino_number_input_callback, app, app->card_data.special1, 0, 255);
    view_dispatcher_switch_to_view(app->view_dispatcher, 4);
}

static void tonuino_submenu_special2_callback(void* context) {
    TonuinoApp* app = context;
    app->current_input_type = TonuinoInputTypeSpecial2;
    number_input_set_header_text(app->number_input, "Special 2");
    number_input_set_result_callback(
        app->number_input, tonuino_number_input_callback, app, app->card_data.special2, 0, 255);
    view_dispatcher_switch_to_view(app->view_dispatcher, 4);
}

static void tonuino_show_waiting_screen(TonuinoApp* app, const char* message) {
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, message);
    widget_add_string_element(
        app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "Place card on back");
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Cancel", tonuino_widget_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, 2);
}

static void tonuino_submenu_write_callback(void* context) {
    TonuinoApp* app = context;
    
    // Check if we're in rapid write mode
    if(app->rapid_write_mode_active) {
        // Show waiting screen briefly
        tonuino_show_waiting_screen(app, "Waiting for card...");
        
        if(tonuino_write_card(app)) {
            notification_message(app->notifications, &sequence_success);
            // Set success message and restore rapid write display
            snprintf(app->rapid_write_status_message, sizeof(app->rapid_write_status_message), "Success");
            app->rapid_write_status_timeout = furi_get_tick() + furi_ms_to_ticks(2000);
            tonuino_rapid_write_update_display(app);
            // Ensure we're still on the widget view
            view_dispatcher_switch_to_view(app->view_dispatcher, 2);
        } else {
            notification_message(app->notifications, &sequence_error);
            // Set error message and restore rapid write display
            snprintf(app->rapid_write_status_message, sizeof(app->rapid_write_status_message), "Error");
            app->rapid_write_status_timeout = furi_get_tick() + furi_ms_to_ticks(2000);
            tonuino_rapid_write_update_display(app);
            // Ensure we're still on the widget view
            view_dispatcher_switch_to_view(app->view_dispatcher, 2);
        }
    } else {
        // Normal write mode - show full overlay
        tonuino_show_waiting_screen(app, "Waiting for card...");
        
        if(tonuino_write_card(app)) {
            notification_message(app->notifications, &sequence_success);
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Card Written!");
            widget_add_button_element(
                app->widget, GuiButtonTypeLeft, "Back", tonuino_widget_callback, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, 2);
        } else {
            notification_message(app->notifications, &sequence_error);
            widget_reset(app->widget);
            widget_add_string_element(
                app->widget, 64, 32, AlignCenter, AlignCenter, FontPrimary, "Write Failed!");
            widget_add_string_element(
                app->widget, 64, 44, AlignCenter, AlignCenter, FontSecondary, "Card not detected");
            widget_add_button_element(
                app->widget, GuiButtonTypeLeft, "Back", tonuino_widget_callback, app);
            view_dispatcher_switch_to_view(app->view_dispatcher, 2);
        }
    }
}

static void tonuino_submenu_view_callback(void* context) {
    TonuinoApp* app = context;
    char buffer[128];
    widget_reset(app->widget);
    
    snprintf(buffer, sizeof(buffer), "Folder: %d", app->card_data.folder);
    widget_add_string_element(app->widget, 0, 0, AlignLeft, AlignTop, FontPrimary, buffer);
    
    const char* mode_str;
    if(app->card_data.mode >= 1 && app->card_data.mode <= ModeRepeatLast) {
        mode_str = mode_names[app->card_data.mode];
    } else {
        mode_str = "Unknown";
    }
    snprintf(buffer, sizeof(buffer), "Mode: %s", mode_str);
    widget_add_string_element(app->widget, 0, 12, AlignLeft, AlignTop, FontSecondary, buffer);
    
    snprintf(buffer, sizeof(buffer), "Special 1: %d", app->card_data.special1);
    widget_add_string_element(app->widget, 0, 24, AlignLeft, AlignTop, FontSecondary, buffer);
    
    snprintf(buffer, sizeof(buffer), "Special 2: %d", app->card_data.special2);
    widget_add_string_element(app->widget, 0, 36, AlignLeft, AlignTop, FontSecondary, buffer);
    
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", tonuino_widget_callback, app);
    widget_add_button_element(
        app->widget, GuiButtonTypeRight, "Write", tonuino_widget_callback, app);
    
    view_dispatcher_switch_to_view(app->view_dispatcher, 2);
}


static void tonuino_submenu_read_callback(void* context) {
    TonuinoApp* app = context;
    tonuino_show_waiting_screen(app, "Waiting for card...");
    
    if(tonuino_read_card(app)) {
        notification_message(app->notifications, &sequence_success);
        text_box_reset(app->text_box);
        
        char mode_str[64];
        if(app->card_data.mode >= 1 && app->card_data.mode <= ModeRepeatLast) {
            snprintf(mode_str, sizeof(mode_str), "%s", mode_names[app->card_data.mode]);
        } else {
            snprintf(mode_str, sizeof(mode_str), "Unknown (%d)", app->card_data.mode);
        }
        
        snprintf(app->read_text_buffer, sizeof(app->read_text_buffer),
            "Card Read:\n\n"
            "Box ID: %02X %02X %02X %02X\n"
            "Version: %d\n"
            "Folder: %d\n"
            "Mode: %s (%d)\n"
            "Special 1: %d\n"
            "Special 2: %d\n\n"
            "Raw Data:\n"
            "%02X %02X %02X %02X %02X %02X %02X %02X\n"
            "%02X %02X %02X %02X %02X %02X %02X %02X",
            app->card_data.box_id[0], app->card_data.box_id[1], 
            app->card_data.box_id[2], app->card_data.box_id[3],
            app->card_data.version,
            app->card_data.folder,
            mode_str,
            app->card_data.mode,
            app->card_data.special1,
            app->card_data.special2,
            app->card_data.box_id[0], app->card_data.box_id[1], 
            app->card_data.box_id[2], app->card_data.box_id[3],
            app->card_data.version, app->card_data.folder,
            app->card_data.mode, app->card_data.special1,
            app->card_data.special2, app->card_data.reserved[0],
            app->card_data.reserved[1], app->card_data.reserved[2],
            app->card_data.reserved[3], app->card_data.reserved[4],
            app->card_data.reserved[5], app->card_data.reserved[6]);
        
        text_box_set_text(app->text_box, app->read_text_buffer);
        text_box_set_font(app->text_box, TextBoxFontText);
        text_box_set_focus(app->text_box, TextBoxFocusStart);
        view_dispatcher_switch_to_view(app->view_dispatcher, 3);
    } else {
        notification_message(app->notifications, &sequence_error);
        widget_reset(app->widget);
        widget_add_string_element(
            app->widget, 64, 28, AlignCenter, AlignCenter, FontPrimary, "Read Failed!");
        widget_add_string_element(
            app->widget, 64, 40, AlignCenter, AlignCenter, FontSecondary, "No TonUINO data");
        widget_add_string_element(
            app->widget, 64, 52, AlignCenter, AlignCenter, FontSecondary, "found on card");
        widget_add_button_element(
            app->widget, GuiButtonTypeLeft, "Back", tonuino_widget_callback, app);
        view_dispatcher_switch_to_view(app->view_dispatcher, 2);
    }
}

static void tonuino_submenu_callback(void* context, uint32_t index) {
    TonuinoApp* app = context;
    
    if(index == 0) {
        tonuino_restore_main_menu(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, 0);
    } else if(index == 100) {
        tonuino_submenu_folder_callback(context);
    } else if(index == 101) {
        tonuino_submenu_mode_callback(context);
    } else if(index == 102) {
        tonuino_submenu_special1_callback(context);
    } else if(index == 103) {
        tonuino_submenu_special2_callback(context);
    } else if(index == 104) {
        tonuino_submenu_view_callback(context);
    } else if(index == 105) {
        tonuino_submenu_write_callback(context);
    } else if(index == 106) {
        tonuino_submenu_read_callback(context);
    } else if(index == 107) {
        tonuino_submenu_about_callback(context);
    } else if(index == 108) {
        // Rapid Write mode
        app->rapid_write_mode_active = true;
        app->rapid_write_selected_folder = true; // Start with folder selected
        tonuino_rapid_write_update_display(app);
        view_set_context(widget_get_view(app->widget), app);
        view_set_input_callback(widget_get_view(app->widget), tonuino_rapid_write_input_callback);
        view_dispatcher_switch_to_view(app->view_dispatcher, 2);
    } else if(index >= 1 && index <= ModeRepeatLast) {
        app->card_data.mode = index;
        tonuino_restore_main_menu(app);
        view_dispatcher_switch_to_view(app->view_dispatcher, 0);
    }
}


static void tonuino_widget_callback(GuiButtonType result, InputType type, void* context) {
    TonuinoApp* app = context;
    if(type == InputTypeShort) {
        if(result == GuiButtonTypeRight) {
            if(app->rapid_write_mode_active) {
                app->rapid_write_mode_active = false;
                view_set_input_callback(widget_get_view(app->widget), NULL);
                tonuino_submenu_write_callback(context);
            } else {
                tonuino_submenu_write_callback(context);
            }
        } else {
            if(app->rapid_write_mode_active) {
                app->rapid_write_mode_active = false;
                view_set_input_callback(widget_get_view(app->widget), NULL);
            }
            view_dispatcher_switch_to_view(app->view_dispatcher, 0);
        }
    }
}

static bool tonuino_rapid_write_input_callback(InputEvent* event, void* context) {
    TonuinoApp* app = context;
    
    if(event->type == InputTypeShort) {
        // Check if status message should be cleared on any input (except OK which triggers write)
        if(app->rapid_write_status_message[0] != '\0' && event->key != InputKeyOk) {
            app->rapid_write_status_message[0] = '\0';
            tonuino_rapid_write_update_display(app);
        }
        if(event->key == InputKeyLeft) {
            if(app->rapid_write_selected_folder) {
                // Decrement folder
                if(app->card_data.folder > 1) {
                    app->card_data.folder--;
                } else {
                    app->card_data.folder = 99; // Wrap around
                }
            } else {
                // Decrement mode
                if(app->card_data.mode > 1) {
                    app->card_data.mode--;
                } else {
                    app->card_data.mode = ModeRepeatLast; // Wrap around
                }
            }
            tonuino_rapid_write_update_display(app);
            return true;
        } else if(event->key == InputKeyRight) {
            if(app->rapid_write_selected_folder) {
                // Increment folder
                if(app->card_data.folder < 99) {
                    app->card_data.folder++;
                } else {
                    app->card_data.folder = 1; // Wrap around
                }
            } else {
                // Increment mode
                if(app->card_data.mode < ModeRepeatLast) {
                    app->card_data.mode++;
                } else {
                    app->card_data.mode = 1; // Wrap around
                }
            }
            tonuino_rapid_write_update_display(app);
            return true;
        } else if(event->key == InputKeyUp || event->key == InputKeyDown) {
            // Toggle between folder and mode selection
            app->rapid_write_selected_folder = !app->rapid_write_selected_folder;
            tonuino_rapid_write_update_display(app);
            return true;
        } else if(event->key == InputKeyOk) {
            // OK button triggers write
            tonuino_submenu_write_callback(context);
            return true;
        }
    }
    return false;
}

static void tonuino_rapid_write_update_display(TonuinoApp* app) {
    widget_reset(app->widget);
    
    // Check if status message should be cleared
    if(app->rapid_write_status_message[0] != '\0' && 
       furi_get_tick() >= app->rapid_write_status_timeout) {
        app->rapid_write_status_message[0] = '\0';
    }
    
    char buffer[128];
    const char* mode_str;
    if(app->card_data.mode >= 1 && app->card_data.mode <= ModeRepeatLast) {
        mode_str = mode_names[app->card_data.mode];
    } else {
        mode_str = "Unknown";
    }
    
    // Display folder number at top
    if(app->rapid_write_selected_folder) {
        snprintf(buffer, sizeof(buffer), "> Folder: %d <", app->card_data.folder);
    } else {
        snprintf(buffer, sizeof(buffer), "Folder: %d", app->card_data.folder);
    }
    widget_add_string_element(app->widget, 64, 8, AlignCenter, AlignTop, FontPrimary, buffer);
    
    // Display mode below
    if(!app->rapid_write_selected_folder) {
        snprintf(buffer, sizeof(buffer), "> Mode: %s <", mode_str);
    } else {
        snprintf(buffer, sizeof(buffer), "Mode: %s", mode_str);
    }
    widget_add_string_element(app->widget, 64, 24, AlignCenter, AlignTop, FontSecondary, buffer);
    
    // Display status message or "OK = Write" prompt
    if(app->rapid_write_status_message[0] != '\0') {
        widget_add_string_element(
            app->widget, 64, 50, AlignCenter, AlignTop, FontSecondary, app->rapid_write_status_message);
    } else {
        widget_add_string_element(
            app->widget, 64, 50, AlignCenter, AlignTop, FontSecondary, "OK = Write");
    }
    
    // Buttons
    widget_add_button_element(
        app->widget, GuiButtonTypeLeft, "Back", tonuino_widget_callback, app);
}

static uint32_t tonuino_exit(void* context) {
    UNUSED(context);
    return VIEW_NONE;
}

static uint32_t tonuino_back_to_menu(void* context) {
    TonuinoApp* app = context;
    tonuino_restore_main_menu(app);
    return 0;
}

static uint32_t tonuino_text_box_exit(void* context) {
    UNUSED(context);
    return 0;
}

TonuinoApp* tonuino_app_alloc() {
    TonuinoApp* app = malloc(sizeof(TonuinoApp));
    
    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    
    app->submenu = submenu_alloc();
    view_set_previous_callback(submenu_get_view(app->submenu), tonuino_exit);
    view_dispatcher_add_view(app->view_dispatcher, 0, submenu_get_view(app->submenu));
    
    app->text_input = text_input_alloc();
    view_set_previous_callback(text_input_get_view(app->text_input), tonuino_back_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, 1, text_input_get_view(app->text_input));
    
    app->number_input = number_input_alloc();
    view_set_previous_callback(number_input_get_view(app->number_input), tonuino_back_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, 4, number_input_get_view(app->number_input));
    
    app->widget = widget_alloc();
    view_set_previous_callback(widget_get_view(app->widget), tonuino_back_to_menu);
    view_dispatcher_add_view(app->view_dispatcher, 2, widget_get_view(app->widget));
    
    app->text_box = text_box_alloc();
    view_set_previous_callback(text_box_get_view(app->text_box), tonuino_text_box_exit);
    view_dispatcher_add_view(app->view_dispatcher, 3, text_box_get_view(app->text_box));
    
    tonuino_restore_main_menu(app);
    
    tonuino_build_card_data(app);
    app->card_data.folder = 1;
    app->card_data.mode = ModeAlbum;
    app->card_data.special1 = 0;
    app->card_data.special2 = 0;
    app->card_ready = false;
    app->rapid_write_mode_active = false;
    app->rapid_write_selected_folder = true;
    app->rapid_write_status_message[0] = '\0';
    app->rapid_write_status_timeout = 0;
    
    return app;
}

void tonuino_app_free(TonuinoApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, 4);
    view_dispatcher_remove_view(app->view_dispatcher, 3);
    view_dispatcher_remove_view(app->view_dispatcher, 2);
    view_dispatcher_remove_view(app->view_dispatcher, 1);
    view_dispatcher_remove_view(app->view_dispatcher, 0);
    
    text_box_free(app->text_box);
    widget_free(app->widget);
    number_input_free(app->number_input);
    text_input_free(app->text_input);
    submenu_free(app->submenu);
    view_dispatcher_free(app->view_dispatcher);
    
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    
    free(app);
}

bool tonuino_read_card(TonuinoApp* app) {
    Nfc* nfc = nfc_alloc();
    if(!nfc) {
        return false;
    }
    
    MfClassicKey default_key = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    MfClassicBlock block;
    MfClassicError error;
    bool success = false;
    
    memset(&block, 0, sizeof(MfClassicBlock));
    memset(&app->card_data, 0, sizeof(TonuinoCard));
    
    uint8_t blocks_to_try[] = {4, 1, 5, 6};
    
    for(int block_idx = 0; block_idx < 4; block_idx++) {
        uint8_t block_num = blocks_to_try[block_idx];
        
        for(int retry = 0; retry < 20; retry++) {
            error = mf_classic_poller_sync_read_block(
                nfc, block_num, &default_key, MfClassicKeyTypeA, &block);
            
            if(error == MfClassicErrorNone) {
                if(block.data[0] == TONUINO_BOX_ID_0 && 
                   block.data[1] == TONUINO_BOX_ID_1 &&
                   block.data[2] == TONUINO_BOX_ID_2 &&
                   block.data[3] == TONUINO_BOX_ID_3) {
                    if(block.data[5] <= 99 && block.data[6] >= 1 && block.data[6] <= ModeRepeatLast) {
                        memcpy(&app->card_data.box_id[0], &block.data[0], 4);
                        app->card_data.version = block.data[4];
                        app->card_data.folder = block.data[5];
                        app->card_data.mode = block.data[6];
                        app->card_data.special1 = block.data[7];
                        app->card_data.special2 = block.data[8];
                        memcpy(&app->card_data.reserved[0], &block.data[9], 7);
                        success = true;
                        break;
                    }
                }
            }
            
            furi_delay_ms(200);
        }
        
        if(success) break;
    }
    
    nfc_free(nfc);
    return success;
}

bool tonuino_write_card(TonuinoApp* app) {
    Nfc* nfc = nfc_alloc();
    if(!nfc) {
        return false;
    }
    
    uint8_t card_data[TONUINO_CARD_SIZE];
    
    card_data[0] = app->card_data.box_id[0];
    card_data[1] = app->card_data.box_id[1];
    card_data[2] = app->card_data.box_id[2];
    card_data[3] = app->card_data.box_id[3];
    card_data[4] = app->card_data.version;
    card_data[5] = app->card_data.folder;
    card_data[6] = app->card_data.mode;
    card_data[7] = app->card_data.special1;
    card_data[8] = app->card_data.special2;
    card_data[9] = 0x00;
    card_data[10] = 0x00;
    card_data[11] = 0x00;
    card_data[12] = 0x00;
    card_data[13] = 0x00;
    card_data[14] = 0x00;
    card_data[15] = 0x00;
    
    MfClassicKey default_key = {.data = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    MfClassicBlock block;
    memcpy(block.data, card_data, TONUINO_CARD_SIZE);
    
    MfClassicError error;
    bool success = false;
    
    for(int retry = 0; retry < 20; retry++) {
        error = mf_classic_poller_sync_write_block(
            nfc, 4, &default_key, MfClassicKeyTypeA, &block);
        
        if(error == MfClassicErrorNone) {
            success = true;
            break;
        }
        
        furi_delay_ms(200);
    }
    
    nfc_free(nfc);
    return success;
}

int32_t tonuino_app(void* p) {
    UNUSED(p);
    
    TonuinoApp* app = tonuino_app_alloc();
    
    view_dispatcher_switch_to_view(app->view_dispatcher, 0);
    view_dispatcher_run(app->view_dispatcher);
    
    tonuino_app_free(app);
    return 0;
}

