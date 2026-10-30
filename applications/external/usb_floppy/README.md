# USB Floppy

Flipper Zero app that shows up as a USB floppy drive. It reads a 720K or 1.44M `.img` from the SD card.

Forked from the official [Mass Storage app](https://github.com/flipperdevices/flipperzero-good-faps/tree/dev/mass_storage). GPL-3.0.

## What the host sees

| Field | Value |
| --- | --- |
| Interface class | Mass Storage (0x08) |
| Subclass | UFI (0x04) |
| Protocol | CBI with interrupt (0x00) or Bulk-Only (0x50), set in the app |
| Geometry | 1.44M: 80 cyl, 2 heads, 18 sectors. 720K: 80 cyl, 2 heads, 9 sectors |

The image size must be exact: 1,474,560 or 737,280 bytes.

## Build and install

```sh
pip install ufbt
ufbt            # builds dist/usb_floppy.fap
ufbt launch     # installs and runs it over USB
```

Put images in `SD card/apps_data/usb_floppy/`.

## Boot an old PC from it

1. Open the app. Pick Transport and Write protect, then the image.
2. Plug the Flipper into a rear USB port. No hubs.
3. Power on the PC.
4. In the BIOS, turn on USB legacy support and boot from `USB-FDD` (Award), `Removable Dev.` (AMI) or `USB Floppy` (Phoenix).

Start with CBI, because real USB floppy drives use it. If the BIOS doesn't list the drive, try Bulk-Only.

Turn on Write protect to keep the image safe. The app then opens the file read only.

## Supported commands

TEST UNIT READY, REQUEST SENSE, INQUIRY, READ FORMAT CAPACITIES, READ CAPACITY, MODE SENSE (6 and 10, pages 05h, 1Bh, 1Ch, 3Fh), MODE SELECT (10), READ (10 and 12), WRITE (10 and 12), WRITE AND VERIFY, VERIFY, SEEK, REZERO, FORMAT UNIT (one track or the whole disk, fills with F6h), START STOP UNIT (eject returns to the menu), PREVENT ALLOW MEDIUM REMOVAL (allow only), SEND DIAGNOSTIC.
