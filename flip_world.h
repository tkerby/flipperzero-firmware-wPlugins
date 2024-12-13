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

// Screen size
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

// World size (3x3)
#define WORLD_WIDTH 384
#define WORLD_HEIGHT 192

// Define the submenu items for our FlipWorld application
typedef enum
{
    FlipWorldSubmenuIndexRun, // Click to run the FlipWorld application
    FlipWorldSubmenuIndexAbout,
    FlipWorldSubmenuIndexSettings,
} FlipWorldSubmenuIndex;

// Define a single view for our FlipWorld application
typedef enum
{
    FlipWorldViewMain,      // The main screen
    FlipWorldViewSubmenu,   // The submenu
    FlipWorldViewAbout,     // The about screen
    FlipWorldViewSettings,  // The settings screen
    FlipWorldViewTextInput, // The text input screen
} FlipWorldView;

// Define a custom event for our FlipWorld application
typedef enum
{
    FlipWorldCustomEventPlay, // Play the game
} FlipWorldCustomEvent;

// Each screen will have its own view
typedef struct
{
    // necessary
    ViewDispatcher *view_dispatcher;      // Switches between our views
    View *view_main;                      // The game screen
    View *view_about;                     // The about screen
    Submenu *submenu;                     // The submenu
    VariableItemList *variable_item_list; // The variable item list (settngs)
    VariableItem *variable_item_ssid;     // The variable item
    VariableItem *variable_item_pass;     // The variable item

    UART_TextInput *text_input;      // The text input
    char *text_input_buffer;         // Buffer for the text input
    char *text_input_temp_buffer;    // Temporary buffer for the text input
    uint32_t text_input_buffer_size; // Size of the text input buffer
} FlipWorldApp;

// TODO - Add Download world function and download world pack button
