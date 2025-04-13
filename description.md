# Atari SIO Peripheral Emulator for Flipper Zero

sio2flip emulates SIO peripherals for Atari 8-bit computers, currently featuring floppy drive emulation, direct XEX executable loading and Atari 850 modem emulation.

## Wiring

Only three signals plus ground are used. Note that Atari SIO uses 5V TTL logic; although the Flipper Zero I/Os are 5V tolerant when configured correctly, careful wiring is essential to avoid damage.

| Flipper pin  | Atari SIO |
| ------------ | --------- |
| TX (13)      | DIN       |
| RX (14)      | DOUT      |
| C0 (16)      | COMMAND   |
| GND (18)     | GND       |

## Instructions

Place ATR disk images in */apps_data/sio2flip/atr/* and XEX files in */apps_data/sio2flip/xex/* on the Flipper’s SD card.

More details and additional information can be found in the GitHub repository.