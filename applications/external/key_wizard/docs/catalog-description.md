# Universal Key Wizard

Multiplexed intercom and access key auto-detector and native asset sorter for Flipper Zero.

Touch any key, fob, or card to your Flipper Zero, and Key Wizard instantly identifies the technology, decodes its protocol, and saves it directly into Flipper native asset folders.

## Features

- True 3-in-1 Multiplexed Scanning: High-speed round-robin polling for 125 kHz RFID, 13.56 MHz NFC, and 1-Wire iButton (Dallas, Cyfral, Metakom).
- Direct Native Folder Routing: Saves keys directly into /ext/lfrfid, /ext/nfc, and /ext/ibutton.
- Preloaded Universal Keys: Quick access to popular universal master keys (Dallas DS1990, EM4100, Cyfral Master, Mifare Blank).
- Automatic Naming: Clean collision-free file naming.
- Haptic and Audio Alerts: Distinct vibration and sound cues upon detection and saving.
- 100% Native Hardware: No external modules required.

## Navigation and Controls

- Press Left / Right in Scanning mode to switch scanning filter (Auto, RFID 125kHz, NFC 13.56MHz, iButton 1-Wire).
- Press Up in Scanning mode to open the Universal Quick Keys menu.
- Press Down in Scanning mode to open the About & Support screen.
- Press Back to exit.
- When a key is detected, press OK to save directly to native folder, or Left/Back to return to scanning.
