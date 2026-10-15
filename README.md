# Flipper Radio

A pocket WiFi radio station. The Flipper Zero (with the official ESP32-S2 WiFi
devboard attached) broadcasts a WiFi network; anyone who joins on their phone
hits a captive portal that opens a player page streaming live music. MP3s live
on the Flipper's SD card; every listener hears the same live stream (true radio
semantics), which keeps bandwidth trivially feasible.

```
[Flipper FAP]                       [ESP32-S2 firmware]                [Phones]
MP3s on SD card                     WiFi softAP  (station = SSID)       join WiFi
station console UI    --UART-->     DNS captive portal -> 192.168.4.1   portal pops
reads + paces bytes   921600 baud   HTTP: / player, /stream, /status    tap play
                      <--stats--    fans ONE stream to all listeners     hear stream
```

## Layout

- `flipper-app/`   — the FAP (station console), built with `ufbt`.
- `esp32-firmware/`— ESP-IDF project (AP + captive portal + stream fan-out).
- `protocol.md`    — the UART protocol spec shared by both halves.
- `tools/`         — helper scripts (music conversion).

## Build (once toolchains are set up)

```sh
# ESP32 firmware
cd esp32-firmware
. ~/esp/esp-idf/export.sh
idf.py set-target esp32s2
idf.py build
idf.py -p <devboard-port> flash        # hold BOOT on first flash

# Flipper app
cd ../flipper-app
ufbt                                    # build -> dist/flipper_radio.fap
ufbt launch                             # build + install to a connected Flipper
```

## Build phases

0. **Toolchain + scaffold** (this) — projects build empty.
1. **Phone experience** — ESP serves an embedded MP3 loop over the captive portal.
2. **First end-to-end radio** — Flipper streams one SD-card file over UART.
3. **Radio features** — playlist, now-playing, listener count, resilience.
4. **Polish** — player-page design, station-name setting, icon, docs.

## Notes

- Music must be **128 kbps CBR MP3**. `tools/convert_music.sh` normalizes any input.
- Flashing the devboard **replaces its stock Black Magic firmware** — restore it
  anytime at lab.flipper.net.
- The devboard and Video Game Module share the Flipper's GPIO header — only one
  can be attached at a time.
