# WM-Bus Decoder

Receive and decode Wireless M-Bus meter telegrams at **868.95 MHz** with
Flipper Zero's built-in Sub-GHz radio. Supports **T and C modes**, live packet
inspection, history, and optional CSV logging to microSD. No external module is
required.

## Getting Started

Use official Release or Release Candidate firmware and a microSD card.

1. Build with `ufbt`, then copy `dist/wmbus_decoder.fap` to `/ext/apps/Sub-GHz/`
   using qFlipper, or install and launch over USB with `ufbt launch`.
2. Open **Apps > Sub-GHz > WM-Bus Decoder**.
3. Start in **T** mode near a compatible meter. Press **Left** to configure
   receive mode, quality/RSSI filters, logging, or decryption keys.

Reception depends on range and the meter's transmission schedule.

## Controls

| Button | Action |
| --- | --- |
| OK | Switch between Live and History |
| Up / Down | Browse History |
| Left | Open configuration |
| Right | Inspect the displayed packet |
| Long Up | Toggle debug overlay |
| Long Down | Open decoded application records |
| Back | Exit; confirm if packets are held in memory |

## Decoding and Keys

Parsers support selected Qundis QWater v2 and Apator telegrams, with a generic
DIF/VIF fallback. Decryption supports **TPL Mode 5 (AES-128-CBC)** using keys you
supply. Unsupported formats retain frame metadata. C-mode Frame B reception
still needs live on-air validation. The app receives only.

Enter keys in configuration or put them in
`/ext/apps_data/wmbus_decoder/keys.txt`: one 32-character hexadecimal AES-128 key
per line, up to 16 keys. Lines starting with `#` are ignored. Only supplied keys
are tried.

Settings and dated CSV logs also live in `/ext/apps_data/wmbus_decoder/`.
Logging offers **Off**, **Basic** summaries, or **Full** wire bytes and decoded
fields. History and CSV have separate minimum quality gates and share an
optional RSSI threshold.

## Build and Development

```bash
ufbt
```

Output: `dist/wmbus_decoder.fap`. Normal builds exclude on-device selftests.

See [development documentation](docs/development.md) for architecture, parser
extensions, protocol references, and selftests, or the
[receive pipeline](docs/receive-pipeline.md) for radio and framing details.

Inspired by [wmbusmeters](https://wmbusmeters.org/).
Licensed under [GPL v3.0](LICENSE). See the [changelog](changelog.md) for releases.
