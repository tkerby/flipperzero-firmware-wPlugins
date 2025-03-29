FlipWiFi is the companion app for the popular FlipperHTTP flash, originally introduced in the https://github.com/jblanked/WebCrawler-FlipperZero/tree/main/assets/FlipperHTTP. It allows you to scan and save WiFi networks for use across all FlipperHTTP apps.

## Requirements

- WiFi Developer Board, Raspberry Pi, or ESP32 Device flashed with FlipperHTTP v1.8.2 or higher: https://github.com/jblanked/FlipperHTTP
- 2.4 GHz WiFi Access Point

## Features

- **Scan**: Discover nearby WiFi networks and add them to your list.
- **Saved Access Points**: View your saved networks, manually add new ones, or configure the WiFi network to be used across all FlipperHTTP apps.

## Setup

FlipWiFi automatically allocates the necessary resources and initializes settings upon launch. If WiFi settings have been previously configured, they are loaded automatically for easy access. You can also edit the list of WiFi settings by downloading and modifying the "wifi_list.txt" file located in the "/SD/apps_data/flip_wifi/data" directory. To use the app:

1. **Flash the WiFi Developer Board**: Follow the instructions to flash the WiFi Dev Board with FlipperHTTP: https://github.com/jblanked/FlipperHTTP
2. **Install the App**: Download FlipWiFi from the Flipper Lab.
3. **Launch FlipWiFi**: Open the app on your Flipper Zero.
4. Connect, review, and save WiFi networks.