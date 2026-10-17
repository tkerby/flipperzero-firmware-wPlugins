v1.1:

- Added saved NFC file selection to UID Changer for MIFARE Classic 1K/4K and ISO 15693 cards.
- Import UID, SAK, ATQA, and available Classic block 0 data, with alerts for unsupported or incomplete files.
- Improved Type 2 chip identification in card details.
- Fixed ISO 15693 Gen1 UID writes with the correct unlock sequence, hidden block addresses, and handling of silent commands.
- Built and validated against the Flipper Zero 1.5.1 SDK (API 88.2).

v1.0:

- Initial Flipper Zero release of MTools.
- Added Magic Check for supported MIFARE Classic and ISO 15693 magic cards.
- Added UID Changer with manual entry, card reading, and write verification.
- Added card details for ISO 14443-A, ISO 15693, and Ultralight/NTAG cards.
