# Kota

Shows your Claude plan usage on the Flipper: the 5-hour session limit and the weekly limit, each with a live countdown to its reset.

The numbers come from a small companion script on your computer, sent over Bluetooth. No cable needed.

## Setup

1. On your Mac, with Claude Code logged in, run the companion script from https://github.com/cobanov/kota:
   `uv run host/kota.py`
2. Open Kota on the Flipper. It links within a few seconds.

## Screen

- Session / Weekly: used percentage and time until the window resets.
- Top right: "live" when the last update is under a minute old, otherwise its age. The dot is filled while the computer is connected.

## Controls

- Back: exit

While Kota is open, the Flipper's normal Bluetooth connection (mobile app) is paused. It returns when you exit.
