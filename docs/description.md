# Jiggler

A mouse jiggler that keeps a computer awake without getting in your way.

## Features

- **Invisible moves:** each move is a 1 px nudge and an immediate nudge back, so the computer sees mouse input but the pointer never visibly leaves its spot. Other patterns: a random-direction wiggle, a tiny square, or a tap of the F15 or Shift key
- **Random timing:** set a shortest and a longest wait, from 5 seconds to 10 minutes, and each move comes at a random moment in between
- **Pauses while you work:** with the desktop helper running, the Flipper knows how long ago you last touched the real mouse or keyboard and holds off until you have been idle for a while. It also knows when the screen is locked
- **Run timers:** stop after a set time, or at a time of day from the Flipper's clock
- **Status at a glance:** what the jiggler is doing, when the next move is due, moves made, time running, and what the helper reports
- **Settings are saved** to the SD card

Without the helper the jiggler still works, it just cannot tell when you are busy.

## Controls

- OK: start or stop
- Right or Up: settings
- Back: quit, which also gives the USB serial port back

In settings, Up and Down pick a line, Left and Right change it, Back returns.

## The desktop helper

A small Python script, helper/jiggler_helper.py in the repository. Once a second it tells the Flipper how long the computer has been idle and whether the screen is locked, and shows the Flipper's status on one line. It needs the hidapi package and works on macOS, Windows and Linux with X11. It talks to the Flipper over a separate USB HID interface, so it needs no special permissions.

## Notes

While the app is open, the Flipper's USB serial port is replaced by the mouse, so qFlipper and the mobile app cannot talk to it. Quit the app to get the port back.
