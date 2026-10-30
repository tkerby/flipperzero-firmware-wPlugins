# BLE Beacon Simulator for Flipper Zero

**BLE Beacon Simulator** turns a Flipper Zero into a configurable, unencrypted Bluetooth Low Energy advertising beacon.

The app is designed as a companion for the **BTHome Logger** Android application, but it can also be used independently for testing BLE scanners, smart-home integrations, widgets, dashboards, and mobile applications.

## Supported beacon types

The application supports the following beacon formats:

| Beacon type | Advertised name | Real Flipper data | Simulated data |
|---|---|---|---|
| BTHome V2 | `FZ BTH2` | Battery, voltage, current, temperatures | None |
| BTHome V1 | `FZ BTH1` | Battery, voltage, current, temperatures | None |
| iBeacon | `FZ` | None | UUID, Major, Minor, RSSI |
| Eddystone TLM | `FZ EDDY` | Battery voltage, temperature | Uptime, advertising count |
| ATC1441 | `FZ ATC` | Battery, voltage, temperature | Humidity, MAC, counter |
| MiBeacon | `FZMI` | Battery, temperature | Humidity, MAC, product ID |
| OEPL | `FZ OEPL` | Battery voltage, temperature | Hardware type, firmware version, counter |

## New in version 1.1

### OEPL tag support

A new **OEPL Tag** mode was added.

When selected, Flipper Zero advertises itself as an **ATC BLE OEPL** compatible tag using manufacturer ID `0x1337` and OEPL advertising format version 2.

An OpenEPaperLink Access Point with BLE scanning enabled should detect the device and register it as an ATC BLE OEPL tag.

Expected AP log output:

```text
ATC BLE OEPL Detected
Version 2 ATC_BLE_OEPL Received
```

The OEPL advertisement contains:

- OEPL manufacturer ID `0x1337`
- OEPL protocol version `2`
- Hardware type
- Firmware version
- Capabilities
- Battery voltage
- Temperature
- Advertising counter

By default, the hardware type is set to `0x0000`. This is enough for the OEPL Access Point to register Flipper as an ATC BLE OEPL tag. To support actual image uploads, replace the hardware type with the value corresponding to a real display model.

### Short advertised names

All beacon types now use short, human-readable advertised names:

```text
FZ BTH2
FZ BTH1
FZ
FZ EDDY
FZ ATC
FZMI
FZ OEPL
```

The short names allow the Complete Local Name field to fit into the 31-byte legacy BLE advertising payload.

### iBeacon changes

The iBeacon now uses a unique, human-readable UUID:

```text
FACADEFA-DECA-FEED-BEEF-FACADEDECADE
```

It also uses:

```text
Major: FACA
Minor: DEFA
```

This makes the Flipper iBeacon easy to distinguish from other iBeacons in applications such as nRF Connect.

The iBeacon advertisement does not include the standard Flags AD structure. This was necessary to fit the short device name `FZ` into the 31-byte advertising payload. The iBeacon manufacturer data remains valid, but some BLE scanners may display the device with fewer classification details.

### ATC1441 fix

The Service Data length for the ATC1441 advertisement was corrected from `0x11` to `0x10`.

The previous value was one byte too long, which could cause BLE scanners to reject or incorrectly parse the advertisement.

### MiBeacon fix

The Service Data length for the MiBeacon advertisement was corrected from `0x14` to `0x15`.

The previous value was one byte too short, which could cause BLE scanners to reject or incorrectly parse the advertisement.

With the shortened name `FZMI`, the complete MiBeacon advertisement now fits exactly into the 31-byte legacy advertising payload.

## Real and simulated data

The application uses real Flipper Zero telemetry where possible:

- Battery charge level
- Battery voltage
- Battery current
- Battery temperature
- Charger temperature

Some beacon formats require values that Flipper Zero cannot measure directly. In that case, the application simulates them:

- Humidity for ATC1441 and MiBeacon
- iBeacon UUID, Major, Minor, and RSSI
- OEPL hardware type, firmware version, and capabilities
- Eddystone TLM uptime and advertising counter

## Controls

| Control | Action |
|---|---|
| `Left` / `Right` | Change beacon type |
| `Up`, short press | Select **Info** |
| `Down`, short press | Select **Help** |
| `Up`, hold | Open **Info** |
| `Down`, hold | Open **Help** |
| `OK` | Open the selected screen or start / stop the beacon |
| `Back` | Open the exit menu |

## Exit menu

When pressing `Back` on the main screen, the application shows an exit confirmation:

- `Left`: return to the application
- `Right`: exit the application

## Installation

Build the application with [uFBT](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
ufbt
```

Then copy the resulting `.fap` file to the Flipper Zero applications directory:

```text
apps/Bluetooth/ble_beacon.fap
```

## Notes

- The application advertises unencrypted BLE beacons only.
- It does not connect to other devices and does not provide a GATT server.
- The OEPL mode registers the device as an ATC BLE OEPL tag, but image upload requires a valid display hardware type.
- The iBeacon mode is intended for testing and identification, not for cloning another person’s AirTag or Find My accessory.

## Author

**Siarhei Besarab** aka **steanlab**

- Telegram: [t.me/lab66](https://t.me/lab66)
- GitHub: [github.com/steanlab](https://github.com/steanlab)
- LinkedIn: [@steanlab](https://www.linkedin.com/in/steanlab)
- Mastodon: [@lab66](https://mastodon.social/@lab66)

## Support

If you find this project useful, you can support the developer:

- Patreon: [@steanlab](https://www.patreon.com/steanlab)
- PayPal: [@steanlab](https://www.paypal.com/paypalme/steanlab)
- Revolut: [@steanlab](https://revolut.com)

## Special thanks

Thanks to the **Flipper Zero Team** and **Pavel Zhouner** for providing lab equipment.
