# WiFi Internet Watch for Flipper Zero

Flipper Zero FAP application that uses a Wi-Fi Developer Board to watch internet
availability. The official ESP-AT firmware is supported, and so are the ESP32 devboard
firmwares that expose a serial console (GhostESP, Marauder, Bruce).

> The official devboard ships pre-flashed with the **BlackMagic** debug-probe firmware,
> which has no Wi-Fi console at all. Flash ESP-AT (or GhostESP) onto the board before
> expecting this app to reach the internet.

## Features

- Scans nearby Wi-Fi networks
- Lets you select an SSID and enter its password
- Remembers up to 16 Wi-Fi credentials on the Flipper microSD card
- Lets you update or forget a saved network
- Connects through the devboard firmware selected or auto-detected at startup
- Detects the devboard firmware family automatically: ESP-AT, GhostESP, Marauder, or Bruce
- Checks internet availability every 60 seconds
- Shows `ONLINE`, `OFFLINE`, or `NO WI-FI`
- Keeps the LED solid green while internet is online
- Blinks the LED red with a 1-second on/off cycle while internet is offline
- Plays a notification and retries when Wi-Fi drops
- Uses `8.8.8.8` as the default internet check target
- Lets you change the ping IP, the internet check timer, and the devboard firmware mode from the settings menu
- Lets you reset the IP, timer, and remembered networks from the settings menu
- Uses Back to rescan from monitoring and to exit from the network list

## Hardware

- Flipper Zero
- Official Wi-Fi Developer Board based on ESP32-S2
- microSD card in the Flipper Zero

## Supported devboard firmware

| Firmware | Detection | Wi-Fi scan and join | Internet check |
| --- | --- | --- | --- |
| ESP-AT (official devboard firmware) | `AT` responds with `OK` | AT commands | `AT+PING` |
| GhostESP | console banner mentions GhostESP | `scanaps` / `connect` | `ping` |
| Marauder | console banner mentions Marauder | not supported | not supported |
| Bruce | console banner mentions Bruce | not supported | not supported |
| BlackMagic (factory firmware) | not detectable, select it manually | not supported | not supported |

In `Auto` mode the app probes `AT` first and, when the devboard does not answer, reads the
console banner to identify the firmware family. If detection guesses wrong, open
**Settings -> Devboard** and pin the mode to `ESP-AT`, `GhostESP`, or `Black Magic`.

Marauder, Bruce, and BlackMagic are recognized so the app can tell you that monitoring is
not supported for them instead of silently failing. BlackMagic is the debug probe the
official board ships with, so it exposes no Wi-Fi commands; flash ESP-AT or GhostESP if you
want internet monitoring.

## ESP-AT configuration

The official board UART connected to Flipper uses:

- ESP32-S2 UART TX: GPIO17
- ESP32-S2 UART RX: GPIO18
- Baud rate: 115200
- Hardware flow control: disabled

The application opens `FuriHalSerialIdLpuart`. The factory parameter source used for the
ESP-AT NVS partition is available in `firmware/mfg_nvs_flipper.csv`.

The ESP-AT firmware itself is not redistributed by this repository. Download a compatible
ESP32-S2 ESP-AT release from Espressif and replace its `mfg_nvs` partition with one generated
from the included CSV.

## Build

Install [uFBT](https://github.com/flipperdevices/flipperzero-ufbt), then run:

```sh
ufbt
```

The resulting application is:

```text
dist/wifi_internet_watch.fap
```

Copy it to:

```text
/ext/apps/GPIO/wifi_internet_watch.fap
```

## Usage

1. Install a devboard firmware configured for GPIO17/GPIO18 (ESP-AT is configured this way
   out of the box).
2. Mount the Wi-Fi Developer Board on Flipper Zero.
3. Open **Apps → GPIO → WiFi Internet Watch**.
4. Select a network, enter its password, and wait for the status screen.

Saved networks are marked with `*`. Press **OK** to connect using the saved password,
hold **OK** to edit it, or press **Right** (`Forget`) to delete it. New and updated
passwords are saved only after ESP-AT successfully joins the network.

Press **Left** in the network list to open settings, where you can edit the internet
check IP address or timer, and pick the devboard firmware mode (`Auto`, `ESP-AT`, or
`GhostESP`).

From the settings menu you can also change the internet check timer in seconds or reset
the app back to defaults. Reset clears the saved IP, restores the 60-second timer and the
`Auto` devboard mode, and removes all remembered Wi-Fi networks.

On the monitoring screen, **Back** returns to Wi-Fi scanning instead of closing the app.

The password screen supports lowercase and uppercase letters, digits, spaces, and common
symbols. Select the `Aa`, `#?`, or `ab` key to switch between lowercase, uppercase, and
symbol layouts. Select `OK` on the keyboard to connect, and use **Back** to delete.

Credentials are stored in the application's private data file on the microSD card. They
are not encrypted, so anyone with access to the card can recover them.

## Development

The devboard serial protocol layer in `devboard_protocol.c` has no Flipper SDK dependency,
so it is unit tested on the host:

```sh
make -C tests test
```

## License

MIT
