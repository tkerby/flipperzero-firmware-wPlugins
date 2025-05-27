#include "bunnyconnect_i.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/popup.h>
#include <notification/notification_messages.h>
#include <furi_hal_usb_hid.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

typedef enum {
    BunnyConnectSubmenuIndexConnect,
    BunnyConnectSubmenuIndexTerminal,
    BunnyConnectSubmenuIndexKeyboard,
    BunnyConnectSubmenuIndexConfig,
    BunnyConnectSubmenuIndexExit,
} BunnyConnectSubmenuIndex;

static void bunnyconnect_submenu_callback(void* context, uint32_t index) {
    BunnyConnectApp* app = context;
    if(!app || !app->view_dispatcher) return;

    switch(index) {
    case BunnyConnectSubmenuIndexConnect:
        view_dispatcher_send_custom_event(app->view_dispatcher, BunnyConnectCustomEventConnect);
        break;
    case BunnyConnectSubmenuIndexTerminal:
        if(app->text_box) {
            view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewTerminal);
        }
        break;
    case BunnyConnectSubmenuIndexKeyboard:
        if(app->custom_keyboard) {
            view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewCustomKeyboard);
        }
        break;
    case BunnyConnectSubmenuIndexConfig:
        if(app->config_menu) {
            view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewConfig);
        }
        break;
    case BunnyConnectSubmenuIndexExit:
        view_dispatcher_stop(app->view_dispatcher);
        break;
    }
}

static void bunnyconnect_keyboard_callback(void* context) {
    BunnyConnectApp* app = context;
    if(!app || !app->view_dispatcher) return;

    view_dispatcher_send_custom_event(app->view_dispatcher, BunnyConnectCustomEventKeyboardDone);
}

bool bunnyconnect_custom_event_callback(void* context, uint32_t event) {
    BunnyConnectApp* app = context;
    if(!app) return false;

    switch(event) {
    case BunnyConnectCustomEventConnect:
        FURI_LOG_I(TAG, "Connect event");
        app->state = BunnyConnectStateConnected;
        if(app->text_box) {
            view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewTerminal);
        }
        return true;

    case BunnyConnectCustomEventDisconnect:
        FURI_LOG_I(TAG, "Disconnect event");
        app->state = BunnyConnectStateDisconnected;
        return true;

    case BunnyConnectCustomEventKeyboardDone:
        FURI_LOG_I(TAG, "Keyboard done event");
        if(app->input_buffer[0] != '\0') {
            // Send the text via HID
            for(size_t i = 0; i < strlen(app->input_buffer); i++) {
                uint16_t key = HID_ASCII_TO_KEY(app->input_buffer[i]);
                if(key != HID_KEYBOARD_NONE) {
                    furi_hal_hid_kb_press(key);
                    furi_hal_hid_kb_release(key);
                }
            }
            // Clear buffer
            memset(app->input_buffer, 0, INPUT_BUFFER_SIZE);
        }
        if(app->text_box) {
            view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewTerminal);
        }
        return true;

    case BunnyConnectCustomEventRefreshScreen:
        FURI_LOG_D(TAG, "Refresh screen event");
        if(app->text_box && app->terminal_buffer) {
            text_box_set_text(app->text_box, app->terminal_buffer);
            text_box_set_focus(app->text_box, TextBoxFocusEnd);
        }
        return true;

    default:
        return false;
    }
}

bool bunnyconnect_navigation_callback(void* context) {
    BunnyConnectApp* app = context;
    UNUSED(app);

    // Allow back navigation to exit
    return false;
}

