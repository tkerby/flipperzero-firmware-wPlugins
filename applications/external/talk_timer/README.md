# Talk Timer

Presentation clicker with a discreet talk timer for the Flipper Zero.
The Flipper acts as a USB or Bluetooth keyboard, flips your slides and reminds
you of your speaking time by vibration, so nobody in the audience notices.

Not affiliated with Apple, Microsoft or Google.

## Features

- Next / previous slide, start presentation, black screen, end presentation
- Profiles for Keynote, PowerPoint (Windows / Mac), Google Slides (Windows / Mac)
  and a generic profile for PDF viewers (Page Down / Page Up)
- Talk timer with start, pause and reset, large remaining time, progress bar
  with warning marks, and a clearly visible overtime counter
- Two warnings before the end plus an end alert, each with its own vibration
  pattern: 1, 2 or 3 long pulses. Optional repeated alert during overtime
- No sound by default
- Five time presets that also store their warning times
- Optional "keep display on" while the timer runs
- Protection against accidents during a talk: leaving needs a long press on
  Back, and a confirmation once the timer has been started
- English and German UI
- USB mode and Bluetooth profile are restored on exit

## Controls on the main screen

| Button | Short press | Long press |
|--------|-------------|------------|
| Right / Left | Next / previous slide (exactly one key press, no auto repeat) | |
| OK | Start / pause timer. With Autostart (default on) the first start also starts the presentation | Reset timer (only while it is not running) |
| Up | Black screen on / off | Start presentation |
| Down | Show remaining or elapsed time | End presentation (Esc) |
| Back | Shows a hint only | Leave (asks for confirmation once the timer was started) |

In the menus, Left means "less" and Right means "more". Left also leaves the
presets menu, and leaves the settings on a row without a value.

## Profiles and test status

All shortcuts come from the vendors' official help pages (retrieved
2026-09-17); the sources are listed in `tt_profiles.c`. Only layout independent
keys are used (arrows, Page Up / Down, F5, Return, Escape, period, B, P), so
the host keyboard layout (QWERTZ / QWERTY) does not matter.

| Profile | Next / Prev | Start | Black | End | Status |
|---------|-------------|-------|-------|-----|--------|
| Keynote | Right / Left | Option+Cmd+P | B | Esc | Tested on macOS with Keynote 15.3.1 over USB |
| PowerPoint Mac | Right / Left | Cmd+Return | . | Esc | Untested, follows Microsoft's documentation |
| PowerPoint Win | Right / Left | Shift+F5 | . | Esc | Untested, follows Microsoft's documentation |
| Google Slides Mac | Right / Left | Cmd+Enter | . | Esc | Untested, follows Google's documentation |
| Google Slides Win | Right / Left | Ctrl+F5 | . | Esc | Untested, follows Google's documentation |
| Generic / PDF | Page Down / Up | none | none | Esc | Untested, common clicker convention |

Windows has not been tested at all; I only own a Mac.

Note for Keynote: Option+Cmd+P is a toggle. If the presentation is already
playing, it stops it. Talk Timer remembers that it started the presentation
and does not send the shortcut twice, but if you started the presentation by
hand, turn Autostart off or let the Flipper start it.

## Usage

1. Open Apps > Tools > Talk Timer.
2. Pick talk time (or a preset), profile and connection, then "Start talk".
3. USB: plug the Flipper into the computer. Bluetooth: pair the device named
   "Talk <your Flipper name>" once in the computer's Bluetooth settings.
4. Bring the presentation app to the front and press OK.

While the main screen is open in USB mode the Flipper is a keyboard, so
qFlipper and ufbt cannot reach it. Leave the main screen first.

Settings are stored in `/ext/apps_data/talk_timer/settings.txt`. The app writes
nowhere else.

## Install

Download the `.fap` that matches your firmware from the
[releases page](https://github.com/Mableton/talk_timer/releases) and copy it to
`/ext/apps/Tools/` on the SD card, or build it yourself.

## Build

    ufbt            # build against the SDK that ufbt currently uses
    ufbt launch     # build, install and start on the connected Flipper

The release contains two builds: one for Momentum firmware mntm-012 and one
for the official firmware. Only APIs of the official SDK are used.

## AI disclosure

The code in this repository was generated with an AI coding assistant
(Claude Code) and directed, reviewed and tested on the device by the author.

## License

GPL-3.0, see [LICENSE](LICENSE).
