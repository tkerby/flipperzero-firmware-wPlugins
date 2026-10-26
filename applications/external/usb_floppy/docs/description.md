# USB Floppy

USB Floppy makes your Flipper Zero appear to a computer as a USB floppy drive. Select a 720K or 1.44M disk image from the SD card and the host can read it, write to it, and even boot from it.

## Features

- Emulates a USB Mass Storage UFI floppy drive
- Reads 720K (737280 bytes) and 1.44M (1474560 bytes) .img images
- Choice of CBI or Bulk-Only transport
- Optional write protection, which also opens the image read only
- Live read and write statistics on screen
- Ejecting the drive from the host returns to the app menu

## Using it

Put your disk images in apps_data/usb_floppy on the SD card, open the app, choose the transport and write-protect setting, then select an image. Plug the Flipper into a rear USB port on the target machine, without a hub.

Start with CBI, because real USB floppy drives use it. If the BIOS does not list the drive, try Bulk-Only.

## Booting an old PC

Enable USB legacy support in the BIOS and select USB-FDD, Removable Dev., or USB Floppy as the boot device, depending on the BIOS vendor.

## Source code

The source is on [GitHub](https://github.com/piersroberts/flipperfloppy). The app is based on the official Mass Storage app and is licensed under GPL-3.0.