static bool bunnyconnect_setup_views(BunnyConnectApp* app) {
    if(!app || !app->view_dispatcher) return false;

    // Allocate terminal buffer
    app->terminal_buffer = malloc(TERMINAL_BUFFER_SIZE);
    if(!app->terminal_buffer) {
        FURI_LOG_E(TAG, "Failed to allocate terminal buffer");
        return false;
    }
    memset(app->terminal_buffer, 0, TERMINAL_BUFFER_SIZE);
    strcpy(app->terminal_buffer, "BunnyConnect Terminal\nReady...\n");

    // Main menu
    app->main_menu = submenu_alloc();
    if(!app->main_menu) {
        FURI_LOG_E(TAG, "Failed to allocate main menu");
        return false;
    }

    submenu_add_item(
        app->main_menu,
        "Connect",
        BunnyConnectSubmenuIndexConnect,
        bunnyconnect_submenu_callback,
        app);
    submenu_add_item(
        app->main_menu,
        "Terminal",
        BunnyConnectSubmenuIndexTerminal,
        bunnyconnect_submenu_callback,
        app);
    submenu_add_item(
        app->main_menu,
        "Keyboard",
        BunnyConnectSubmenuIndexKeyboard,
        bunnyconnect_submenu_callback,
        app);
    submenu_add_item(
        app->main_menu,
        "Config",
        BunnyConnectSubmenuIndexConfig,
        bunnyconnect_submenu_callback,
        app);
    submenu_add_item(
        app->main_menu, "Exit", BunnyConnectSubmenuIndexExit, bunnyconnect_submenu_callback, app);

    view_dispatcher_add_view(
        app->view_dispatcher, BunnyConnectViewMainMenu, submenu_get_view(app->main_menu));

    // Terminal
    app->text_box = text_box_alloc();
    if(!app->text_box) {
        FURI_LOG_E(TAG, "Failed to allocate text box");
        return false;
    }
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->terminal_buffer);
    view_dispatcher_add_view(
        app->view_dispatcher, BunnyConnectViewTerminal, text_box_get_view(app->text_box));

    // Config menu
    app->config_menu = submenu_alloc();
    if(!app->config_menu) {
        FURI_LOG_E(TAG, "Failed to allocate config menu");
        return false;
    }
    submenu_add_item(app->config_menu, "Baud Rate: 115200", 0, NULL, app);
    submenu_add_item(app->config_menu, "Flow Control: Off", 1, NULL, app);
    view_dispatcher_add_view(
        app->view_dispatcher, BunnyConnectViewConfig, submenu_get_view(app->config_menu));

    // Custom keyboard
    app->custom_keyboard = bunnyconnect_keyboard_alloc();
    if(!app->custom_keyboard) {
        FURI_LOG_E(TAG, "Failed to allocate custom keyboard");
        return false;
    }
    bunnyconnect_keyboard_set_header_text(app->custom_keyboard, "Enter command:");
    bunnyconnect_keyboard_set_result_callback(
        app->custom_keyboard,
        bunnyconnect_keyboard_callback,
        app,
        app->input_buffer,
        INPUT_BUFFER_SIZE,
        true);
    view_dispatcher_add_view(
        app->view_dispatcher,
        BunnyConnectViewCustomKeyboard,
        bunnyconnect_keyboard_get_view(app->custom_keyboard));

    // Popup
    app->popup = popup_alloc();
    if(!app->popup) {
        FURI_LOG_E(TAG, "Failed to allocate popup");
        return false;
    }
    view_dispatcher_add_view(
        app->view_dispatcher, BunnyConnectViewPopup, popup_get_view(app->popup));

    FURI_LOG_I(TAG, "Views setup successfully");
    return true;
}

int32_t bunnyconnect_app_run(BunnyConnectApp* app) {
    if(!app) {
        FURI_LOG_E(TAG, "App is NULL");
        return -1;
    }

    FURI_LOG_I(TAG, "Starting app");

    if(!bunnyconnect_setup_views(app)) {
        FURI_LOG_E(TAG, "Failed to setup views");
        return -1;
    }

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bunnyconnect_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bunnyconnect_navigation_callback);

    view_dispatcher_switch_to_view(app->view_dispatcher, BunnyConnectViewMainMenu);

    FURI_LOG_I(TAG, "Running view dispatcher");
    view_dispatcher_run(app->view_dispatcher);

    FURI_LOG_I(TAG, "App finished");
    return 0;
}

int32_t bunnyconnect_app(void* p) {
    UNUSED(p);

    FURI_LOG_I(TAG, "BunnyConnect starting");

    BunnyConnectApp* app = bunnyconnect_app_alloc();
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app");
        return -1;
    }

    int32_t result = bunnyconnect_app_run(app);
    bunnyconnect_app_free(app);

    FURI_LOG_I(TAG, "BunnyConnect finished with result: %ld", result);
    return result;
}
