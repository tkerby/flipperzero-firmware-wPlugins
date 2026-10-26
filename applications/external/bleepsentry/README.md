# Bleepsentry

Passive RF anomaly sentry for Flipper Zero, built for two audiences:

- **Red team / physical pentest**: passive monitoring of a target frequency (e.g. a garage/gate remote) to detect unexpected transmissions — flags possible replay-attack activity or unauthorized transmitters during an engagement.
- **TSCM (counter-surveillance)**: a live RSSI meter across common ISM sub-GHz bands, used the way a professional bug sweeper uses a spectrum analyzer — establish a quiet baseline in a room, then get an alert the moment something starts transmitting above that baseline. Many covert bugs/transmitters operate in exactly this 300–928 MHz range.

## Why Sub-GHz, not Bluetooth

This project started as a BLE passive-scanning tool. During development I checked the actual Flipper Zero SDK headers (not assumptions) and confirmed the public SDK's BLE stack (`bt.h`, `gap.h`, `furi_hal_bt.h`) only exposes **peripheral/advertiser** functionality — starting/stopping advertising, GATT profiles, extra-beacon transmission. There is no GAP central/observer scan API, no advertisement-received event, and no raw HCI passthrough exposed to apps. Passive BLE device scanning is not buildable against the official, catalog-eligible Flipper firmware today. Rather than ship something that can't work, the project pivoted to Sub-GHz (CC1101), where `furi_hal_subghz` genuinely exposes RSSI readout and async RX — the same APIs every existing capture/spectrum app in the catalog already relies on.

## How it works

Two modes, both logging every alert to `/data/log.csv` (timestamp, frequency, baseline, threshold, peak RSSI, duration):

- **Watch mode** (short-press OK): calibrates a baseline on the *current* frequency only, then continuously monitors just that one frequency. Built for red-team use — watching one known frequency (e.g. a garage/gate remote) for replay-attack-style activity.
- **Sweep mode** (long-press OK): calibrates a baseline on *every* frequency in sequence, then continuously cycles through all of them, alerting and logging independently per frequency. This is the actual multi-band TSCM bug-sweep workflow.

Controls:
- **Up/Down**: switch to a specific frequency manually (drops out of Sweep mode; keeps any previously-calibrated baseline for that frequency)
- **OK (short)**: calibrate + watch the current frequency
- **OK (long)**: calibrate + sweep all frequencies
- **Back**: exit

## Status

Feature-complete for v1 and verified on real hardware (Flipper Zero, official firmware 1.5.1, API 88.2):

- Clean install and launch, no crash
- Watch-mode calibration runs to completion
- Sweep mode completes a full calibration + continuous cycle across all 4 frequencies without crashing
- Clean exit via a physical Back press, confirmed via on-device thread/loader inspection (the app's thread fully tears down, loader returns to the main menu)

Builds clean against SDK API 88.2 on both `dev` and `release` channels (verified in CI on every commit). Passes `ufbt lint`. The pure logic (threshold math, alert state transitions, CSV formatting) is unit-tested on the host in `tests/` (also run in CI) — see `tests/test_logic.c`.

One known, non-defect quirk found during testing: remotely closing the app via a tool's "close current app" RPC request (e.g. re-running `ufbt launch`) returns "has to be closed manually." This is expected for apps built on the plain `ViewPort` pattern (what `ufbt create`'s own boilerplate generates, as this app is) rather than the `ViewDispatcher`/scene framework — only the latter supports a remote-close hook. A real physical Back press is unaffected and works correctly, as verified above.

## Screenshots

![Idle, no baseline yet](screenshots/idle.png)
![Watch mode after calibration](screenshots/watch_calibrated.png)
![Sweep mode calibrating across frequencies](screenshots/sweep_calibrating.png)
![Sweep mode actively watching](screenshots/sweeping.png)

## Running the host-side logic tests

```
cd tests
gcc -std=c11 -Wall -Wextra -I.. test_logic.c ../bleepsentry_logic.c -lm -o test_logic
./test_logic
```

## Hardware

Stock Flipper Zero only. No add-on boards required (CC1101 radio is on-board).

## Intended use

Built for authorized security engagements (penetration tests, red team exercises, TSCM/bug sweeps under contract) and for sweeping your own spaces. It only listens — it never transmits.

## Build

```
pip install ufbt
ufbt
ufbt launch   # with a Flipper connected over USB
```
