v1.0.0:
- Initial release
- BTHome v1 and v2 beacon simulation
- iBeacon, Eddystone TLM, ATC1441, and MiBeacon simulation
- Real Flipper battery and temperature telemetry
- Simulated humidity and iBeacon fields
- Info and Help screens with scrolling
- Startup and exit splash screens
- Exit confirmation menu
- Fixed BLE beacon crash caused by missing beacon configuration

1.1.0:

Added

- OEPL Tag beacon type with ATC BLE OEPL compatibility (`0x1337`, protocol v2).
- Short advertised names: `FZ BTH2`, `FZ BTH1`, `FZ`, `FZ EDDY`, `FZ ATC`, `FZMI`, `FZ OEPL`.
- Added two iBeacon modes:
  - `iBeacon Tag`: standard iBeacon advertisement with Flags and no device name.
  - `iBeacon FZ Tag`: iBeacon advertisement with the short name `FZ`, without Flags.
- Both iBeacon modes use UUID `FACADEFA-DECA-FEED-BEEF-FACADEDECADE`, Major `FACA`, and Minor  `DEFA`.

Changed

- iBeacon now uses the short name `FZ`; Flags AD was removed to fit the 31-byte advertising limit.
- Updated Info, splash, and exit screens for version 1.1.
- Updated application metadata to `1.1.0`.

Fixed

- Fixed ATC1441 Service Data length (`0x11` → `0x10`).
- Fixed MiBeacon Service Data length (`0x14` → `0x15`).
- Fixed MiBeacon advertisement size to fit the 31-byte BLE advertising payload.
