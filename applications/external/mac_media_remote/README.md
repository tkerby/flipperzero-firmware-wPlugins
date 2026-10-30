# Mac Media Remote

**English** | [Русский](README.ru.md)

Display the current macOS media session on Flipper Zero over Bluetooth Low Energy: track title, artist, album, playback progress, and available cover art in a high-contrast poster style. Works with any app or browser tab that publishes information to the system Now Playing session. Audio stays on the Mac. USB is only needed to install the Flipper app.

## Requirements

- Flipper Zero with official firmware 1.4.3 (API 87.1), Bluetooth enabled, and a microSD card.
- A Mac with Bluetooth and the Swift toolchain.
- `nowplaying-cli`: `brew install nowplaying-cli`.

## Setup

```sh
sh scripts/build-flipper.sh # Uses the firmware 1.4.3 SDK by default
sh scripts/build-mac.sh
```

The Flipper build script downloads the official SDK and places the FAP in `dist/`. Use qFlipper over USB to install the FAP at `/ext/apps/Bluetooth/now_playing.fap`, then open **Apps → Bluetooth → Now Playing**. For development, run `./fbt launch APPSRC=now_playing` from the SDK directory to upload and launch the FAP without updating the firmware.

In a Mac terminal, run:

```sh
sh scripts/run-mac.sh
```

On first launch, allow Bluetooth access and confirm pairing on both devices when prompted. You can disconnect the USB cable after installing the FAP. Hold Back to exit the Flipper app.

If the Flipper stays on `Waiting for Mac`, check its screen for a pairing code and press OK to confirm. The companion should then connect automatically.

## Controls

- OK: play or pause the active system media player.
- Left/right: previous/next track.
- Up/down: adjust Mac system volume by 5%; hold to repeat. Requires an audio output device that supports software volume control.

Apps and browsers that do not publish their state to macOS Now Playing will not appear on the Flipper. Playback position depends on the timestamp provided by the media source. If Bluetooth disconnects, the companion reconnects automatically; the Flipper marks the previous track information as `Reconnecting`.

## Screenshots

Actual screenshots from a connected Flipper. The music screens were captured during Yandex Music playback:

| Waiting for Mac | Playing | Another album |
| --- | --- | --- |
| ![Waiting for Mac](flipper/now_playing/screenshots/00-waiting-for-mac.png) | ![Yandex Music track](flipper/now_playing/screenshots/01-playing-yandex.png) | ![Another album](flipper/now_playing/screenshots/02-playing-yandex.png) |

| Next track | Paused | Reconnecting |
| --- | --- | --- |
| ![Next track](flipper/now_playing/screenshots/03-next-track-yandex.png) | ![Paused](flipper/now_playing/screenshots/04-paused-yandex.png) | ![Reconnecting](flipper/now_playing/screenshots/05-reconnecting.png) |

## Source code

The protocol is defined in `protocol/constants.json`. The Flipper code is adapted from `villenull/flipper-now-playing`; see `THIRD_PARTY_NOTICES.md` and `LICENSE`.
