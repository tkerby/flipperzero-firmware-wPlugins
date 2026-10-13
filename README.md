# BLE Beacon Simulator for Flipper Zero

**BLE Beacon Simulator** is a Flipper Zero application that turns the device into a configurable Bluetooth Low Energy (BLE) beacon for testing BLE scanners, smart-home integrations, and mobile applications.

The app was originally created as a second test sensor for debugging [BTHome Logger](https://github.com/steanlab/BTHome-Logger), an Android application that decodes BTHome v1 and v2 advertisements into human-readable readings. Instead of buying a second physical thermometer, the Flipper Zero can emulate one.

It has since grown into a more general BLE beacon simulator. It is useful for developers, makers, home-automation enthusiasts, firmware testers, and anyone working with BLE sensors, smart-home integrations, or Bluetooth advertising formats.

## What it does

The application broadcasts unencrypted BLE advertisements from the Flipper Zero. You can switch between several common beacon formats and inspect live Flipper telemetry on the screen.

Supported beacon types:

- **BTHome V2 Flipper**
- **BTHome V1 Flipper**
- **iBeacon Flipper**
- **Eddystone TLM Flipper**
- **ATC1441 Flipper**
- **MiBeacon Flipper**

The device advertises with the local name `Flipper` where the selected advertising format has enough space for it.

## Why it is useful

Testing a BTHome receiver, Android widget, Home Assistant integration, or BLE scanner usually requires a real sensor that continuously broadcasts temperature, humidity, battery, and voltage data.

This app provides a convenient software-based test transmitter:

- Test a BTHome decoder without a second hardware sensor.
- Generate repeatable BLE advertisements for mobile-app debugging.
- Check how a scanner handles different beacon formats.
- Simulate a temperature and humidity sensor for smart-home testing.
- Verify that a widget or logger correctly updates when new advertisements arrive.
- Experiment with BTHome v1, BTHome v2, iBeacon, Eddystone TLM, ATC1441, and MiBeacon formats.

## Real and simulated data

The Flipper Zero provides real battery and power-management telemetry. Some fields required by certain beacon formats are not physically available on the device, so they are clearly simulated.

| Data | Source |
|---|---|
| Battery level | Real Flipper battery percentage |
| Battery voltage | Real Fuel Gauge measurement |
| Battery current | Real Fuel Gauge measurement |
| Battery temperature | Real Fuel Gauge temperature |
| Charger / board temperature | Real charger IC temperature |
| Humidity | Simulated: 42–48% |
| iBeacon UUID | Static test UUID |
| iBeacon major / minor | Static test values |
| iBeacon measured RSSI | Static test value: -59 dBm |
| Advertising counter | Incremented on every update |
| Uptime | Incremented on every update |

The simulated humidity changes slightly between advertising updates, which makes it useful for checking that a receiver updates its UI correctly.

## Beacon formats

### BTHome V2

Broadcasts an unencrypted BTHome V2 advertisement using service UUID `0xFCD2`.

Included data:

- Battery level
- Battery voltage
- Battery current
- Battery temperature
- Charger temperature

This is the recommended format for testing [BTHome Logger](https://github.com/steanlab/BTHome-Logger) and Home Assistant-style BTHome integrations.

### BTHome V1

Broadcasts an unencrypted BTHome V1 advertisement using service UUID `0x181C`.

It contains the same telemetry fields as BTHome V2 and is useful for testing older BTHome receivers and custom parsers.

### iBeacon

Broadcasts an Apple iBeacon advertisement with:

- Proximity UUID: `e2c56db5-dffb-48d2-b060-d0f5a71096e6`
- Major: `1`
- Minor: `1`
- Measured RSSI at 1 m: `-59 dBm`

The iBeacon format has a fixed structure and occupies nearly the entire 31-byte BLE advertising payload limit. Therefore, it does not include the `Flipper` local name.

### Eddystone TLM

Broadcasts an Eddystone Telemetry frame using the Eddystone service UUID `0xFEAA`.

Included data:

- Battery voltage
- Battery temperature
- Advertising count
- Uptime in seconds

### ATC1441

Emulates the advertising format used by ATC1441 / PVVX-flashed Xiaomi thermometer sensors.

Included data:

- Temperature
- Simulated humidity
- Battery level
- Battery voltage
- Advertising counter

This format is useful for testing receivers intended for custom Xiaomi thermometer firmware.

### MiBeacon

Emulates a simplified unencrypted Xiaomi MiBeacon advertisement.

Included data:

- Temperature
- Simulated humidity
- Battery level

The product ID is intended to be replaced with the ID of a real Xiaomi device when full receiver compatibility is required.

## User interface

The main screen shows the selected beacon type, live telemetry, and the advertising status.

```text
        BLE Beacon
 < Type: BTHome V2 Flipper >
  V:4100mV 98% T:27C
 Unencrypted BLE beacon

[Info]    (Start)    [Help]
```

When the beacon is active, an `ADV` indicator and a blinking Bluetooth symbol appear at the top of the screen.

```text
 >- ADV
        BLE Beacon
 < Type: BTHome V2 Flipper >
  V:4100mV 98% T:27C
 Unencrypted BLE beacon

[Info]     (Stop)     [Help]
```

## Controls

| Button | Action |
|---|---|
| Left | Previous beacon type |
| Right | Next beacon type |
| Up, short press | Select **Info** |
| Down, short press | Select **Help** |
| Up, long press | Open the Info screen |
| Down, long press | Open the Help screen |
| OK | Start or stop the beacon; opens Info or Help if selected |
| Back | Open the exit confirmation screen |

The beacon is not started automatically. Press `OK` to begin advertising.

## Info screen

The Info screen explains which values are simulated and which values come from the Flipper hardware.

It includes:

- Simulated humidity range
- Static iBeacon UUID, major, minor, and RSSI values
- List of real Flipper telemetry values
- BLE local name

Use `Up` and `Down` to scroll through the text. Press `OK` or `Back` to return to the main screen.

## Help screen

The Help screen contains a short reference for all controls.

Use `Up` and `Down` to scroll through the text. Press `OK` or `Back` to return to the main screen.

## Exit screen

Pressing `Back` on the main screen opens an exit confirmation dialog:

```text
          Exit?

 [ No ]           [ Yes ]
```

- `Left` selects **No**
- `Right` selects **Yes**
- `OK` confirms the selection

If you select **Yes**, the app shows a short exit splash screen:

```text
┌──────────────────────────────┐
│                              │
│         BLE Beacon           │
│       Simulator v1.0         │
│                              │
│   LAB-66 (C) Siarhei Besarab │
│                              │
└──────────────────────────────┘
```

After the splash screen, the application stops BLE advertising, releases all resources, and exits cleanly.

## Technical details

- The application uses the Flipper Zero extra BLE beacon API.
- Advertising interval: 1000 ms.
- Advertising channels: 37, 38, and 39.
- Address type: random static address.
- Advertising update interval: 2000 ms.
- All supported advertisements are unencrypted.
- The BLE advertising payload is limited to 31 bytes, which determines which fields and local names fit into each beacon format.

BTHome is an open standard for broadcasting sensor data over Bluetooth LE. BTHome v2 uses service UUID `0xFCD2`, while BTHome v1 uses `0x181C`. [157][156]

## Building

Build the application with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
ufbt
```

Install and launch it on a connected Flipper Zero:

```bash
ufbt launch
```

For a clean rebuild:

```bash
ufbt clean
ufbt
```

## Installation

1. Build the application or download a release `.fap` file.
2. Copy it to the `apps/Bluetooth` folder on the Flipper Zero SD card.
3. Open the Flipper Zero menu and launch **BLE Beacon**.
4. Select the desired beacon type.
5. Press `OK` to start advertising.

## Related project

- [BTHome Logger for Android](https://github.com/steanlab/BTHome-Logger) — an Android application for receiving and decoding unencrypted BTHome v1 and v2 advertisements.

## Author

**Siarhei Besarab (steanlab)**  
LAB-66

## License

Add your chosen open-source license here, for example MIT or GPL-3.0.
