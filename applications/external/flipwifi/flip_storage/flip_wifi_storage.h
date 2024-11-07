#ifndef FLIP_WIFI_STORAGE_H
#define FLIP_WIFI_STORAGE_H

#include <flip_wifi.h>

// define the paths for all of the FlipperHTTP apps
#define WIFI_SSID_LIST_PATH STORAGE_EXT_PATH_PREFIX "/apps_data/flip_wifi/wifi_list.txt"

extern char* app_ids[8];

// Function to save the playlist
void save_playlist(WiFiPlaylist* playlist);

// Function to load the playlist
bool load_playlist(WiFiPlaylist* playlist);

void save_settings(const char* ssid, const char* password);
#endif
