#include "bunnyconnect_i.h"
#include <furi.h>
#include <gui/gui.h>
#include <gui/icon_i.h>
#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

static bool bunnyconnect_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    BunnyConnectApp* app = context;

    // Simple event handling without scene manager for now
    switch(event) {
    case BunnyConnectCustomEventConnect:
        FURI_LOG_D(TAG, "Connect event received for app: %p", (void*)app);
        break;
    case BunnyConnectCustomEventDisconnect:
        FURI_LOG_D(TAG, "Disconnect event received for app: %p", (void*)app);
        break;
    default:
        FURI_LOG_D(TAG, "Unknown event: %lu for app: %p", event, (void*)app);
        break;
    }

    return true;
}

static bool bunnyconnect_app_back_event_callback(void* context) {
    furi_assert(context);
    UNUSED(context);

    // Allow back navigation - return false to exit
    return false;
}

BunnyConnectApp* bunnyconnect_app_alloc(void) {
    BunnyConnectApp* app = malloc(sizeof(BunnyConnectApp));
    if(!app) {
        FURI_LOG_E(TAG, "Failed to allocate app");
        return NULL;
    }

    // Initialize all pointers to NULL first
    memset(app, 0, sizeof(BunnyConnectApp));

    // Initialize config with defaults
    strncpy(app->config.device_name, "BunnyConnect", sizeof(app->config.device_name) - 1);
    app->config.device_name[sizeof(app->config.device_name) - 1] = '\0';
    app->config.baud_rate = 115200;
    app->config.auto_connect = false;
    app->config.flow_control = false;
    app->config.data_bits = 8;
    app->config.stop_bits = 1;
    app->config.parity = 0;
    app->state = BunnyConnectStateDisconnected;

    // Open records first
    app->gui = furi_record_open(RECORD_GUI);
    if(!app->gui) {
        FURI_LOG_E(TAG, "Failed to open GUI record");
        bunnyconnect_app_free(app);
        return NULL;
    }

    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    if(!app->notifications) {
        FURI_LOG_E(TAG, "Failed to open notification record");
        bunnyconnect_app_free(app);
        return NULL;
    }

    // Allocate view dispatcher
    app->view_dispatcher = view_dispatcher_alloc();
    if(!app->view_dispatcher) {
        FURI_LOG_E(TAG, "Failed to allocate view dispatcher");
        bunnyconnect_app_free(app);
        return NULL;
    }

    // Allocate text string
    app->text_string = furi_string_alloc();
    if(!app->text_string) {
        FURI_LOG_E(TAG, "Failed to allocate text string");
        bunnyconnect_app_free(app);
        return NULL;
    }

    // Set up view dispatcher callbacks
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, bunnyconnect_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, bunnyconnect_app_back_event_callback);

    // Attach to GUI
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    FURI_LOG_I(TAG, "App allocated successfully");
    return app;
}

void bunnyconnect_app_free(BunnyConnectApp* app) {
    if(!app) return;

    FURI_LOG_I(TAG, "Freeing app resources");

    // Stop worker thread if running
    if(app->worker_thread) {
        app->is_running = false;
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }

    // Free stream buffers
    if(app->rx_stream) {
        furi_stream_buffer_free(app->rx_stream);
        app->rx_stream = NULL;
    }
    if(app->tx_stream) {
        furi_stream_buffer_free(app->tx_stream);
        app->tx_stream = NULL;
    }

    // Free GUI elements - remove views first, then free them
    if(app->view_dispatcher) {
        if(app->main_menu) {
            view_dispatcher_remove_view(app->view_dispatcher, BunnyConnectViewMainMenu);
        }
        if(app->text_box) {
            view_dispatcher_remove_view(app->view_dispatcher, BunnyConnectViewTerminal);
        }
        if(app->config_menu) {
            view_dispatcher_remove_view(app->view_dispatcher, BunnyConnectViewConfig);
        }
        if(app->custom_keyboard) {
            view_dispatcher_remove_view(app->view_dispatcher, BunnyConnectViewCustomKeyboard);
        }
        if(app->popup) {
            view_dispatcher_remove_view(app->view_dispatcher, BunnyConnectViewPopup);
        }
        view_dispatcher_free(app->view_dispatcher);
        app->view_dispatcher = NULL;
    }

    // Free views
    if(app->main_menu) {
        submenu_free(app->main_menu);
        app->main_menu = NULL;
    }
    if(app->text_box) {
        text_box_free(app->text_box);
        app->text_box = NULL;
    }
    if(app->config_menu) {
        submenu_free(app->config_menu);
        app->config_menu = NULL;
    }
    if(app->custom_keyboard) {
        bunnyconnect_keyboard_free(app->custom_keyboard);
        app->custom_keyboard = NULL;
    }
    if(app->popup) {
        popup_free(app->popup);
        app->popup = NULL;
    }

    // Free terminal buffer
    if(app->terminal_buffer) {
        free(app->terminal_buffer);
        app->terminal_buffer = NULL;
    }

    // Free mutexes
    if(app->rx_mutex) {
        furi_mutex_free(app->rx_mutex);
        app->rx_mutex = NULL;
    }
    if(app->tx_mutex) {
        furi_mutex_free(app->tx_mutex);
        app->tx_mutex = NULL;
    }

    // Free message queue
    if(app->tx_queue) {
        furi_message_queue_free(app->tx_queue);
        app->tx_queue = NULL;
    }

    // Free strings
    if(app->text_string) {
        furi_string_free(app->text_string);
        app->text_string = NULL;
    }

    // Close records
    if(app->gui) {
        furi_record_close(RECORD_GUI);
        app->gui = NULL;
    }
    if(app->notifications) {
        furi_record_close(RECORD_NOTIFICATION);
        app->notifications = NULL;
    }

    free(app);
}

void bunnyconnect_show_error_popup(BunnyConnectApp* app, const char* message) {
    if(!app || !message) return;

    FURI_LOG_E(TAG, "Error: %s", message);

    // For now, just log the error since we don't have popup set up yet
    // TODO: Implement popup display when views are properly initialized
}
