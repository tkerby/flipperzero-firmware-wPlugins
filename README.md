# RF Survey

A logging spectrum survey tool for the Flipper Zero. Give it a frequency range, it sweeps
it, reads signal strength (RSSI) at every step, shows a live spectrum across several views,
logs every sweep to a CSV on the SD card, helps you walk a signal to its source, and can
record a located sub-GHz signal to a `.sub` you replay in the stock SubGHz app.

Think of it as the stock Frequency Analyzer, but it scans a range you choose, keeps a time
history, and logs everything for analysis on a PC. **Sub-GHz RX only — it never transmits.**

## What it is (and isn't)

- It **measures signal strength, location and burst timing** and **captures** static OOK/2FSK
  bursts. It never decodes a protocol: the Cadence page tells you *how* a signal behaves
  (periodic machine vs human press) and the capture screen hints at the *encoding family* from
  pulse widths, which narrows down what you are looking at without claiming to name it.
- The radio is a CC1101, **not an SDR**. It only covers the sub-GHz bands **300-348,
  387-464, 779-928 MHz**. So it **cannot** see Wi-Fi/Bluetooth (2.4 GHz) devices, and it
  **cannot** make sense of cellular (GSM/LTE) traffic even when that traffic is in band.
- Used honestly it's great for: locating a sub-GHz transmitter by watching a bin rise as you
  move, surveying which frequencies in a band are busy over time, and grabbing a remote.

## Features

- **Range config** — scroll a Band list (full-spectrum / full-band / ~26 known allocations,
  each tagged, e.g. 433 ISM, 868 ISM, PMR446, LTE800), or type an exact Start/End range in a
  Frequency-Analyzer-style editor. Pick modulation preset (AM650/AM270/FM238/FM476) and step.
- **Six live views**, flip with Left/Right:
  - **Bars** — grouped bars with per-bin peak-hold, a movable cursor, and a dotted trigger
    line. OK zooms into the cursor band and **narrows the RX bandwidth** so a carrier shows a
    sharp peak, not a wide mountain.
  - **Connected** — per-frequency average over a time window plus a dotted peak envelope.
  - **Waterfall** — horizontal time × frequency heat map with an adjustable window (5 s to
    10 min) and a time scale, so you can read how often and how regularly a band is busy.
  - **Numeric** — the strongest peaks (adjacent bins grouped into one), ranked by peak-hold;
    select one and capture it. When the step is finer than 100 kHz each row also shows the
    -6 dB occupied bandwidth, which separates a chirp (125/250/500 kHz) from a narrowband
    remote.
  - **History** — the session's busiest channels with hit counts, tagged by band.
  - **Cadence** — zero-span burst timing at one frequency: OK parks the radio there, Up/Down
    nudges it in 10 kHz steps, and the page reports period, jitter (CV + stddev), duty cycle
    and repeat-group size, classifying the source as crystal-locked **machine** or
    **human/event**. A swept survey samples each bin once per sweep, so this is the only view
    that can measure anything shorter than a sweep. A `~` in the header means an observation
    hole (a slow SD write, a preempted thread) landed in the last few seconds: the initial
    intervals and burst lengths that spanned it were discarded rather than averaged in.
- **Trigger** — a settable dBm floor that drives the waterfall, the haptic cue, capture and
  the burst segmentation on the Cadence page. Pressing OK on the Trigger row auto-calibrates
  it from the sweep's own 25th percentile (+8 dB).
- **Modulation hint** — on park entry the app spends 200 ms on AM650 and 200 ms on FM238 with
  each preset's own bandwidth and reports both contrasts (`AM27 FM4`). It is a hint, not a
  decision: RSSI measures channel power, so a sustained carrier scores ~0 on both families and
  a one-off button press may land in neither window. The preset you chose stays loaded.
- **Settle** — per-bin dwell (1-20 ms): the sweep-speed vs weak-signal trade-off.
- **Save waterfall** — writes the captured waterfall to a binary PGM with its frequency,
  step, preset, per-column timestamps and dBm-to-gray mapping in the header. Rows are
  max-folded and one column is one sweep, which the header states: it is a coarse activity map
  for a report, not a spectrum. **Clear peaks** re-seeds the peak-hold.
- **Haptic** — the Flipper vibrates when a bin clears the trigger (hands-free locating), more
  often the stronger it is.
- **Gain** — cap the CC1101 AGC so a strong nearby source doesn't saturate the front end.
- **CSV logging** — long-press OK on any view to record every sweep to a timestamped CSV
  (`/ext/apps_data/rfsurvey/survey_<ts>.csv`, one row per sweep) for analysis on a PC.
- **`.sub` capture** — from Numeric, lock the selected peak and record it (RSSI-gated, like
  the stock RAW recorder) to `/ext/subghz/`, replayable in the stock SubGHz app. When the
  capture stops, the screen shows the envelope's pulse-width signature: base pulse, the
  ratio to the second cluster, and the encoding family it implies (1:3 → PT2262/EV1527 class,
  1:2 → biphase, 1:1 → steady clock). A family hint, not a decoder.

## A note on region

To read signal strength across the full CC1101 range — including bands outside the firmware's
default region window — RF Survey installs a permissive region for its lifetime and restores
the previous one on exit. This only affects **receiving**: the app never transmits. Measuring
how strong a signal is, and where it comes from, is a passive RX operation.

## Testing without a Flipper

The parts that can be reasoned about without a radio live in `rf_logic.h`: the parked burst
state machine, the waterfall ring indexing, step fitting. `rf_survey.c` includes that header,
so the firmware and the tests run the **same code**, not a re-implementation.

```sh
cd tests && cc -std=c11 -Wall -Wextra -Werror -O2 -I.. logic_test.c -o /tmp/t && /tmp/t
```

Each case is a scenario that was a real bug or one bad decision away from one: a ring that
wraps, an observation hole landing inside a burst, a mid-burst stop, a repeat group, a
percentile on a tiny sample. Full-device emulation is not a substitute — the firmware can be
booted under Renode, but no emulator models the CC1101, which is the part that matters here.

## Analysing the logs on a PC

`tools/analyze.py` reads both formats the app writes and turns them into behaviour numbers:

```sh
./tools/analyze.py cadence_20261028_131500_042.csv      # period, jitter, duty, repeats
./tools/analyze.py survey_20261028_131500_042.csv -n 8  # busy channels + behaviour
./tools/analyze.py cadence_*.csv --plot cadence.png     # needs matplotlib
```

A cadence log is a time series at one frequency sampled at ~125 Hz, so burst period and jitter
are real measurements. A survey log samples each bin once per sweep, so its timing is a lower
bound by construction -- the script says so instead of pretending otherwise.

## Build & install

Built with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
ufbt            # build rfsurvey.fap into dist/
ufbt launch     # build, upload to a connected Flipper and run
```

Or copy `dist/rfsurvey.fap` to `/ext/apps/Sub-GHz/` on the SD card.

## Controls

- **Left / Right** — flip views (or move the Bars cursor / Numeric selection).
- **Up / Down** — time window (Connected/Waterfall), Numeric selection, or the parked
  frequency on the Cadence page (10 kHz steps, live).
- **OK (short)** — zoom into the cursor band (Bars) · park/release the selected frequency
  (Cadence) · open capture for the selected peak (Numeric).
- **OK (long)** — start/stop CSV logging (any view).
- **Back** — zoom out, then return to config; from config, exit.

## License

MIT — see [LICENSE](LICENSE).
