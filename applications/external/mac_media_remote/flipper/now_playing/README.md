# Now Playing for Mac

Show the current macOS media session on Flipper Zero over Bluetooth LE: title, artist, album, progress, and available cover art. Use Flipper buttons to pause or resume, change tracks, and adjust Mac system volume.

## Requirements

- A Mac with Bluetooth running the [Mac Media Remote companion](https://github.com/Vchekryzhov/mac-media-remote). The Flipper Apps Catalog installs only the Flipper app.
- [nowplaying-cli](https://github.com/kirtan-shah/nowplaying-cli) installed on the Mac. The companion uses it to read the system Now Playing session.
- An app or browser tab that publishes playback information to macOS Now Playing.

## Setup

1. Install and launch Now Playing from the Bluetooth category on Flipper Zero.
2. Follow the [Mac setup instructions](https://github.com/Vchekryzhov/mac-media-remote#установка) to build and start the companion.
3. Enable Bluetooth and confirm pairing when prompted. USB is only needed for setup or recovery.

## Controls

- OK: play or pause.
- Left and right: previous or next track.
- Up and down: Mac system volume, when the selected audio output supports software volume control.
- Hold Back: exit the app.

Cover art is optional. Playback information and controls require the Mac companion to be running.
