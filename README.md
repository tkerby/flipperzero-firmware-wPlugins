# Fire String
Generate truly random strings from the Flipper Zero using its IR sensor. Capture the infrared bursts from a flame source (like a lighter) as an unpredictable entropy source.

Inspired by a [Lemmy post](https://lemmy.dbzer0.com/post/29276607) from dipdowel

# *** Work in Progress ***

### Main Goals:
:white_check_mark: Generate random string using IR noise

:white_check_mark: Generate random string using internal `furi_hal_random` engine

:white_check_mark: Enable different character combinations for strings

:white_check_mark: Variable string lengths

:black_square_button: Send generated string though Flipper's USB HID

:black_square_button: Save generated string to internal storage

:black_square_button: Load generated string from internal storage

:black_square_button: Add an About popup to the main menu, introducing the app

## Disclaimer

Don't be stupid with fire, it burns things. Never keep an open flame unattended or near any objects.

### Acknowledgments
[The Flipper Developer Docs](https://developer.flipper.net/flipperzero/doxygen/)

[A Visual Guide to Flipper Zero GUI Modules](https://brodan.biz/blog/a-visual-guide-to-flipper-zero-gui-components/)

[flipper-zero-tutorials](https://github.com/jamisonderek/flipper-zero-tutorials)

[Building an app for Flipper Zero](https://instantiator.dev/post/flipper-zero-app-tutorial-01/])

And all the wonderful and helpful people on the Flipper Zero [Discord Server](https://flipperzero.one/discord) 

