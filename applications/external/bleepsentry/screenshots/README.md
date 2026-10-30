# Screenshots

All four images are 512x256 PNGs in the same format qFlipper's screenshot
feature produces for a 128x64 Flipper Zero frame:

- landscape, no rotation, no borders or annotations
- exactly two colours: foreground black (0,0,0) on the Flipper orange page (254,138,44)
- nearest-neighbour pixels, no anti-aliasing, no alpha channel

They were captured from the device itself, running official firmware 1.5.1
(API 88.2), with the app driven over the Flipper RPC input API so each frame
is a real state of the running app rather than a mock-up:

- `idle.png` - launched, no baseline yet, 433.92 MHz
- `watch_calibrated.png` - Watch mode after a baseline calibration
- `sweep_calibrating.png` - Sweep mode calibrating the band list
- `sweeping.png` - Sweep mode cycling the calibrated bands

Each file was checked after capture: 512x256, two colours only, and layout
identical to a fresh capture of the same build. Values that differ between
captures (RSSI, baseline, threshold, log count) are the live readings of the
moment the frame was taken, not edited numbers.
