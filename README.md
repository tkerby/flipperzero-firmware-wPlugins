# Blinker

Flipper Zero App, which blinks LED with decreasing cadency.

Based on the following [HN thread](https://news.ycombinator.com/item?id=38274782).

## Main screen:
![Blinker menu](pictures/blinker_menu.png)

## Configure interval and duration:
![Blinker number menu](pictures/blinker_number_menu.png)

It is possible to configure following:
* `max interval`: Initial cadence.
* `duration`: Period of time during which the cadence should change.
* `min interval`: Eventual cadence.

## Start blinking:
![Start blinking](pictures/blinker_blinking.png)

While the following screen is shown, the LED on Flipper Zero blinks in red color with the same `bpm` as shown on the screen.

## Development
Use [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) to flash the code to Flipper. Execute `ufbt` or `ufbt launch`.

I used the following [website](https://www.moodlight.org/#0) to verify with my __caveman eye__ that the frequency of blinking LED matches the config.