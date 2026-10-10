# BusterMan Decoder

Real input replaces the first version's built-in meal picker. The supplied desktop Python files remain untouched. Native C ports provide the baseline Hex -> Base64 -> Brainfuck chain, explicit decoder modes, and ROT13. Device limits and conservative Brainfuck auto detection are deliberate departures from the unlimited desktop baseline.

## Release status

The app display name is **BusterMan Decoder**. A renamed binary built with uFBT against official release SDK 1.4.3 is available at [builds/BusterMan.fap](builds/BusterMan.fap). Build validation passed for target f7, API 87.1. The owner reports the renamed build works on their Flipper; the previously supplied binary is preserved separately in the owner's private backups.

Catalog icon and metadata are prepared; submission is pending creation of the catalog fork and pull request. Original qFlipper captures are in screenshots/; they show the prior build's splash and decoder modes.

The internal appid, save directory, entry point, and USB commands retain their legacy names for compatibility with existing progress and CyberKing.

[DigitalEnigma website](https://d3cyberking.com) · [About the projects](https://github.com/Mr-Nobody88/AboutMe)

## Install

Close the old app. Using qFlipper, replace its file at `/ext/apps/Tools/BusterMan.fap` with the new compiled file. Open Apps > Tools > BusterMan Decoder. The published renamed binary is built with the official 1.4.3 SDK, target f7, API 87.1. Firmware with an incompatible app API needs a rebuild.

## Controls

- OK dismisses the splash (it also disappears after about three seconds).
- Up/Down selects Auto chain, Hex, Base64, Brainfuck or ROT13.
- OK opens Flipper's text keyboard. Enter your input and save/submit using the keyboard's own controls. Existing input remains available for editing.
- Right opens an SD-card browser for .txt payloads. Use this for long strings, multiline code, or symbols unavailable on the built-in keyboard. Copy any encoded text file to the SD card, then browse to it. This is an input option, not a watched feed folder.
- On results, Up/Down scrolls, Left shows result details, and Back returns to decoder selection.
- Back from decoder selection exits.

## Feeding and progression

Each successful nonempty decode that produces changed data earns 2 XP per decoded layer. Ten XP grants a level; extra XP carries over. A small eating indicator accompanies new meals. The last 64 mode/input hashes prevent repeat feeding rewards. This is bounded recent history, not permanent deduplication. A 32-bit fingerprint has a small collision possibility. Choosing a different mode is treated as a different meal.

Progress is saved to `/ext/apps_data/megaman/progress.bin`. Inputs and decoded strings are not saved. A temporary state file is synced and renamed into place on each new meal; if writing fails, the header shows ! and the info screen reports session-only progress. This cannot guarantee survival of a power loss during SD operations.

## Graphics

The splash and all six walking frames come from the supplied DigitalEnigma_v3(2).py. They are converted to monochrome and resized for Flipper's 128x64 display. Their original colors cannot be displayed on the built-in LCD. The character is 30x50 pixels; the original animation sequence is retained. Art is packed into readable short C array lines, compiled inside the .fap; no image files or giant Base64 strings are required at runtime.

## Limits

- Input: 1024 bytes, output: 512 bytes.
- Brainfuck: 256 cells, wrapping 8-bit values, 100000 execution steps.
- Unmatched brackets, pointer boundaries, and exceeded limits produce an error; no XP is awarded.
- Brainfuck comma input remains a no-op, as in the supplied code.
- Auto mode attempts Hex, Base64 and Brainfuck once each in that order. It does not recursively unwrap arbitrary layers. Auto Brainfuck detection requires instruction-only text rather than treating ordinary punctuation as code. Explicit Brainfuck mode permits comments.
- Hex/Base64/Auto input whitespace is concatenated. Base64 requires valid padding; it is not guessed or repaired.
- Nonprintable output bytes appear as \\xNN. Output display uses ASCII; UTF-8 bytes are escaped.
- No ZIP extraction, wordlists, brute force, networking, script execution, voice. USB CLI supports desktop capture transfer and job status; wordlist computation remains on the PC.

## Quick real-input check

The examples folder contains optional .txt payloads. Copy these to any folder on the SD card. Decode 01 with Base64, 02 with Hex, 03 with Brainfuck, 04 with ROT13 and 05 with Auto. Outputs are Hello, ROBOT, A, Hello and Hello. From a fresh save, they total 12 XP: level 2 with 2 XP remaining. Repeating the same file in the same mode earns no XP. Exit and reopen to check saved progress.

## Build and verification

Install uFBT: `python -m pip install ufbt`. Run `ufbt` in this folder. application.fam explicitly lists only the three application C source files, excluding tests.

Host decoder checks: `cc -std=c11 -Wall -Wextra -Werror -fsanitize=address,undefined decoder.c tests/test_decoder.c -o test_decoder`, then run the binary. They cover known outputs, chained decoding, binary bytes, malformed input, tape/step/output limits, and 15000 bounded random cases. The supplied source notes report prior host checks and native compilation. This GitHub publication does not certify a new firmware build. Physical LCD appearance, keyboard behavior, SD persistence and prolonged runtime require checking on your Flipper; no physical-device test was performed here.

## USB highest-progress sync update

This version registers `megaman_sync <total_xp>` while BusterMan Decoder is running. It responds `[MEGAMAN_SYNC] XP=<highest> LV=<level> SAVE=<0|1>`. The higher total XP wins and is saved without changing the existing progress-file schema. Use the matching desktop update, open this app on Flipper, connect USB, and click Sync levels. It syncs level/XP only, not deduplication histories or meal counters. Physical USB integration has not been tested here.

## Final update

Right previews a selected text file before OK decodes it. Left from the menu shows PC job status. New meal/level/duplicate/error reactions are +, *, =, !.

The USB bridge lists saved `/ext/apps_data/marauder/pcaps/` captures and transfers bounded, CRC-checked chunks. CLI commands are present only while BusterMan is open. Saved files do not detect board presence; capture in Marauder first, stop it, then open BusterMan for transfer. See the desktop FINAL-UPDATE.md for workflow and limits.

Build against the exact firmware SDK: `ufbt update --hw-target f7 --local flipper-z-f7-sdk-mntm-012.zip`, then `ufbt`. SDK release: https://github.com/Next-Flip/Momentum-Firmware/releases/tag/mntm-012.

Host sync/bridge callbacks: `python tests/test_sync.py` and `python tests/test_bridge.py`. These simulate storage/USB, not physical hardware.
