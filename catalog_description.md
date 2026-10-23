Turn your Flipper Zero and an ESP32 WiFi devboard into a pocket radio station.

Pick an MP3 off your SD card and the Flipper streams it to the devboard, which broadcasts it as its own WiFi network. Up to eight people nearby can join that network, open a browser, and hear what you are playing. Live, in sync, with nothing to install.

## What you need

- A Flipper Zero
- An ESP32-S2 WiFi devboard, such as the official Flipper WiFi devboard
- The companion firmware from the project page, flashed to the devboard once

## How it works

Neither chip decodes audio. The Flipper reads the file off the SD card and paces the bytes out over the GPIO header at exactly the track's own bitrate, and that bitrate is the clock that keeps the stream in real time. The devboard re-serves those bytes as a live audio stream over its own access point, and each listener's browser does the decoding.

Because it is a real broadcast rather than a file transfer, everyone hears the same moment at the same time. Join late and you come in wherever the track has got to, exactly like tuning into a radio station.

## On the Flipper

- Choose any MP3 on the SD card
- Pause and resume without losing your place
- Watch the listener count, link health and buffer level while it plays
- Rename your station in Settings, so people see your name in the WiFi list

**Up to eight listeners at once.** The web player shows how many of those eight slots are in use, so everyone can see whether there is still room.

## For listeners

Join the WiFi network named Flipper Radio and open 192.168.4.1 in any browser, then press Listen. Phones show the sign-in sheet by themselves when they join, and it says where to go. Playback keeps running while you use other apps.

Constant bitrate MPEG1 Layer III files only.
