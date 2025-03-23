# Atari SIO Peripheral Emulator for Flipper Zero

sio2flip is a Flipper Zero application that emulates SIO peripherals for Atari 8-bit computers. The project is still in its early stages, and currently offers limited support for floppy disk drives.

## What's tested

The app has been tested with the PAL version of the Atari 800XL. It appears to be quite functional, allowing the system to boot from various ATR image files. The following features have already been implemented:

- Device status command (0x53)
- Sector read and write commands (0x52, 0x57, 0x53)
- Disk formatting commands (0x21, 0x22, 0x66)
- Commands for reading and writing PERCOM configuration (0x4E, 0x4F)
- US Doubler mode is emulated (with either 38400 Bd or 57600 Bd).
- XF-551 High Speed mode is emulated 
- Support for 90K, 130K, 180K, 360K, and 720K ATR disk images
- 128- or 256-byte sectors are supported
- Up to four floppy disk drives can be emulated.


## Wiring

The wiring is quite simple; only three signals are need. 

| Flipper pin  | Atari SIO |
| ------------ | --------- |
| TX (13)      | DIN       |
| RX (14)      | DOUT      |
| C0 (16)      | COMMAND   |
| GND (18)     | GND       |

Note: All signals on the Atari SIO use 5V TTL logic. Flipper Zero’s I/Os are 5V tolerant when configured as inputs. 

## Instructions for Use

The emulator accepts disk images in ATR format, which must be copied to the SD card to `/apps_data/sio2flip/atr/`.

## TODO

I have some plans, but I’m not sure if or when I’ll be able to complete them all.

- Add a “New Disk Image” menu command
- Verify that PERCOM configuration is correct
- Add CAS file emulation
- Implement an XEX file loader
- Improve error signaling to host via FDD status byte