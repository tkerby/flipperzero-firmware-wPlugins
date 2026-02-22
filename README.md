# flipperzero-radio-with-volume-control
Flipper Zero external app to control TEA5767 FM radio boards and PT2257 I2C volume.

## Hardware / Schematics

- Schematic (SVG): [schematics/svg/schematic_radio_tea5767_pt2257_2026-02-22.svg](schematics/svg/schematic_radio_tea5767_pt2257_2026-02-22.svg)
- PCB files: planned to be added in the future (work in progress).

## Provenance / Licensing

- **Upstream origin**: https://github.com/victormico/flipperzero-radio
- **Upstream FlipC listing**: [![FlipC.org](https://flipc.org/victormico/flipperzero-radio/badge?nowerr=1)](https://flipc.org/victormico/flipperzero-radio?nowerr=1)
- **This version**: packaged as `[TEA+PT2257] FM Radio` (appid `fmradio_controller_pt2257`) so it can coexist
	with a TEA5767-only build.
- **Maintained by**: pchmielewski1
- **License**: GPLv3 (see `LICENSE`).

If you publish binaries (e.g. `.fap`) built from modified sources, publish the corresponding source
code for that version (tag/commit) and keep attribution.

## Supported boards
* TEA5767 (I2C FM receiver)
* PT2257 (I2C electronic volume controller)
* PAM8403 (external audio amplifier)

### PAM8403 note
TEA5767 line-out is not meant to drive a speaker directly. A small stereo amp module like PAM8403 is a common choice.
Some PAM8403 boards have a fixed gain/volume and accept L/R line input.

## Audio chain (example)
One working setup:

- TEA5767 L/R -> coupling caps `4.7uF` (X7R ceramic) -> PT2257 IN
- PT2257 OUT -> coupling caps `4.7uF` (X7R ceramic) -> PAM8403 L/R input
- PAM8403 output -> speaker `8Ω` (~7.5 cm) in enclosure

Volume / noise tweak used on one popular PAM8403 board:

- Reduce overall gain/volume by changing the board's volume resistor from `10kΩ` to `20kΩ` (on the PAM "volume" pin network).

## PIN
 - 3V3 (3V3, pin 9)  = VCC
 - GND (GND, pin 18) = GND
 - SCL (C0, pin 16)  = SCL
 - SDA (C1, pin 15)  = SDA

## Controls
- Left/Right (short): tune -/+ 0.1 MHz (76.0–108.0)
- Left/Right (hold): scan band -/+ (TEA5767) and build presets
- Left/Right (hold) x3 quickly (within ~2.5s): clear presets
- OK (short): toggle mute (PT2257)
- OK (hold): save current frequency to preset (no duplicates, overwrites current slot if full)
- Up/Down (short): preset next/prev (uses scanned list when available; otherwise cycles the built-in list)
- Up/Down (hold): volume up/down (PT2257 attenuation; hold repeats)
- Back: go back (Listen view -> Menu; Menu -> exit)

## Config menu
Open with: `Menu -> Config`

- `Freq (MHz)`: picks a frequency from the built-in list and tunes immediately.
	Use this if you want a quick jump to a known station (main view Left/Right still does fine manual tuning).
- `Volume`: toggles PT2257 mute/unmute (same effect as `OK (short)` in the main view).
	Real volume level is PT2257 attenuation controlled by `Up/Down (hold)`.

- `Backlight`: keeps the screen backlight on while the app is running.
	Useful if your screen turns off after ~1 minute.

- `SNC`, `De-emph`, `SoftMute`, `HighCut`, `Mono`: TEA5767 live audio options (see sections below).

## Presets (persistent)
This version maintains a dynamic preset list saved on the SD card.

- Scan (hold Left/Right) walks the band and adds found stations to the preset list.
- Up/Down (short) navigates the preset list when it exists.
- OK (hold) saves the current tuned frequency into presets.
- Scan x3 quickly (3x hold Left/Right within ~2.5s) clears the preset file.

Files:
- Settings: `/ext/apps_data/fmradio_controller_pt2257/settings.fff`
- Presets: `/ext/apps_data/fmradio_controller_pt2257/presets.fff`

## Differences vs upstream
- PT2257 I2C volume + mute integrated (Flipper HAL I2C)
- PT2257 hot-plug handling (applies attenuation/mute when PT appears on the bus)
- Correct mono/stereo reporting for TEA5767 (REG_3_MS semantics)
- Manual tuning on short Left/Right in 0.1 MHz steps
- Long Left/Right starts full-band scan to build presets
- Band: 76.0–108.0 MHz
- GUI updates: simplified Audio line (MUTE or Stereo/Mono) and PT2257 status (OK/ERROR)

## Persistent settings (SD)
The app saves and restores the last state to a file on the SD card (apps data).

- Path: `/ext/apps_data/fmradio_controller_pt2257/settings.fff`
- Stored keys:
	- `Freq10kHz` (e.g. 8810 for 88.10 MHz)
	- `PtAttenDb` (0..79)
	- `PtMuted` (true/false)
	- `TeaSNC` (bool) - TEA5767 Stereo Noise Cancelling (SNC) toggle
	- `TeaDeemph75us` (bool) - TEA5767 de-emphasis time constant: true=75µs (US), false=50µs (EU)
	- `TeaSoftMute` (bool) - TEA5767 SoftMute (reduces noise on weak stations, may "pump")
	- `TeaHighCut` (bool) - TEA5767 High Cut Control (less hiss, less treble detail)
	- `TeaForceMono` (bool) - TEA5767 force mono (often cleaner on weak signals)
	- `BacklightKeepOn` (bool) - keep Flipper backlight on while the app is running

## TEA5767 SNC (Stereo Noise Cancelling)

You can toggle TEA5767 SNC in the app menu:

- `Menu -> Config -> SNC`

Notes:
- `SNC On` can reduce hiss/noise on weak stations.
- `SNC Off` can sound more natural on strong stereo stations.

## TEA5767 de-emphasis (DTC)

FM broadcast uses pre-emphasis; the receiver must apply matching de-emphasis.

- `Menu -> Config -> De-emph`
- `50us` is typical for EU.
- `75us` is typical for US.

## TEA5767 live audio options

These options can be toggled while listening (you should hear the change immediately):

- `Menu -> Config -> SoftMute`
- `Menu -> Config -> HighCut`
- `Menu -> Config -> Mono`

## Audible noise / power filtering tips
If you hear audible tones (often in the ~8–16 kHz range) while powering modules from Flipper:

- Add local decoupling close to each module IC: `100nF` + `1–10uF` ceramics (X7R) in addition to bulk electrolytics.
- Add a series element on the supply into the audio section (PAM8403 especially): ferrite bead (e.g. 600Ω@100MHz) or 10µH inductor.
- Use star-grounding and short wiring; keep audio wires away from SDA/SCL.
- Consider disabling forced backlight if it injects PWM noise (see `BACKLIGHT_ALWAYS_ON` in `radio.c`).

### Proven fix (ground wiring)
Observed symptom on one build:

- A phone spectrum app measurement showed two peaks around ~8.4 kHz and ~16.6 kHz while powering TEA/PT and PAM8403 from Flipper.
- The ~8.4 kHz tone was clearly audible; the ~16.6 kHz component was present but barely audible.

What fixed it:

- Run PAM8403 power ground (GND next to VCC) as a separate wire directly to Flipper main GND (star ground style).
- Run PAM8403 audio/input ground to the same ground reference used by the PT2257 audio path (i.e. pair audio L/R with its signal GND).

Notes:

- Some PAM8403 boards expose separate "power GND" and "audio GND" pads; they may read 0Ω between them on the PCB. Even then, wiring them as above helps avoid shared return currents in your harness.

Preset file keys (presets.fff):
- `Count`
- `Freq10kHz` (array)
- `Index`

## Build
This is a Flipper Zero external app (has `application.fam`). Two common workflows:

- `ufbt` (recommended for external apps):
	- `ufbt build`
	- `ufbt launch`
	- Note: do not pass `.fap` path to `ufbt launch` (e.g. `ufbt launch fmradio_controller_pt2257.fap`), because that form is not valid in this setup.
- `fbt`: clone the Flipper firmware repo, copy/link this app into `applications_user/`, then run `./fbt fap_fmradio_controller_pt2257`.

Exact commands vary depending on your firmware/tooling version.

## Automated releases (GitHub Actions)
This repo includes a GitHub Actions workflow that builds a ready-to-download `.fap` on every push to `main` and publishes it as a GitHub Release.

- Workflow: `.github/workflows/release.yml`
- Versioning scheme (auto-incrementing): `0.9+pt2257.<GITHUB_RUN_NUMBER>`
	- Example: `0.9+pt2257.123`
	- The release tag is `v0.9-pt2257.<GITHUB_RUN_NUMBER>` (tags avoid `+` for compatibility).

Install from Release:
- Download the `.fap` from the latest GitHub Release.
- Copy it to your Flipper SD card: `/ext/apps/GPIO/`
- Run it from `Applications -> GPIO`.


## References
App inspired by [Radio](https://github.com/mathertel/Radio) project for Arduino

## Aknowledgements
Special thanks to [Derek Jamison](https://github.com/jamisonderek) for his amazing [Flipper Zero Tutorials](https://github.com/jamisonderek/flipper-zero-tutorials) that helped me to get started with Flipper Zero development.

And to [Oleksii Kutuzov](https://github.com/oleksiikutuzov) for his [Lightmeter](https://github.com/oleksiikutuzov/flipperzero-lightmeter) app, which was used as a template for this app.
