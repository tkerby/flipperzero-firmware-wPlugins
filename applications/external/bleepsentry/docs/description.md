# Bleepsentry

Passive RF anomaly sentry, built for two audiences:

- **Red team / physical pentest**: passive monitoring of a target frequency (for example a garage or gate remote) to detect unexpected transmissions. Flags possible replay-attack activity or unauthorized transmitters during an engagement.
- **TSCM (counter-surveillance)**: a live RSSI meter across common ISM sub-GHz bands, used the way a professional bug sweeper uses a spectrum analyzer. Establish a quiet baseline in a room, then get an alert the moment something starts transmitting above that baseline.

## How it works

Two modes, both logging every alert (timestamp, frequency, baseline, threshold, peak RSSI, duration):

- **Watch mode** (short-press OK): calibrates a baseline on the current frequency only, then continuously monitors just that one frequency.
- **Sweep mode** (long-press OK): calibrates a baseline on every frequency in sequence, then continuously cycles through all of them, alerting and logging independently per frequency.

Controls: Up and Down switch frequency manually. OK (short) calibrates and watches the current frequency. OK (long) calibrates and sweeps all frequencies. Back exits.

## Intended use

Built for authorized security engagements (penetration tests, red team exercises, TSCM and bug sweeps under contract) and for sweeping your own spaces. It only listens, and never transmits.

## Hardware

Stock Flipper Zero only. No add-on boards required.
