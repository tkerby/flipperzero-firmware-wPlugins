# Ghost Tracker

A paranormal-investigation instrument for the Flipper Zero. Every reading comes from
real hardware (Sub-GHz radio, ADC, a Goertzel filter, raw RF audio) — nothing is a
random-number simulation.

Three tools, plus an in-app wiring reference:

- **EMF Meter** — ambient RF scanner (built-in, no wiring) or an external Hall sensor
  with a real 50/60Hz mains-hum filter
- **Spirit Box** — real Sub-GHz frequency sweep with the raw demodulated RX static
  played through the speaker
- **REM Pod** — capacitive proximity detector via a bare copper-wire antenna

Targets **Unleashed Firmware**, built with `ufbt`.

## Build

```bash
cd ~/flipper-zero-apps/ghost_tracker
source ~/flipper-zero-apps/.venv/bin/activate
ufbt            # build
ufbt launch     # build + install + run on a connected Flipper
```

The SDK is pinned to Unleashed (not the default official-firmware channel):

```bash
ufbt update --hw-target f7 --url "https://github.com/DarkFlippers/unleashed-firmware/releases/download/<tag>/flipper-z-f7-sdk-<tag>.zip"
```

## How each module actually works

### EMF Meter

Toggle source with **◀ ▶**.

- **Internal** (no wiring): reuses the CC1101 Sub-GHz radio to round-robin 8 fixed
  frequencies across 300.00–348.00MHz and read real RSSI. Deliberately does **not**
  use the NFC chip — its field-detect circuit only reacts to an *active* 13.56MHz
  reader nearby (a phone, another reader), never to ambient fields, so it never
  moves in practice.
- **External** (optional): a linear Hall sensor (SS49E) wired to pin 16. Rather than
  a raw DC voltage deviation, the sensor is sampled at 640Hz and run through a
  **Goertzel filter** at 50Hz and 60Hz (keeping whichever is stronger) — the same
  physical quantity a real K-II meter's induction-coil + bandpass front end
  reports, not an approximation.

```
SS49E bare-IC pinout (3.0-6.5V):         Wiring to the Flipper:
  ___________                              OUT -> Pin 16 (PC0, ADC)
 | o   o   o |   1 = VCC                   VCC -> Pin 9  (3V3)
 |___________|   2 = GND                   GND -> Pin 11 (GND)
   1   2   3     3 = OUT
  VCC GND OUT

If yours is a breakout module (not the bare IC), follow the silkscreen
labels (S/OUT, -/GND, +/VCC) — physical pin order varies by manufacturer.
```

### Spirit Box

A real SB7-style spirit box sweeps AM/FM stations every 100-350ms and injects
synthetic noise between steps. The Flipper has no broadcast radio receiver, but it
does have a real one for Sub-GHz: `furi_hal_subghz_set_async_mirror_pin()` mirrors
the CC1101's raw demodulated RX bitstream directly onto the speaker's GPIO pin, in
hardware — real RF static, not a synthesized tone. Same technique the community
"Radio Scanner" app uses.

- Sweeps 300.00–348.00MHz, dwell time adjustable with **+/-** (100-500ms)
- Flashes "SEÑAL!"/lights the LED when RSSI crosses a real signal threshold
- **OK** toggles mute (there's no analog volume for a raw digital bitstream — it's
  either connected to the speaker or not)

### REM Pod

```
Antenna: bare rigid copper wire, no extra components
  Wire -> Pin 15 (PC1, ADC)
  Suggested length: 15cm (longer = more sensitive)
```

A floating high-impedance ADC node picks up real ambient 50/60Hz electric-field
coupling; a nearby body/object changes that coupling. 24 raw back-to-back ADC
samples are taken per tick (no oversampling — we want the jitter, not an average),
and the peak-to-peak spread is the signal. Same physical principle a real REM Pod
uses (a high-Z antenna node), not an RC charge-time circuit.

- **◀ ▶** sensitivity, **▲ ▼** alert volume
- On alert: LED strobes + continuous tone; the screen text changes but no longer
  flashes/inverts (kept readable on purpose)

### Pin map

| Pin | Signal | Used by |
|---|---|---|
| 9 | 3V3 | EMF external sensor power |
| 11 / 18 | GND | EMF external sensor / general ground |
| 15 (PC1) | ADC | REM Pod antenna |
| 16 (PC0) | ADC | EMF external Hall sensor |

