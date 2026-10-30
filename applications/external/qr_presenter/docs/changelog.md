# Changelog

## 1.1

- Translated every app-owned on-screen message and control to English.
- Added named cards and SD file import for UTF-8 TXT and standard VCF contacts.
- Made TXT/VCF files discoverable without a manually generated NDEF companion.
- Added sorted card lists, SD refresh and collision-safe names for new cards.
- Improved sharing layout with measured two-line names and a four-module QR quiet zone.
- Made NFC capacity errors explicit and preserved keyboard input after a save failure.
- Serialized module callbacks through dispatcher events and protected the share model.
- Restored dialog callbacks after reset so status and confirmation buttons remain functional.
- Added host regression tests for storage, import normalization and NDEF encoding.

## 1.0

- Initial public release.
- Added QR rendering and NFC NDEF emulation.
- Added local payload management from the Flipper UI.
- App starts without bundled personal or demo payloads.
