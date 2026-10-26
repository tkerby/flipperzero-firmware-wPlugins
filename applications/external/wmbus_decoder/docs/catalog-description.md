# WM-Bus Decoder

Receive and inspect Wireless M-Bus meter telegrams at 868.95 MHz using Flipper
Zero's built-in Sub-GHz radio. No external radio module is required.

## Features

- T and C mode reception.
- Frame length and CRC validation, live display, and packet history.
- Packet details and decoded application records.
- Targeted parsing for supported Qundis QWater v2 and Apator telegrams, plus
  generic parsing of standard DIF/VIF records.
- Mode 5 AES-128 decryption with keys supplied by the user.
- Optional CSV logging to microSD, packet quality filters, and an RSSI gate.

## Controls

- OK: switch between Live and History.
- Up and Down: browse packets in History.
- Left: open configuration.
- Right: inspect the displayed packet.
- Long Up: toggle the debug overlay.
- Long Down: open decoded application records.
- Back: exit, with confirmation if packets are held in memory.

## Setup

Start in T mode near a compatible meter. Use configuration to select C mode
when needed. Reception depends on range and the meter's transmission schedule.

Settings and logs are stored under /ext/apps_data/wmbus_decoder/ on microSD.
Optional AES keys belong in keys.txt in that directory: one 32-character
hexadecimal AES-128 key per line, up to 16 keys. Keys can also be entered from
configuration. Only supplied keys are tried.

## Limitations

The app receives only; it does not transmit or pair. Device-specific decoding
and security support are limited to supported telegram formats. Unsupported
telegrams may show only frame metadata. C mode Frame B reception still needs
live on-air validation.

## Support

See the [project README](https://github.com/maresmar/wmbus-decoder#readme) for
detailed usage and development instructions. Report issues on
[GitHub](https://github.com/maresmar/wmbus-decoder/issues).
