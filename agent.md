# Repo notes (agent/human)

This repository contains a Flipper Zero external app for controlling an FM receiver and (optionally) an external I2C volume controller.

## What it is
- App entry: [application.fam](application.fam)
- Main app/UI logic: [radio.c](radio.c)
- TEA5767 driver: [TEA5767/](TEA5767/)
- PT2257 support: [PT2257/](PT2257/)

## Build / run
This is an external app. Typical workflow:

- Build: `ufbt build`
- Install + launch (device connected): `ufbt launch`

## Persistent data on device
Settings and presets are stored on the SD card:

- `/ext/apps_data/fmradio_controller_pt2257/settings.fff`
- `/ext/apps_data/fmradio_controller_pt2257/presets.fff`

## Publishing / licensing
- License: GPLv3 (see [LICENSE](LICENSE)).
- Upstream origin is documented in [README.md](README.md).
- If you publish built binaries (e.g. `.fap`) from modified sources, publish the corresponding source code for that exact version (tag/commit) and keep attribution.
