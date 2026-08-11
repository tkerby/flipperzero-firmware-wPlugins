# WM-Bus Decoder

`wmbus_decoder` is a receive-only Flipper Zero Sub-GHz app for Wireless M-Bus at `868.95 MHz`.

## Supported Features

- WM-Bus receive pipeline with `T` and experimental `C` mode capture paths
- packet validation with plausibility, length, and CRC checks
- packet history on device with quality-based retention threshold and optional RSSI gate
- optional dated CSV logging with quality-based CSV threshold
- optional AES key loading from `keys.txt`
- generic DIF/VIF parsing for standard application records
- targeted device parsing for supported telegram families
- opt-in test builds write a startup selftest report to `/ext/apps_data/wmbus_decoder/selftest.txt`

### Packet flow

The mode-specific radio and capture contract is documented in
[Receive Pipeline](docs/receive-pipeline.md).

1. radio RX captures raw telegram data in `T` or `C` mode
2. capture code reconstructs a candidate WM-Bus frame for the selected mode
3. packet processing validates plausibility, length, and CRC
4. packet metadata, identity fields, and the authoritative wire bytes are stored in `WmBusPacketRecord`
5. normalized and optionally decrypted payload views remain transient during application parsing
6. registered parsers inspect the packet and populate application-level fields
7. the result is routed to live view, history storage, and optional CSV logging
8. history and CSV retention apply optional RSSI and packet-quality gates

## Current Limitations

- `T` and `C` mode Frame A processing is validated against real rx captures; the C radio profile with frame B handling still need live on-air validation
- the app is receive-only; it does not transmit or pair
- device-specific parsing is limited; unsupported telegrams fall back to generic decode or remain undecoded
- decryption support is limited to what the shared packet path and registered parsers can validate

## Parsers

Currently registered application parsers:

- `Apator162`: targeted device parser for supported Apator short-TPL telegrams
- `DifVif`: generic parser for standard DIF/VIF application records

Parser registration is order-dependent. The first parser whose `parse()` succeeds wins, so device-specific parsers must stay ahead of generic fallbacks such as `DifVif`.

## Adding A Parser

New parsers live under `applications_user/wmbus_decoder/protocol/parser/`.

Minimum steps:

1. add a new parser ID in `wmbus_application_types.h`
2. add a fail-closed `parse(const WmBusParserPacketView*, WmBusPacketApplicationData*)` implementation
3. add its display name to the indexed table in `wmbus_parser.c`
4. include and register it in `wmbus_parser_registry.c` with its `parser_id`
5. place it before any broader parser it should override
6. add or update selftests in `src/test/`

Parser inputs come through `WmBusParserPacketView`, which exposes DLL, TPL, payload, and identity data from the shared packet pipeline. New parser tests should exercise the full packet path through `wmbus_packet_process_phy_frame(...)` rather than calling parser internals directly.

## Runtime Controls

- `OK`: switch between `Live` and `History`
- `Up` / `Down`: browse saved packets in `History`
- `Left`: open config
- `Right`: open packet detail for the selected history entry
- `Long Up`: toggle the debug overlay
- `Long Down`: open application records for the selected history entry
- `Back`: exit immediately when no packet is in memory; otherwise confirm before discarding packets

## Configuration

Available settings:

- RX mode: `T` / `C`
- CSV logging: `Off` / `Basic` / `Full`
- memory quality threshold: `MEM gate >=`
- CSV quality threshold: `CSV gate >=`
- RSSI gate: `Off` or a negative dBm threshold
- keyring status and key entry
- About screen with author, repository, and build commit information

Files used by the app:

- settings: `/ext/apps_data/wmbus_decoder/settings.txt`
- keys: `/ext/apps_data/wmbus_decoder/keys.txt`
- selftest report: `/ext/apps_data/wmbus_decoder/selftest.txt`
- CSV basic: `/ext/apps_data/wmbus_decoder/packets_YYYYMMDD_basic.csv`
- CSV full: `/ext/apps_data/wmbus_decoder/packets_YYYYMMDD_full.csv`

## Key File

Optional AES keys are loaded from:

`/ext/apps_data/wmbus_decoder/keys.txt`

Format:

```text
00112233445566778899AABBCCDDEEFF
```

- one AES-128 key per line as 32 hex characters
- lines starting with `#` are ignored
- only keys explicitly listed in this file are tested; no keys are derived or brute-forced
- up to 16 keys are tried in file order, stopping at the first result with the Mode 5 `2F 2F` decryption-verification field required by EN 13757-7/OMS

## Protocol Sources

- EN 13757-3 and EN 13757-4 are normative for supported WM-Bus frame, transport, and security behavior.
- OMS documents are used only for extensions they define.
- The [TI CC1101 errata](https://www.ti.com/lit/er/swrz020e/swrz020e.pdf) is authoritative for RX FIFO handling.
- Proprietary Apator behavior is maintained from tested packet fixtures.

## Build

From this app directory:

```bash
ufbt
```

Output:

`dist/wmbus_decoder.fap`

## Tests

Normal builds exclude the on-device test suite. Build, deploy, and run the test variant with:

```bash
WMBUS_SELFTESTS=1 ufbt launch
```

The test build writes its startup report to:

`/ext/apps_data/wmbus_decoder/selftest.txt`

After building and launching the app on a device, fetch the report with the Flipper CLI:

```bash
ufbt cli
```

Then run:

```text
storage read /ext/apps_data/wmbus_decoder/selftest.txt
```

Expected result ends with:

```text
selftests done total=<N> passed=<N> failed=0
```

Rebuild production mode after testing so `dist/wmbus_decoder.fap` does not contain selftests:

```bash
ufbt
```

## Credits

- Inspired by [wmbusmeters](https://wmbusmeters.org/)
