#ifndef FLIP_WIFI_E_H
#define FLIP_WIFI_E_H

#include <flipper_http/flipper_http.h>
#include <easy_flipper/easy_flipper.h>
#include <storage/storage.h>

#define TAG                "FlipWiFi"
#define MAX_SCAN_NETWORKS  100
#define MAX_SAVED_NETWORKS 25
#define MAX_SSID_LENGTH    64

// Define the submenu items for our FlipWiFi application
typedef enum {
    FlipWiFiSubmenuIndexAbout,
    //
    FlipWiFiSubmenuIndexWiFiScan,
    FlipWiFiSubmenuIndexWiFiSaved,
    FlipWiFiSubmenuIndexCommands,
    //
    FlipWiFiSubmenuIndexWiFiSavedAddSSID,
    //
    FlipWiFiSubmenuIndexFastCommandStart = 50,
    FlipWiFiSubmenuIndexWiFiScanStart = 100,
    FlipWiFiSubmenuIndexWiFiSavedStart = 200,
} FlipWiFiSubmenuIndex;

// Define a single view for our FlipWiFi application
typedef enum {
    FlipWiFiViewWiFiScan, // The view for the wifi scan screen
    FlipWiFiViewWiFiSaved, // The view for the wifi scan screen
    //
    FlipWiFiViewSubmenuMain, // The submenu for the main screen
    FlipWiFiViewSubmenuScan, // The submenu for the wifi scan screen
    FlipWiFiViewSubmenuSaved, // The submenu for the wifi saved screen
    FlipWiFiViewSubmenuCommands, // The submenu for the fast commands screen
    FlipWiFiViewAbout, // The about screen
    FlipWiFiViewTextInputScan, // The text input screen for the wifi scan screen
    FlipWiFiViewTextInputSaved, // The text input screen for the wifi saved screen
    //
    FlipWiFiViewTextInputSavedAddSSID, // The text input screen for the wifi saved screen
    FlipWiFiViewTextInputSavedAddPassword, // The text input screen for the wifi saved screen
    //
    FlipWiFiViewGeneric, // generic view
    FlipWiFiViewSubmenu, // generic submenu
    FlipWiFiViewTextInput, // generic text input
} FlipWiFiView;

// Define the WiFiPlaylist structure
typedef struct {
    char ssids[MAX_SAVED_NETWORKS][MAX_SSID_LENGTH];
    char passwords[MAX_SAVED_NETWORKS][MAX_SSID_LENGTH];
    size_t count;
} WiFiPlaylist;

// Each screen will have its own view
typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views
    Widget* widget_info; // The widget for the about screen
    View* view_wifi; // generic view for the wifi scan and saved screens
    Submenu* submenu_main; // The submenu for the main screen
    Submenu* submenu_wifi; // generic submenu for the wifi scan and saved screens
    VariableItemList* variable_item_list_wifi; // The variable item list (settngs)
    VariableItem* variable_item_ssid; // The variable item
    UART_TextInput* uart_text_input; // The text input screen
    char* uart_text_input_buffer; // Buffer for the text input
    char* uart_text_input_temp_buffer; // Temporary buffer for the text input
    uint32_t uart_text_input_buffer_size; // Size of the text input buffer
} FlipWiFiApp;

// Function to free the resources used by FlipWiFiApp
void flip_wifi_app_free(FlipWiFiApp* app);
extern WiFiPlaylist* wifi_playlist; // The playlist of wifi networks

#endif // FLIP_WIFI_E_H
