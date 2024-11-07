#ifndef FLIP_WIFI_CALLBACK_H
#define FLIP_WIFI_CALLBACK_H

#include <flip_wifi.h>
#include <flip_storage/flip_wifi_storage.h>
#include <flip_wifi_icons.h>

// array to store each SSID
extern char* ssid_list[64];
extern uint32_t ssid_index;

void flip_wifi_redraw_submenu_saved(FlipWiFiApp* app);

uint32_t callback_to_submenu_main(void* context);

uint32_t callback_to_submenu_scan(void* context);

uint32_t callback_to_submenu_saved(void* context);

void popup_callback_saved(void* context);

void popup_callback_main(void* context);

// Callback for drawing the main screen
void flip_wifi_view_draw_callback_scan(Canvas* canvas, void* model);

void flip_wifi_view_draw_callback_saved(Canvas* canvas, void* model);

// Input callback for the view (async input handling)
bool flip_wifi_view_input_callback_scan(InputEvent* event, void* context);

// Input callback for the view (async input handling)
bool flip_wifi_view_input_callback_saved(InputEvent* event, void* context);

// Function to trim leading and trailing whitespace
// Returns the trimmed start pointer and updates the length
char* trim_whitespace(char* start, size_t* length);

bool flip_wifi_handle_scan(FlipWiFiApp* app);

void callback_submenu_choices(void* context, uint32_t index);

void flip_wifi_text_updated_password_scan(void* context);

void flip_wifi_text_updated_password_saved(void* context);

void flip_wifi_text_updated_add_ssid(void* context);

void flip_wifi_text_updated_add_password(void* context);

#endif // FLIP_WIFI_CALLBACK_H
