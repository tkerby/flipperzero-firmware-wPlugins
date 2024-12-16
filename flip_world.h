#pragma once
#include <font/font.h>
#include <flipper_http/flipper_http.h>
#include <easy_flipper/easy_flipper.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <gui/view.h>
#include <gui/modules/submenu.h>
#include <gui/view_dispatcher.h>
#include <notification/notification.h>
#include <dialogs/dialogs.h>

#define TAG "FlipWorld"
#define VERSION_TAG "FlipWorld v0.1"

// Define the submenu items for our FlipWorld application
typedef enum
{
    FlipWorldSubmenuIndexRun, // Click to run the FlipWorld application
    FlipWorldSubmenuIndexAbout,
    FlipWorldSubmenuIndexSettings,
    FlipWorldSubmenuIndexWiFiSettings,
    FlipWorldSubmenuIndexGameSettings,
    FlipWorldSubmenuIndexUserSettings,
} FlipWorldSubmenuIndex;

// Define a single view for our FlipWorld application
typedef enum
{
    FlipWorldViewMain,             // The main screen
    FlipWorldViewSubmenu,          // The submenu
    FlipWorldViewAbout,            // The about screen
    FlipWorldViewSettings,         // The settings screen
    FlipWorldViewVariableItemList, // The variable item list screen
    FlipWorldViewTextInput,        // The text input screen
    //
    FlipWorldViewWidgetResult, // The text box that displays the random fact
    FlipWorldViewLoader,       // The loader screen retrieves data from the internet
} FlipWorldView;

// Define a custom event for our FlipWorld application
typedef enum
{
    FlipWorldCustomEventProcess,
    FlipWorldCustomEventPlay, // Play the game
} FlipWorldCustomEvent;

// Each screen will have its own view
typedef struct
{
    View *view_loader;
    Widget *widget_result;
    //
    ViewDispatcher *view_dispatcher;       // Switches between our views
    View *view_main;                       // The game screen
    View *view_about;                      // The about screen
    Submenu *submenu;                      // The submenu
    Submenu *submenu_settings;             // The settings submenu
    VariableItemList *variable_item_list;  // The variable item list (settngs)
    VariableItem *variable_item_wifi_ssid; // The variable item for WiFi SSID
    VariableItem *variable_item_wifi_pass; // The variable item for WiFi password
    //
    VariableItem *variable_item_game_fps;            // The variable item for Game FPS
    VariableItem *variable_item_game_download_world; // The variable item for Download world
    //
    VariableItem *variable_item_user_username; // The variable item for the User username
    VariableItem *variable_item_user_password; // The variable item for the User password

    UART_TextInput *text_input;      // The text input
    char *text_input_buffer;         // Buffer for the text input
    char *text_input_temp_buffer;    // Temporary buffer for the text input
    uint32_t text_input_buffer_size; // Size of the text input buffer

} FlipWorldApp;

extern char *game_fps_choices[];
extern char *game_fps; // The game FPS
// TODO - Add Download world function and download world pack button
