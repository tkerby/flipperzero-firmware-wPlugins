# v2.0-alpha (2025-02-04)
## New Features
- Transmission now sends in thread instead of blocking.
- Mode Selection:
  - Normal ("Normal"): Transmits after interval triggers, continuing until exited with back button.
  - Immediate ("Immed"): Same as Normal mode, only this transmits immediately when entering the run state.
  - One-Shot ("1-Shot"): Waits the specified interval, transmits, then exits the run loop.
- Menu changes, including moving [Single] and [Playlist] markers from "Start" to "Select File"
- If playlist selected, number of list items are displayed.
## Bug-fixes
- Timing bug where 'previous_time' would be set before transmission completes. Very noticeable on longer playlists.
- Repeats caused lockup in v1.0. TX repeats now functional.

# Initial release - v1.0 (2025-01-29)

## Features:
- Can be used with single SUB files (beacon) and TXT playlists.
- Each transmitted signal can individually be sent once or repeated up to 6 times.
- Discrete interval values range from 10 seconds to 12 hours.
- Display shows countdown to next scheduled transmission.