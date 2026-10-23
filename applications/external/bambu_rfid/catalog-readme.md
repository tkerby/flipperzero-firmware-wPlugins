# Bambu RFID for Flipper Zero

A Flipper Zero external app for reading, exporting, browsing, inspecting, writing, wiping, verifying, and locking Bambu Lab filament RFID tags on compatible magic MIFARE Classic 1K media.

## Features

- Reads the 4-byte tag UID using ISO14443-3A anticollision.
- Derives all 16 MIFARE Classic sector A-keys and all 16 sector B-keys directly from the UID.
- Reads all 64 MIFARE Classic 1K blocks without a pre-populated user dictionary, using the derived sector A/B keys.
- Saves every successful scan as a Bambu-Lab-RFID-Library-compatible five-file bundle in a UID folder.
- Browses saved UID folders as one tag per folder.
- Retains backward compatibility with legacy flat Flipper `.nfc` files and 1024-byte raw `.bin` dumps in the tags directory.
- Displays decoded Bambu fields: material and variant IDs, filament type, primary/secondary color, spool weight and width, filament diameter and length, hotend/bed/drying temperatures, nozzle diameter, production date, tray UID, X-Cam bytes, and known/unknown auxiliary fields.
- Writes a selected saved dump, including every original sector Key A/access/Key B trailer, to a blank UID-changeable MIFARE Classic 1K tag.
- Verifies the live anticollision UID/SAK/ATQA, all 64 blocks, and all 16 bytes of block 0 before any irreversible lock/seal/fuse command is allowed.
- Tests UID mutability with a different valid UID/BCC, restores the Bambu UID/data before any lock attempt, and requires three clean blocked rewrite attempts before declaring a tag locked.
- Uses documented family-specific lock behavior for UFUID, Gen3/APDU, DirectWrite-OTP/OTP2/FUID-style auto-personalization, and Gen4/Ultimate cards; MF4 is detected and rejected before writing because its hidden configuration aliases normal trailer bytes.
- Adds **Advanced → Wipe Bambu magic tag**: reads the live UID, derives its Bambu A/B keys, identifies the magic access path, and resets all 64 blocks to the canonical empty magic MIFARE Classic 1K image (`00 01 02 03` UID, zero data blocks, and default `FFFFFFFFFFFF / FF078069 / FFFFFFFFFFFF` trailers).
- Write and wipe status screens use wrapped, Up/Down-scrollable text areas so long safety/result messages remain readable.
- While browsing saved tags, the Flipper RGB status LED follows the highlighted tag's primary filament color; the same color remains active in the tag details screen and is cleared when leaving those views.
- Every successful live Bambu scan adds all derived MIFARE Classic Key A and Key B values to Flipper's native user dictionary at `/ext/nfc/assets/mf_classic_dict_user.nfc`. Opening **Browse scans** also synchronizes keys from all saved dumps into that dictionary without adding duplicates.
