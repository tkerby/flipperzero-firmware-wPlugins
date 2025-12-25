# 📟 ListEM – Flipper Zero RFID/NFC/iButton Fuzzer&Bruteforce List Generator

**ListEM** is a Flipper Zero application that generates large, randomized UID dictionaries for **RFID**, **NFC**, and **iButton** protocols directly on your Flipper.

I decided to buld it as flipper app for flexibility, and ease of use on the go! 
ListEM brings advanced list generating (previously done via my python script) straight onto your Flipper Zero.

---

## 📸 Screenshots

<p align="center">
  <img src="screenshots/main.jpg" width="220">
  <img src="screenshots/prefix.jpg" width="220">
  <img src="screenshots/progress.jpg" width="220">
</p>

---

## ✨ Features

- ✅ Supports **28 protocols**  
  - **RFID** : EM4100, HID Prox, Indala, IoProx, PAC/Stanley, Paradox, Viking, Pyramid, Keri, Nexwatch, H10301, Jablotron, Electra, IDTeck, Gallagher.
  - **NFC** : MIFARE Classic 1K, MIFARE Classic 4K, MIFARE Ultralight, DESFire EV1, iCLASS, FeliCa.
  - **iButton** : Dallas DS1990, Cyfral, Metacom, Maxim iButton, Keypad/Access Control, Temperature iButton, Custom iButton.

- 🧬 **Manufacturer Prefix Support**
  - Generate lists using selected manufacturer prefixes or just randomized IDs with no prefix
  - If multiple prefixes are available for a protocol,the user can, use all available prefixes, or select one or more
  - If **no prefix is selected**, only radomized IDs will be generated
  - If **one or more prefixes are selected**, only those are used
  - Prefixes chosen will be added to the **output filename**

---

## 📁 Output Structure

Generated files are saved automatically to:
corresponding protocol folders (mifare_fuzzer, lfrfid_fuzzer, ibutton_fuzzer) under ListEM folder

- 🕹️ **Controls**

| ⬆️ / ⬇️ = Change protocol |
| ⬅️ / ➡️ = Decrease / Increase ID count |
| **OK**   = Generate list |
| **Hold OK** = Prefix selection (if supported) |
| **Back** = Exit app |

---

- ⚠️ **Cautious!**
  - *USE ON AUTHORIZED EQUPMENTS ONLY* 
