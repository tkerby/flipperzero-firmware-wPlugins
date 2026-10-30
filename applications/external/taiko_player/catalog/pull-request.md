# Application Submission

Add Taiko Player v0.5 to the Media category. It plays TJA charts with streamed PCM backing audio, automatic Don/Ka mixing, a scrolling note lane, drumrolls, balloon counts, GOGO TIME effects, and optional four-channel GPIO output. Playback speed, scroll speed, a 0-200% app master volume, seeking intervals, a 15-60 FPS target display rate, and 25-100 ms GPIO pulses are configurable. Don/Ka pitch and natural duration stay fixed across playback speeds.

The project code and original icons are GPL-3.0-or-later. The two default drum sounds are unmodified assets from TJAPlayer3's MIT-licensed SimpleStyle skin; source provenance and the upstream license are included. The screenshot is an original device export supplied by the author and is included without editing.

# Extra Requirements

- An SD card and user-provided TJA charts with matching backing audio.
- Python 3 and FFmpeg on a computer to prepare unsigned 8-bit mono PCM WAV files. Conversion scripts and English instructions are provided in the source repository.
- Catalog installation includes the app; songs and optional Don/Ka WAV resources are uploaded separately. Missing drum sounds do not prevent backing-audio playback.
- No extra hardware is needed for speaker playback. GPIO output is optional, disabled at startup, and uses 3.3 V logic with a shared ground.
- Built against official firmware 1.5.1 (API 88.2). Playback, pause/resume, seeking, restart, and saved frame-rate selection were tested on a Momentum mntm-009 device. The author also confirmed normal operation of the new API 87.1 build on Momentum CN v1.1.6. Official-firmware hardware testing is still welcome.

# Author Checklist (Fill this out)

- [ ] I've read the [contribution guidelines](https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Contributing.md) and my PR follows them
- [ ] I own the code I'm submitting or have code owner's permission to submit it
- [ ] I have performed a self-review of my own code
- [ ] I have commented my code, particularly in hard-to-understand areas
- [ ] I [have validated](https://github.com/flipperdevices/flipper-application-catalog/blob/main/documentation/Manifest.md#validating-manifest) the manifest file(s) with the catalog bundle tool

# AI usage disclosure (Fill this out)

- Fully AI generated - explain what all the generated code does in moderate detail.

GPT-6.1-sol generated the project implementation, tests, and documentation under the author's direction. The author supplied requirements, reviewed behavior, and provided device testing and feedback. The screenshot was supplied by the author; the third-party drum sounds were copied from their documented upstream source.

The TJA parser handles selected difficulties, timing changes, per-note scroll multipliers, GOGO TIME, and bounded drumroll/balloon hit schedules. Allocation checks reject oversized or fragmented chart allocations, and difficulty switching stops audio before releasing the old chart.

The audio worker streams unsigned 8-bit mono WAV data from the SD card into a DMA buffer and mixes drum samples along the same chart timeline. Don and Ka each have one voice; same-sound hits replace the old tail with a short fade. Normal Don, drumrolls, and balloons share the Don voice. Hit timing uses chart samples while drum resampling uses the DMA output rate, keeping drum pitch and natural duration fixed across playback speeds. App master volume is independent of system volume and supports 0-200% gain with PCM saturation.

The UI copies shared view state under a mutex and renders a monochrome note lane, drum indicator, balloon counts, playback status, and menus. Frame-rate scheduling uses fractional ticks without adding a new timer or thread. While paused, the app waits for input instead of repeatedly recalculating and redrawing the static chart. The GPIO module handles four independently timed outputs with a shared configurable pulse width of 25-100 ms in 5 ms steps (default: 70 ms), and resets active outputs on pause or exit. Settings use a text file in the app's own data directory, with validation and temporary-file replacement.

Portable tests exercise parsing, chart timing, independent sound retriggering, fixed drum pitch across all speeds, seeking, volume amplification and saturation, adjustable GPIO pulse timing, menu selection, settings validation, and memory failures. They use AddressSanitizer and UndefinedBehaviorSanitizer. Simulated hardware tests do not replace physical GPIO waveform measurements or device testing.

# Reviewer Checklist (Don't fill this out, and don't remove it from the template)

- [ ] Bundle is valid
- [ ] There are no obvious issues with the source code
- [ ] I've ran this application and verified its functionality
