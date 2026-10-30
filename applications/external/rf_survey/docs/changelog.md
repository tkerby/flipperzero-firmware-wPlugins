## 1.3
Ported from the wider tool ecosystem (PortaPack Mayhem, rtl_433, URH), plus the measurement
discipline that came out of reviewing them.

- **Channel occupancy** replaces the busy-channel history. The old counter kept ONE winner per
  sweep, so a strong carrier hid every weaker active channel; now every bin counts how many
  recent sweeps saw it above the trigger, in a 255-wide rolling window (halved, never
  overflowed). The History page shows real percentages.
- **Prominence peak picking** (Mayhem's NxtPk rule): a peak must stand 3 dB above the lowest
  point between it and the next higher peak. The old 400 kHz grouping was a guess about the
  filter width; this measures the valley.
- **Delta markers** on the Numeric page: peaks below the strongest are labelled with their
  frequency offset and dB below it instead of repeating the band name.
- **Min-hold trace** and a **Trace** selector (Live / Max hold / Min hold) on the Bars page.
  A channel that never drops is a continuous carrier; one that does was only passing traffic.
- **HOLD** (long Up) freezes the traces so you can compare before/after; **long Down** clears
  both holds.
- **Bookmarks** from `/ext/apps_data/rfsurvey/freqs.txt`, one `name, start_hz, end_hz` per line.
- **Protocol decoding** on capture, via the firmware's own decoder chain (`subghz_receiver_*`,
  verified to be exported to FAPs): the decoded string replaces the pulse-width guess when the
  protocol is recognised.
- **Pre-trigger history** on gated captures: the 256 edges before the RSSI gate opened used to
  be discarded; they are now written first, so the preamble and first frame survive.
- **Allan deviation** (tau = one sample) alongside the period, a stability estimator that
  cancels slow drift instead of counting it as jitter.
- **Settle-too-fast warning** (`!`) when the dwell is below what the filter width needs.
- Hidden in the list above: the burst state machine, ring indexing and step fitting moved to
  `rf_logic.h`, which `tests/logic_test.c` compiles and exercises on a PC. The first run found
  a real defect (tick 0 was treated as "no previous sample", losing the first period after
  boot).

## 1.3
Ported from the wider tool ecosystem (PortaPack Mayhem, rtl_433, URH), plus the measurement
discipline that came out of reviewing them.

- **Channel occupancy** replaces the busy-channel history. The old counter kept ONE winner per
  sweep, so a strong carrier hid every weaker active channel; now every bin counts how many
  recent sweeps saw it above the trigger, in a 255-wide rolling window (halved, never
  overflowed). The History page shows real percentages.
- **Prominence peak picking** (Mayhem's NxtPk rule): a peak must stand 3 dB above the lowest
  point between it and the next higher peak. The old 400 kHz grouping was a guess about the
  filter width; this measures the valley.
- **Delta markers** on the Numeric page: peaks below the strongest are labelled with their
  frequency offset and dB below it instead of repeating the band name.
- **Min-hold trace** and a **Trace** selector (Live / Max hold / Min hold) on the Bars page.
  A channel that never drops is a continuous carrier; one that does was only passing traffic.
- **HOLD** (long Up) freezes the traces so you can compare before/after; **long Down** clears
  both holds.
- **Bookmarks** from `/ext/apps_data/rfsurvey/freqs.txt`, one `name, start_hz, end_hz` per line.
- **Protocol decoding** on capture, via the firmware's own decoder chain (`subghz_receiver_*`,
  verified to be exported to FAPs): the decoded string replaces the pulse-width guess when the
  protocol is recognised.
- **Pre-trigger history** on gated captures: the 256 edges before the RSSI gate opened used to
  be discarded; they are now written first, so the preamble and first frame survive.
- **Allan deviation** (tau = one sample) alongside the period, a stability estimator that
  cancels slow drift instead of counting it as jitter.
- **Settle-too-fast warning** (`!`) when the dwell is below what the filter width needs.
- Hidden in the list above: the burst state machine, ring indexing and step fitting moved to
  `rf_logic.h`, which `tests/logic_test.c` compiles and exercises on a PC. The first run found
  a real defect (tick 0 was treated as "no previous sample", losing the first period after
  boot).

## 1.2
