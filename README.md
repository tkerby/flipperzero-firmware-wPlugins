# Smart Home for Flipper Zero

Smart Home is a Flipper Zero application for controlling smart home devices through Home Assistant using a Wi-Fi Developer Board compatible with FlipperHTTP.

## Features

- Direct Home Assistant REST API control
- Persistent device and settings storage on the Flipper SD card
- Home Assistant `switch.*` entities with On, Off, and Toggle
- Home Assistant `climate.*` entities with status, temperature, HVAC mode, and fan controls
- Home Assistant `light.*` entities with Status, On, Off, Toggle, brightness presets, RGB colors, and effects
- Rename and remove saved devices
- Custom on-device Home Assistant URL keyboard with URL punctuation
- Editable Home Assistant long-lived access token
- Optional SD-card Home Assistant configuration import

## Requirements

- Flipper Zero
- Wi-Fi Developer Board compatible with FlipperHTTP
- FlipperHTTP firmware/library
- A reachable Home Assistant server
- A Home Assistant long-lived access token

Home Assistant is required. Smart Home communicates with Home Assistant directly; no companion PC relay is required.

## First-Time Setup

1. Install `smart_home.fap` on the Flipper Zero.
2. Connect the Wi-Fi Developer Board.
3. Open **Smart Home**.
4. Open **Settings**.
5. Configure **Home Assistant URL**.
6. Configure **Edit HA Token**, or import the settings from the SD card.
7. Return to the main menu.
8. Select **+ Add Device**.
9. Choose the desired Home Assistant device type.

A new URL entry starts with:

    https://

Enter the normal Home Assistant base URL, for example:

    https://ha.example.com

or a local address such as:

    http://192.168.1.50:8123

Use a trusted HTTPS certificate for HTTPS connections.

## SD-Card Configuration Import

For long tokens, it can be easier to prepare a small configuration file on a computer and copy it to the Flipper SD card.

Create:

    /ext/apps_data/smart_home/ha_config.txt

with:

    ha_url=https://ha.example.com
    ha_token=PASTE_YOUR_HOME_ASSISTANT_LONG_LIVED_ACCESS_TOKEN_HERE

Then select:

    Settings -> Import HA Config

The import is explicit; the application does not automatically re-import the file on every launch.

Treat the Flipper SD card as sensitive because Home Assistant credentials are stored locally.

## Adding a Home Assistant Switch

1. Select **+ Add Device**.
2. Choose **Home Assistant Switch**.
3. Enter a local display name.
4. Enter the Home Assistant `switch.*` entity ID.

Example:

    switch.smart_plug

Supported actions:

- Turn ON
- Turn OFF
- Toggle
- Rename
- Delete

## Adding a Home Assistant Light

1. Select **+ Add Device**.
2. Choose **Home Assistant Light**.
3. Enter a local display name.
4. Enter the Home Assistant `light.*` entity ID.

Example:

    light.monitor_backlight

Supported actions:

- Status
- Turn ON
- Turn OFF
- Toggle
- Brightness 25%
- Brightness 50%
- Brightness 75%
- Brightness 100%
- **Solid** submenu with RGB color presets
- **Effects** submenu

The Status screen shows the entity state, brightness, and active effect when Home Assistant provides them.

The Effects menu includes the WLED effect set used by current WLED integrations. A selected effect only works when the target Home Assistant light entity supports that effect. RGB colors likewise require an RGB-capable light.

## Adding a Home Assistant Thermostat

1. Select **+ Add Device**.
2. Choose **Home Assistant Thermostat**.
3. Enter a local display name.
4. Enter the Home Assistant `climate.*` entity ID.

Example:

    climate.living_room

Supported actions:

- Status
- Temperature +1
- Temperature -1
- Set Temperature
- Cool
- Heat
- Auto / Heat-Cool
- Off
- Fan Auto
- Fan On
- Rename
- Delete

Available HVAC and fan modes depend on the selected Home Assistant entity.

## Configuration Storage

Smart Home stores application data under:

    /ext/apps_data/smart_home/

Current files include:

- `devices.txt`
- `settings.txt`
- optional `ha_config.txt`

`settings.txt` contains the configured Home Assistant URL and token. Backups of the app-data folder therefore contain credentials and should be protected accordingly.

## Building

Install uFBT, then from this directory run:

    ufbt

The resulting application is created at:

    dist/smart_home.fap

To install and launch directly on a connected Flipper Zero:

    ufbt launch

## Project Structure

    application.fam
    smart_home.c
    smart_home_10px.png
    flipper_http/
        flipper_http.c
        flipper_http.h

## Third-Party Components

This project includes the MIT-licensed FlipperHTTP library by JBlanked.

See `THIRD_PARTY_NOTICES.md` for attribution and upstream project information.

## Version

Current release: 2.0

## Author

Vincenzo
