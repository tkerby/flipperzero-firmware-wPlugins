# Changelog

## v1.3

- Save iButton and 125 kHz RFID keys using native firmware serializers (ibutton_protocols_save and lfrfid_dict_file_save) for 100% compatibility with stock Flipper apps
- Save NFC keys using native NfcDevice ISO14443-3A serialization
- Move application category to Tools in application manifest
- Remove simulated detection debug feature from shipped application
- Remove donation address from catalog store description
- Replace non-ASCII bullet characters with plain ASCII dashes in About UI
- Allow selecting preloaded universal master keys from Quick Keys menu

## v1.2

- Added scrollable About screen with native scrollbar
- Updated application icon graphics
- Optimized multiplexed scanning loop timing
- Added support for Dallas DS1990, Cyfral, and Metakom 1-Wire keys

## v1.0

- Initial release
- Auto-detect and save 125 kHz RFID, 13.56 MHz NFC, and iButton keys
- Direct native asset folder routing
- Built-in universal master keys library
