# BLE Beacon

BLE Beacon turns a Flipper Zero into a configurable Bluetooth Low Energy advertising beacon.

It simulates unencrypted BTHome v1 and v2, iBeacon, Eddystone TLM, ATC1441, and MiBeacon advertisements.

The app was created as a Flipper Zero companion for [BTHome Logger](https://github.com/steanlab/BTHome-Logger), but it can also be used independently for testing BLE scanners, smart-home integrations, widgets, and mobile applications.

## Supported beacon types

- BTHome V2 Tag
- BTHome V1 Tag
- iBeacon Tag
- Eddystone TLM Tag
- ATC1441 Tag
- MiBeacon Tag

## Real and simulated data

The app broadcasts real Flipper Zero telemetry:

- Battery level
- Battery voltage
- Battery current
- Battery temperature
- Charger temperature

Some fields are simulated because the Flipper Zero does not have the corresponding hardware sensors:

- Humidity: 42 to 48 percent
- iBeacon UUID
- iBeacon major and minor values
- iBeacon measured RSSI

The simulated humidity changes slightly between advertising updates, which is useful for testing that a receiver updates its interface correctly.

## Main screen

The main screen shows the selected beacon type, live telemetry, and the advertising status.

When the beacon is active, an ADV indicator and a blinking Bluetooth symbol appear at the top of the screen.

## Controls

- Left: previous beacon type
- Right: next beacon type
- Up, short press: select Info
- Down, short press: select Help
- Up, long press: open Info
- Down, long press: open Help
- OK: start or stop the beacon, or open the selected Info or Help screen
- Back: open the exit confirmation screen

The beacon is not started automatically. Press OK to begin advertising.

## Info screen

The Info screen explains which values are simulated and which values come from the Flipper hardware.

It also contains information about the author, contacts, ways to support development, and special thanks.

Use Up and Down to scroll through the text. Press OK or Back to return to the main screen.

## Help screen

The Help screen contains a short reference for all controls.

Use Up and Down to scroll through the text. Press OK or Back to return to the main screen.

## Exit screen

Pressing Back opens an exit confirmation screen.

- Left selects No and returns to the application
- Right selects Yes and exits the application

After selecting Yes, the app shows a short exit splash screen, stops BLE advertising, releases resources, and exits cleanly.

## Technical details

- Uses the Flipper Zero extra BLE beacon API
- Advertising interval: 1000 ms
- Advertising update interval: 2000 ms
- Advertising channels: 37, 38, and 39
- Address type: random static address
- All supported advertisements are unencrypted
- BLE advertising payload is limited to 31 bytes

## Related project

[BTHome Logger for Android](https://github.com/steanlab/BTHome-Logger) receives and decodes unencrypted BTHome v1 and v2 advertisements.

## Author

Siarhei Besarab, also known as steanlab.  
LAB-66

- Telegram: [t.me/lab66](https://t.me/lab66)
- LinkedIn: [linkedin.com/in/steanlab](https://www.linkedin.com/in/steanlab)
- Mastodon: [mastodon.social/@lab66](https://mastodon.social/@lab66)

## Support development

- Patreon: [patreon.com/steanlab](https://www.patreon.com/steanlab)
- GitHub Sponsors: [github.com/sponsors/steanlab](https://github.com/sponsors/steanlab)
- PayPal: [paypal.me/steanlab](https://paypal.me/steanlab)
- Revolut: [revolut.com](https://revolut.com)

## Special thanks

Thanks to the Flipper Zero Team and Pavel Zhouner for providing lab equipment.

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.
