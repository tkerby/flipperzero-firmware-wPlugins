# Fire String
Generate truly random strings from the Flipper Zero using its IR sensor. Capture the infrared bursts from a flame source (like a lighter) as an unpredictable entropy source.

Inspired by a [Lemmy post](https://lemmy.dbzer0.com/post/29276607) from dipdowel

### Main Goals:
:white_check_mark: Generate random string using IR noise

:white_check_mark: Generate random string using internal `furi_hal_random` engine

:white_check_mark: Enable different character combinations for strings

:white_check_mark: Variable string lengths

:white_check_mark: Send generated Fire String though Flipper's USB HID

:white_check_mark: Save generated string to internal storage

:white_check_mark: Load generated string from internal storage

:white_check_mark: Add an About scene to the main menu, introducing the app

:white_check_mark: Create custom app icon

### Features to consider:
* Send Fire String using Bluetooth HID
* Use raw subghz noise as entropy
* Send Fire String using various other wireless capabilities (for fun?) e.g. NFC, RFID, subghz, infrared
* Create custom animations and icons for more flare 🔥
* Spam mode - continuously generate and send random characters while center button is held down (could be fun?)

## Disclaimer

Don't be stupid with fire, it burns things. Never keep an open flame unattended or near any objects.

### Acknowledgments
[The Flipper Developer Docs](https://developer.flipper.net/flipperzero/doxygen/)

[The Flipper Zero Firmware](https://github.com/flipperdevices/flipperzero-firmware)

[A Visual Guide to Flipper Zero GUI Modules](https://brodan.biz/blog/a-visual-guide-to-flipper-zero-gui-components/)

[flipper-zero-tutorials](https://github.com/jamisonderek/flipper-zero-tutorials)

[Building an app for Flipper Zero](https://instantiator.dev/post/flipper-zero-app-tutorial-01/])

And all the wonderful and helpful people on the Flipper Zero [Discord Server](https://flipperzero.one/discord) 

