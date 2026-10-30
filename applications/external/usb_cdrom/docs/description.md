# USB CD-ROM

USB CD-ROM makes your Flipper Zero appear to a computer as a USB CD/DVD-ROM drive. The disc in the drive is an ISO image from the SD card, and you can swap discs without unplugging the Flipper.

## Features

- Emulates a USB Mass Storage SCSI CD/DVD-ROM drive
- Serves ISO 9660, Joliet, UDF, hybrid and El Torito bootable images
- Chooses CD-ROM or DVD-ROM automatically, or lets you set the disc type
- Swap discs from the app while the host stays connected
- Reports tray locking and supports host-requested eject
- Shows live read statistics on screen

## Using it

Put ISO images in apps_data/usb_cdrom on the SD card. Open the app, choose Insert disc, then pick an image. The Flipper connects as a drive with the disc in it.

Press OK to eject. Press OK again to open the browser and insert another disc. Press Back to disconnect the drive.

The host can lock the tray while a disc is mounted. When it is locked, OK asks the host to eject, the way a real drive's button does. The host unmounts the disc and opens the tray. If the host does not answer, hold OK to force the tray open.

## Images

An image must be a plain ISO: a sector dump of 2048 byte blocks, so its size must be a multiple of 2048. ISO 9660, Joliet, UDF, hybrid and El Torito bootable images all work, because the drive does not look inside them.

Raw and multi-track formats such as .bin/.cue, .nrg and .mdf are not supported, including audio CDs and mixed mode discs. Images of 4 GB or more are also not supported, because the Flipper's storage API seeks with a 32 bit offset.

## Booting a PC

Pick the image in the app, plug the Flipper into a rear USB port, then power on the PC and boot from USB CDROM or USB CD/DVD in the boot menu. Older BIOSes need USB legacy support turned on.

The Flipper's USB runs at full speed and the SD card reads at a few hundred KB/s, so small boot images load fine, while a full OS installer will take a long time.

## Source code

The source is on [GitHub](https://github.com/piersroberts/flippercd). The app is based on the official Mass Storage app and is licensed under GPL-3.0.
