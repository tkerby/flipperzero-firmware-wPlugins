# Smart Home

Smart Home lets you control Home Assistant devices from your Flipper Zero using a Wi-Fi Developer Board compatible with FlipperHTTP.

## Features

- Direct Home Assistant REST API control
- Persistent devices and settings
- Home Assistant switch.* support
- Home Assistant climate.* support
- Home Assistant light.* support
- Switch On, Off, and Toggle actions
- Light Status, On, Off, Toggle, brightness, RGB color, and effect controls
- Thermostat status and temperature controls
- HVAC and fan mode controls
- Custom Home Assistant URL entry
- Editable Home Assistant token
- Optional SD-card configuration import
- Rename and remove saved devices

## Requirements

- Flipper Zero
- Compatible FlipperHTTP Wi-Fi Developer Board
- Reachable Home Assistant server
- Home Assistant long-lived access token

Home Assistant is required. No companion PC relay is required.

## Home Assistant Switches

Add a switch using its switch.* entity ID, for example:

- switch.smart_plug

Supported actions include On, Off, Toggle, Rename, and Delete.

## Home Assistant Lights

Add a light using its light.* entity ID, for example:

- light.monitor_backlight

Supported actions include Status, On, Off, Toggle, brightness presets, a Solid RGB color submenu, Rename, and Delete.

The Effects submenu includes the WLED effect set used by current WLED integrations. Effects and RGB colors depend on capabilities exposed by the selected Home Assistant light entity.

## Home Assistant Climate / Thermostat

Add a thermostat using its climate.* entity ID, for example:

- climate.living_room

Supported actions include Status, temperature adjustment, direct target temperature, HVAC modes, fan modes, Rename, and Delete.

Available actions ultimately depend on the capabilities exposed by the selected Home Assistant entity.

## Credentials

The Home Assistant URL and long-lived token are stored in the Smart Home app-data folder on the Flipper SD card. Treat that storage and any backups as sensitive.
