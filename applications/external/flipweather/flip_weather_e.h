#ifndef FLIP_WEATHER_E_H
#define FLIP_WEATHER_E_H

#include <flipper_http.h>
#include <easy_flipper.h>
#include <jsmn.h>

#define TAG "FlipWeather"

// Define the submenu items for our FlipWeather application
typedef enum {
    FlipWeatherSubmenuIndexWeather, // Click to view the weather
    FlipWeatherSubmenuIndexGPS, // Click to view the GPS
    FlipWeatherSubmenuIndexAbout, // Click to view the about screen
    FlipWeatherSubmenuIndexSettings, // Click to view the settings screen
} FlipWeatherSubmenuIndex;

// Define a single view for our FlipWeather application
typedef enum {
    FlipWeatherViewWeather, // The weather screen
    FlipWeatherViewGPS, // The GPS screen
    FlipWeatherViewSubmenu, // The main submenu
    FlipWeatherViewAbout, // The about screen
    FlipWeatherViewSettings, // The wifi settings screen
    FlipWeatherViewTextInputSSID, // The text input screen for SSID
    FlipWeatherViewTextInputPassword, // The text input screen for password
} FlipWeatherView;

// Each screen will have its own view
typedef struct {
    ViewDispatcher* view_dispatcher; // Switches between our views
    View* view_weather; // The weather view
    View* view_gps; // The GPS view
    Submenu* submenu; // The main submenu
    Widget* widget; // The widget (about)
    VariableItemList* variable_item_list; // The variable item list (settngs)
    VariableItem* variable_item_ssid; // The variable item
    VariableItem* variable_item_password; // The variable item
    UART_TextInput* uart_text_input_ssid; // The text input
    UART_TextInput* uart_text_input_password; // The text input

    char* uart_text_input_buffer_ssid; // Buffer for the text input
    char* uart_text_input_temp_buffer_ssid; // Temporary buffer for the text input
    uint32_t uart_text_input_buffer_size_ssid; // Size of the text input buffer

    char* uart_text_input_buffer_password; // Buffer for the text input
    char* uart_text_input_temp_buffer_password; // Temporary buffer for the text input
    uint32_t uart_text_input_buffer_size_password; // Size of the text input buffer
} FlipWeatherApp;

#endif
