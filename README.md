# flipper-zero-tutorials

[YouTube: @MrDerekJamison](https://YouTube.com/@MrDerekJamison)

[https://github.com/jamisonderek/flipper-zero-sao](https://github.com/jamisonderek/flipper-zero-sao) is the repo for using the Flipper Zero to control SAO, effectively acting like a badge.

[https://github.com/jamisonderek/flipboard](https://github.com/jamisonderek/flipboard) is the repo for using the FlipBoard accessory for the Flipper Zero. FlipBoard was created by [MakeItHackin](https://github.com/makeithackin/flipboard) with software tutorials written by me (CodeAlNight).


[Discord invite: NsjCvqwPAd](https://discord.com/invite/NsjCvqwPAd) - Currently, I've **stopped using** my own Discord server and would prefer you either send me a direct message on Discord, or tag @CodeAllNight in any of the Flipper Zero firmware Discord servers.


I will use this repository for my Flipper Zero projects.  The various README.md files should describe how to use the files on your Flipper Zero.

Feel free to reach out to me at Discord with any questions or leave them in the [issues section](https://github.com/jamisonderek/flipper-zero-tutorials/issues) for this project.


## Video Game Module
### air_labyrinth
[game](./vgm/apps/air_labyrinth/README.md) - This is a game where you control a ball through a maze.  The game uses the accelerometer in the Video Game Module to control the ball.  For best experience, it is recommended to connect the VGM to the HDMI input on a TV.  The game is also displayed on the Flipper Zero's screen.

NOTE: This game uses the game `engine`, so you must recursively clone the `flipper-zero-tutorials` repository to get the `engine` submodule, or download the [https://github.com/flipperdevices/flipperzero-game-engine](https://github.com/flipperdevices/flipperzero-game-engine) into a folder called `./vmg/apps/air_labyrinth/engine`.

## RFID
### [spreadsheet](./rfid/README.md) - This is a spreadsheet that can be used to convert RFID data from one format to another, and to create new credentials with a shared facility code.  It can also be used to create RFID data for the T5577 chip.

## YouTube
### videos
[video index](./youtube/README.md) - This is a list of my various YouTube video tutorials.


## Electronics
### electronics projects
[parts list](./electronics/README.md) - This is a list of electronics parts that I will be using in my videos or tutorials.


## Firmware
### firmware-updating
[tutorial](./firmware/updating/README.md) - The Flipper Zero makes updating the firmware simple and fairly safe.  It is easy to build your own firmware as well using the fbt command.


## GPIO
### wiegand
[project](./gpio/wiegand/README.md) - This is a tool for reading and writing Wiegand data. Wiegand is typically used by NFC, RFID and keypads.  This tool can be used to read a Wiegand signal, save and display the data on the Flipper Zero's screen.  It can also play the signal back.

### gpio-pins
[tool](./gpio/pins/README.md) - This is a set of files you can copy to your Flipper Zero so that you can easily lookup the meaning of a pin.

### hc_sr04
[project](./gpio/hc_sr04/README.md) - This is an improvement over the original HC_SR04 library so that you can get mm accuracy in measurements.  It displays ultrasonic distance in both inches/cm.  The readme explains how the code gets more accurate timings.

### gpio-gpio-7segment
[project](./gpio/gpio_7segment/README.md) - This application can control a common-anode or common-cathode 7-segment display.  When you click the OK button the display shows a random dice roll (1-6); but you can modify code to display any number from 0-9.

### gpio-gpio-polling-demo
[tutorial](./gpio/gpio_polling_demo/README.md) - This is a "hello world" demonstration of reading a GPIO pin using polling.

### gpio-gpio-interrupt-demo
[tutorial](./gpio/gpio_interrupt_demo/README.md) - This is a "hello world" demonstration of triggering a callback when a GPIO pin transitions from VCC to GND.

### gpio-memsic_2125
[tutorial](./gpio/memsic_2125/README.md) - This is a demostration of using GPIO interrupts to interpret data from the Memsic 2125 (Mx2125) Dual-Axis Accelerometer.


## Subghz [folder](./subghz/README.md)
### subghz-plugins-subghz_demo
[tutorial](./subghz/plugins/subghz_demo/README.md) - This is a demonstration of sending &amp; receiving radio signals using the subghz_tx_rx worker library.

### subghz-plugins-rock_paper_scissors
[game](./subghz/plugins/rock_paper_scissors/README.md) -
This is a two player game that uses the subghz_tx_rx worker library for communication.  Two Flipper Zeros running this game can play Rock, Paper, Scissors against each other!

### subghz-protocol-x10-decoder
[project](./subghz/protocols/x10/README.md) - This is a protocol decoder for the Flipper Zero to decode the x10 series of devices when doing a read from the Sub-GHz menu.

### subghz-samples-chevy-hhr-2006
[data](./subghz/samples/chevy-hhr-2006/README.md) - These Flipper Zero subghz captures are from the remote control for the Chevy HHR 2006 keyfob.

### subghz-samples-quantum-fire
[data](./subghz/samples/quantum-fire/README.md) - These Flipper Zero subghz captures are from the remote control for the Quantum Fire (QF-6LR).

### subghz-samples-x10
[data](./subghz/samples/x10/README.md) - These Flipper Zero subghz captures are from the remote control for the X10.

## Marauder
[tutorial](./marauder/README.md) - This is a tutorial on how to use the Marauder tool to attack 2.4GHz devices.

[tool](./marauder/secret_ssid/README.md) - This is a tool that can be used to send a secret message over WiFi.  The message is encoded into a SSID, which is then sent out over WiFi.  The message can be decoded on a PC.

## Support
[Buy me a coffee](https://ko-fi.com/codeallnight) - Thanks for supporting my work.
