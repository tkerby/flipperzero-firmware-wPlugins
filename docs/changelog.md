# Changelog

## v0.4

- Add a saved target display frame rate from 15 to 60 FPS in 1 FPS steps, defaulting to 50 FPS.
- Preserve audio and hit scheduling independently of the selected display frame rate.
- Replace the drum sounds with unmodified TJAPlayer3 SimpleStyle assets and retain their MIT attribution in converted resources and packages.
- Add GPL-3.0-or-later licensing, an original device screenshot, and catalog metadata.
- Split detailed documentation into user, chart-support, and development guides, and add an English README.

## v0.3

- Prevent automatic drumroll and balloon hits from accumulating sound tails; fade out the previous automatic hit before retriggering.
- Raise the automatic-hit cap to 32 hits per second.
- Smooth big-note, drumroll, drum-indicator, and balloon pixel contours.
- Improve balloon head sizing and preserve its complete hit ring.
- Reduce repeated mixer work and reuse drawing text storage.
- Improve allocation checks and cleanup when audio startup fails.

## v0.2

- Add hit-ring feedback for normal and big Don/Ka notes.
- Refine GOGO TIME effects and align the playback-time and difficulty labels.
- Release finished audio workers and buffers promptly.
- Reduce memory peaks when changing difficulty and recover the previous chart on failure.
- Add heap exhaustion, fragmentation, retry, and speed-change rollback tests.

## v0.1

- Initial release with TJA chart playback and PCM backing audio.
- Add scrolling notes, automatic drum sounds, drumrolls, balloons, and GOGO TIME.
- Add difficulty, playback-speed, scroll-speed, seeking, and volume controls.
- Add configurable GPIO drum-hit outputs and saved preferences.
