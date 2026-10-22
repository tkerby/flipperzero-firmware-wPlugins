# Coach Watch

A coach stopwatch for Flipper Zero. **OK** starts, stops, and resets. **Right** records splits. Sessions are saved as text files on the microSD card.

## How to use

1. **OK**: Start, then Stop, then Reset (Reset also saves the session).
2. **Right** while running: split (cumulative time plus lap).
3. **Right** while paused: save without reset.
4. **Up**: list saved sessions. **OK** opens a file.
5. **Down**: save immediately (also while running, without stopping).
6. **Back**: short press goes back one screen, or exits from the timer. Long press Back exits from any screen and saves first if the session is unsaved. **Left** does the same as short Back. **Up** opens the session list.

The Flipper vibrates on start, stop, split, and save.

## Files

Saved under apps_data/coach_watch/ on the SD card. File names look like 20261004_102315.txt and contain date, total time, and each split (absolute time and lap).

Reset or exit with an unsaved time writes the session automatically.

## Install

Copy dist/coach_watch.fap to the microSD card under apps/Tools/. On the Flipper: **Apps → Tools → Coach Watch**.

Build with ufbt. Firmware must match the API printed by APPCHK.
