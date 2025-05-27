#pragma once

#include "bunnyconnect.h"
#include "lib/bunnyconnect_views.h"
#include "lib/bunnyconnect_keyboard.h"
#include "lib/bunnyconnect_helpers.h"
#include "lib/bunnyconnect_draw.h"

#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/modules/submenu.h>
#include <gui/modules/text_box.h>
#include <gui/modules/text_input.h>
#include <gui/modules/popup.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>
#include <furi_hal_usb_hid.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TAG                  "BunnyConnect"
#define TERMINAL_BUFFER_SIZE 1024
#define INPUT_BUFFER_SIZE    256

typedef enum {
    BunnyConnectViewMainMenu,
    BunnyConnectViewTerminal,
    BunnyConnectViewConfig,
    BunnyConnectViewCustomKeyboard,
    BunnyConnectViewPopup,
} BunnyConnectViewId;

typedef enum {
    BunnyConnectStateDisconnected,
    BunnyConnectStateConnected,
    BunnyConnectStatePopup,
} BunnyConnectState;

typedef enum {
    BunnyConnectCustomEventKeyboardDone,
    BunnyConnectCustomEventRefreshScreen,
    BunnyConnectCustomEventConnect,
    BunnyConnectCustomEventDisconnect,
    BunnyConnectCustomEventConnectionFailed,
    BunnyConnectCustomEventTogglePower,
    BunnyConnectCustomEventConfigSave,
} BunnyConnectCustomEvent;

typedef struct {
    char device_name[32];
    uint32_t baud_rate;
    bool auto_connect;
    bool flow_control;
    uint8_t data_bits;
    uint8_t stop_bits;
    uint8_t parity;
} BunnyConnectConfig;

struct BunnyConnectApp {
    Gui* gui;
    ViewDispatcher* view_dispatcher;

    // Views
    Submenu* main_menu;
    Submenu* config_menu;
    TextBox* text_box;
    BunnyConnectKeyboard* custom_keyboard;
    Popup* popup;

    NotificationApp* notifications;

    // Serial communication
    FuriHalSerialHandle* serial_handle;
    BunnyConnectState state;
    BunnyConnectConfig config;

    // Terminal
    char* terminal_buffer;
    size_t terminal_buffer_position;

    // Threading and synchronization
    FuriThread* worker_thread;
    FuriMutex* rx_mutex;
    FuriMutex* tx_mutex;
    FuriMessageQueue* tx_queue;
    FuriStreamBuffer* rx_stream;
    FuriStreamBuffer* tx_stream;

    // App state
    bool is_running;
    bool usb_power_enabled;
    bool notification_pending;

    FuriString* text_string;
    char input_buffer[INPUT_BUFFER_SIZE];
};

// Function declarations
void bunnyconnect_show_error_popup(BunnyConnectApp* app, const char* message);
bool bunnyconnect_custom_event_callback(void* context, uint32_t event);
bool bunnyconnect_navigation_callback(void* context);

#ifdef __cplusplus
}
#endif