## Future: optional TEA5767 FM module for Spirit Box

Not implemented yet (user is buying the module later) — researched and planned so
it's ready to build once the hardware is in hand.

A TEA5767 would let Spirit Box sweep **real FM broadcast** (76-108MHz) with actual
audible content (voices/music), controlled over I2C from the Flipper:

```
TEA5767          Flipper
  VCC    ----->  Pin 9  (3V3)
  GND    ----->  Pin 18 (GND)
  SCL    ----->  Pin 16 (PC0)   *shares a pin with the EMF Hall sensor*
  SDA    ----->  Pin 15 (PC1)   *shares a pin with the REM Pod antenna*
```

**Important finding:** in the reference implementation, FM audio does **not** route
through the Flipper's speaker at all — the real signal chain is
`TEA5767 L/R → PT2257/PT2259 (I2C volume) → PAM8406 amp → its own dedicated speaker`.
The Flipper only sends I2C tuning/volume commands; there's no feasible software
bridge to pipe that analog audio into the Flipper's own internal buzzer in real
time.

Two implementation tiers when the module arrives:

1. **Minimal (recommended first step):** bare TEA5767 (~$2) + earbuds/small speaker
   wired directly to its L/R output. Flipper does I2C tuning only. No PT2257/PAM8406
   needed — just real FM audio through a separate speaker.
2. **Full-featured:** add a PT2257 (I2C volume control) and a PAM8406 (amp) to match
   the reference project, for I2C-controlled volume and RDS station-name decode.

Since pins 15/16 are already used by the REM Pod antenna and EMF Hall sensor, the
TEA5767 will need either different free pins or a time-shared/swappable wiring
setup.

## Sources

Code and design decisions in this app were grounded against real SDK headers, real
hardware behavior (verified via live device testing and screen capture — see
`tools/capture_screen.py`), and the following external references:

**Firmware / SDK**
- [DarkFlippers/unleashed-firmware](https://github.com/DarkFlippers/unleashed-firmware) — target firmware; SDK pinned to this release channel instead of the default official-firmware one

**Techniques referenced/adapted**
- [RocketGod-git/Flipper-Zero-Radio-Scanner](https://github.com/RocketGod-git/Flipper-Zero-Radio-Scanner) — confirmed the `furi_hal_subghz_set_async_mirror_pin` technique for playing real demodulated RF audio through the speaker, used in Spirit Box
- [hnsvrdmn/flipper-rfid-metal-detector](https://github.com/hnsvrdmn/flipper-rfid-metal-detector) — researched the 125kHz LF-RFID coil as a possible passive EMF sensor; confirmed it only works as an *active* eddy-current metal detector, which ruled it out for the EMF Meter's "internal" mode
- [pchmielewski1/tea5767-pt22xx-fm-radio-with-volume-control](https://github.com/pchmielewski1/tea5767-pt22xx-fm-radio-with-volume-control) — wiring reference and audio-chain finding for the planned future TEA5767 integration
- [evilpete/flipperzero_protobuf_py](https://github.com/evilpete/flipperzero_protobuf_py) — Python RPC client used in `tools/capture_screen.py` to pull real screenshots over USB serial (same protocol qFlipper's "Remote Control" uses) for visually verifying UI changes during development

**Menu ghost icon**
- [Lucide icons — "ghost"](https://lucide.dev/icons/ghost) ([lucide-icons/lucide](https://github.com/lucide-icons/lucide), MIT) — reference for the round dome, straight sides, and zigzag feet replicated with plain canvas primitives (disc/box/triangle) in `view_menu.c`
- [Twitter/Twemoji](https://github.com/twitter/twemoji) ghost emoji (`1f47b.svg`) — also researched as a reference, not the style ultimately used

**Official Flipper docs**
- [docs.flipper.net/zero/gpio-and-modules](https://docs.flipper.net/zero/gpio-and-modules) and [docs.flipper.net/zero/rfid](https://docs.flipper.net/zero/rfid) — GPIO pinout and LF-RFID reference

**Background research (general articles, not code)**
- K-II/EMF meter internals: coil + ELF/VLF bandpass filter design, informed the Goertzel-filter approach in the EMF Meter's external mode
- SB7-style spirit box sweep/noise behavior, informed the Spirit Box's sweep timing and audio approach
- SS49E Hall sensor datasheet, for the bare-IC pinout and baseline voltage used in the About screen's wiring diagram
