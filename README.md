# Anki-Remote

This app lets you assign any keyboard key to each button on your Flipper Zero and use it as a BLE (Bluetooth Low Energy) remote. Key mappings are saved to the SD card inside the apps_data folder.

---

## Purpose

The purpose I originally had in mind for this app was to be used as a remote for [Anki](https://apps.ankiweb.net/), a popular spaced repetition flashcard app for med school students. Having some kind of remote really makes long study sessions more comfortable and lets you review flashcards in any position, not just sitting upright.

However, this app doesn’t have to be used only with Anki; you can use it for literally any other purpose or app where a Bluetooth keyboard remote might be useful.

The current HID keyboard in the BT remote app is limited for this use case because it takes a long time to get to a different button, and you have to keep looking at the Flipper’s screen at all times.

---

## How to Use

To use the app, simply open it, select **Settings**, and change the key mappings to whatever you want. These settings will be saved to the SD card and will remain the same the next time you open the app.

Then, go back to the main menu, press **Start**, and connect to Bluetooth. Once you’re successfully connected, you’ll see the controller screen.

---

## Default Keymaps

- **Up:** Up arrow  
- **Down:** Down arrow  
- **Left:** 1  
- **Right:** 3  
- **OK:** Space  
- **Back:** u  

---

## Credits & References

The following projects were used or referenced to help me make my app:

- [Flipper Zero's Official HID](https://github.com/flipperdevices/flipperzero-firmware/tree/dev/applications/system/hid_app)  
  keyboard logic, core functionality, most images, and the remote screen layout.

- [BT Trigger](https://github.com/xMasterX/all-the-plugins/tree/dev/apps_source_code/bluetooth-trigger)  
  bluetooth cross-compatibility, the “awaiting bluetooth” screen design, and general example code.

- [Instantiator’s Flipper Zero App Tutorial](https://instantiator.dev/post/flipper-zero-app-tutorial-01/)  
  installing ufbt and setting up the development environment in VSCode.
