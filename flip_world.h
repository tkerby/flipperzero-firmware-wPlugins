#pragma once
#include <font/font.h>
#include <flipper_http/flipper_http.h>
#include <easy_flipper/easy_flipper.h>

// added by Derek Jamison to lower memory usage
#undef FURI_LOG_E
#define FURI_LOG_E(tag, msg, ...)
//

#define TAG "FlipWorld"
#define VERSION 0.3
#define VERSION_TAG "FlipWorld v0.3"

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
    View *view_about;                      // The about screen
    Submenu *submenu;                      // The submenu
    Submenu *submenu_settings;             // The settings submenu
    VariableItemList *variable_item_list;  // The variable item list (settngs)
    VariableItem *variable_item_wifi_ssid; // The variable item for WiFi SSID
    VariableItem *variable_item_wifi_pass; // The variable item for WiFi password
    //
    VariableItem *variable_item_game_fps;              // The variable item for Game FPS
    VariableItem *variable_item_game_screen_always_on; // The variable item for Screen always on
    VariableItem *variable_item_game_download_world;   // The variable item for Download world
    VariableItem *variable_item_game_sound_on;         // The variable item for Sound on
    VariableItem *variable_item_game_vibration_on;     // The variable item for Vibration on
    //
    VariableItem *variable_item_user_username; // The variable item for the User username
    VariableItem *variable_item_user_password; // The variable item for the User password

    UART_TextInput *text_input;      // The text input
    char *text_input_buffer;         // Buffer for the text input
    char *text_input_temp_buffer;    // Temporary buffer for the text input
    uint32_t text_input_buffer_size; // Size of the text input buffer
} FlipWorldApp;

extern char *game_fps_choices[];
extern const float game_fps_choices_2[];
extern int game_fps_index;
extern char *yes_or_no_choices[];
extern int game_screen_always_on_index;
extern int game_sound_on_index;
extern int game_vibration_on_index;
bool is_enough_heap(size_t heap_size);
