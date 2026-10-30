# NFC QR Presenter

NFC QR Presenter turns Flipper Zero into a compact digital business card presenter. It stores local payloads on the SD card, renders the selected payload as a QR code, and emulates the same content as an NFC NDEF tag using a virtual NTAG profile.

Main features:

- QR presentation through native Canvas drawing.
- NFC NDEF emulation through the firmware NFC stack.
- English interface with Share, Add card, Edit card and Delete card.
- Named cards and a file picker for importing UTF-8 TXT or standard VCF contacts.
- Copy a TXT or VCF directly into /ext/apps_data/nfc_presenter/; no binary NDEF preparation is required.
- Sorted local card list, SD refresh and deletion confirmation.
- No bundled personal or demo cards.

Cards must fit the virtual NTAG203's 141-byte encoded NDEF capacity. The app checks this before starting NFC and displays a clear error for oversized cards.

The app is designed for quick sharing of URLs, vCards, and short text payloads during events, meetings, and demos.
