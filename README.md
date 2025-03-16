# Atari SIO Peripheral Emulator for Flipper Zero

sio2flip is a Flipper Zero application that emulates SIO peripherals for Atari 8-bit computers. The project is still in its early stages, and currently, only limited support for Floppy Disk Drives is implemented. However, it appears to be quite functional, allowing booting from various types of ATR image files.

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

The emulator accepts disk images in ATR format, which must be copied to the SD card to `/apps_data/sio2flip/atr/`. It supports SD, ED, and DD disk images with 128- or 256-byte sectors, as well as single- and double-sided disks. Up to four floppy disk drives can be emulated.

## TODO

I have some plans, but I’m not sure if or when I’ll be able to complete them all.

- Add a “New Disk Image” menu command
- Better support for PERCOM configuration
- Support higher baud rates
- Add CAS file emulation
- Implement an XEX file loader