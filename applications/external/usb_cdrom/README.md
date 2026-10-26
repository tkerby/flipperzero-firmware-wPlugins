# USB CD-ROM

Flipper Zero app that shows up as a USB CD/DVD-ROM drive. The disc in the drive is an `.iso` file from the SD card, and you can swap discs without unplugging.

Sister app to [USB Floppy](https://github.com/piersroberts/flipperfloppy). Both are forked from the official [Mass Storage app](https://github.com/flipperdevices/flipperzero-good-faps/tree/dev/mass_storage). GPL-3.0.

## What the host sees

| Field | Value |
| --- | --- |
| Interface class | Mass Storage (0x08) |
| Subclass | SCSI transparent (0x06) |
| Protocol | Bulk-Only (0x50) |
| Peripheral type | CD/DVD (0x05), removable, tray loader |
| Block size | 2048 bytes |
| Profiles | CD-ROM (0x0008), DVD-ROM (0x0010) |

Images over 99 minutes of CD (912,384,000 bytes) are served as DVD-ROMs, and smaller ones as CD-ROMs. The Disc type setting overrides this.

## Images

An image must be a plain ISO: a sector dump of 2048 byte blocks, so its size is a multiple of 2048. ISO 9660, Joliet, UDF, hybrid and El Torito bootable images all work, because the drive doesn't look inside them.

These don't work:

- `.bin`/`.cue`, `.nrg`, `.mdf` and other raw or multi-track formats, including audio CDs and mixed mode discs. Convert data-only ones to ISO with `bchunk` or similar.
- Images of 4 GB or more. The Flipper's storage API seeks with a 32 bit offset, which rules out most full single layer DVDs.

## Build and install

```sh
pip install ufbt
ufbt            # builds dist/usb_cdrom.fap
ufbt launch     # installs and runs it over USB
```

Put images in `SD card/apps_data/usb_cdrom/`.

## Use

1. Open the app and pick Insert disc, then an image. The Flipper connects as the drive with the disc in it.
2. Press OK to eject. Press OK again to open the file browser and insert another disc.
3. Press Back to disconnect the drive.

The host can lock the tray while a disc is mounted, as macOS, Windows and Linux all do. When it's locked, OK asks the host to eject, the way a real drive's button does. The host unmounts the disc and opens the tray. If the host doesn't answer, as a BIOS or DOS won't, hold OK to force the tray open.

The bottom line on the screen shows the disc type and size, and `Locked` while the host holds the tray.

## Boot a PC from it

1. Pick the image in the app.
2. Plug the Flipper into a rear USB port. No hubs.
3. Power on the PC and boot from `USB CDROM` or `USB CD/DVD` in the boot menu. Older BIOSes need USB legacy support turned on.

The Flipper's USB runs at full speed (12 Mbit/s), and the SD card reads at a few hundred KB/s. Small boot images like DOS boot discs, utility CDs and netinstall ISOs load fine. A full OS installer will take a long time.

## Supported commands

TEST UNIT READY, REQUEST SENSE, INQUIRY, MODE SENSE (6 and 10, pages 01h, 2Ah, 3Fh), MODE SELECT (6 and 10), START STOP UNIT (eject, refused while locked), PREVENT ALLOW MEDIUM REMOVAL, READ CAPACITY, READ (10 and 12), READ CD (user data only), SEEK, VERIFY, READ TOC/PMA/ATIP (formats 0, 1, 2, LBA and MSF), GET CONFIGURATION, GET EVENT STATUS NOTIFICATION (media class, polled), READ DISC INFORMATION, READ TRACK INFORMATION, READ DISC STRUCTURE (DVD formats 00h and 01h), MECHANISM STATUS, SET CD SPEED, SET STREAMING, SYNCHRONIZE CACHE, REZERO, SEND DIAGNOSTIC.

A new disc raises UNIT ATTENTION (28h, not ready to ready change) and a NewMedia event. With the tray open, media commands return NOT READY (3Ah, medium not present).
