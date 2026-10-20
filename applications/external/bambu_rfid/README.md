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


## Magic-tag write and lock workflow

Writing is available from a saved tag's **Tag details** screen. Select **Write**, then hold **OK** on the warning screen. The writer is intentionally conservative because several magic-tag families can be irreversibly personalized.

The state machine follows these rules:

1. Load a complete 64-block Bambu dump and reject an invalid block-0 BCC.
2. Require a 4-byte MIFARE Classic 1K target and detect a supported magic path. **No random UID is ever written to an unknown blank target.** Gen3 is detected with its unauthenticated block-0 read and a same-page capability check. Otherwise the first attempted DirectWrite/Gen2 manufacturer-block write is the **final selected Bambu block 0 itself**, before any restrictive Bambu sector-0 trailer is installed. This protects write-once DirectWrite/OTP cards from being permanently personalized to a probe UID.
3. Verify all three pre-lock invariants:
   - the **live anticollision UID/SAK/ATQA** matches the Bambu identity,
   - all **64 blocks** exactly match the selected dump, and
   - all **16 bytes of block 0** exactly match the selected dump.
4. Create a different valid 4-byte probe UID and attempt a UID rewrite. Gen1/Gen2 probes preserve bytes 5-15 of block 0 and update BCC; Gen2 temporarily places sector 0 in transport access while retaining both Bambu keys, then restores the exact Bambu trailer. Gen3 uses the dedicated `90 FB CC CC 07 <UID>` command so its block 0 is not touched by the probe.
5. If the UID changes, restore the Bambu UID/block 0 and repeat all three verification checks **before** any lock command.
6. Try only the documented lock method for the detected family. Unknown-family irreversible opcodes are never sprayed at a tag.
7. After the lock attempt, verify the Bambu data again and make three UID rewrite attempts. A lock is only reported successful when all attempts are blocked while the live Bambu UID/SAK/ATQA, all 64 blocks, and the exact 16-byte block 0 remain correct.
8. If no supported lock can be verified, the app attempts to leave the canonical empty magic MIFARE Classic 1K image and warns that the mutable tag cannot be locked and the AMS will "brick" it if used as a Bambu clone.

Back/exit is ignored while the destructive writer sequence is active so the app cannot be interrupted while a post-write temporary probe UID is on the tag. The initial blank-target preflight never uses a temporary UID.

### Advanced wipe workflow

The **Advanced → Wipe Bambu magic tag** operation is deliberately destructive. It first reads the live 4-byte UID and derives both Bambu key sets from that UID. It then requires a complete 64-block read with those keys before touching the card. For supported magic families, the canonical blank manufacturer block is established before the remaining sectors are erased where the family permits that ordering. Every data block becomes zero and every sector trailer becomes `FF FF FF FF FF FF FF 07 80 69 FF FF FF FF FF FF`. The final manufacturer block is `00 01 02 03 00 08 04 00 00 00 00 00 00 00 00 00`. Success is reported only after all 64 blocks are re-read using default `FFFFFFFFFFFF` keys. MF4 remains intentionally refused because its hidden configuration aliases normal trailer bytes.

### Supported magic behavior

| Family | Programming path | Lock behavior used by this app |
|---|---|---|
| Gen1A / UID | `40(7)`, `43`, backdoor block writes | If personalization itself disables UID rewrite, that is verified. Generic mutable Gen1A has no guessed permanent lock. |
| Gen1B | `40(7)` followed directly by backdoor reads/writes | No undocumented permanent lock is guessed; mutability is verified and unlocked tags fall back to a blank image. |
| FUID / alternate Gen1 | `20(7)`, `23`, backdoor block writes | Auto-personalization is accepted only if UID rewriting is actually blocked afterward. Otherwise no undocumented seal is guessed. |
| MF4 | Read-only comparison of backdoor vs authenticated sector-0 trailer view | **Rejected before writing.** MF4 aliases configuration into trailer bytes; exact Bambu trailers can change live SAK/ATQA before its magic path is permanently disabled. |
| UFUID | Gen1A backdoor | Uses the documented UFUID seal configuration only after live UID + full dump + block 0 are exact. |
| Gen2 / CUID | Direct block-0 write; Bambu ACLs are temporarily relaxed with original keys for probe/restore | No documented permanent lock is assumed; UID rewrite is tested rather than inferred. |
| Gen3 / APDU | Normal sectors plus Gen3 block-0 APDU | Uses the documented permanent `90 FD 11 11 00` freeze only after the three pre-lock checks. |
| Gen4 / Ultimate | `CF <pwd> CD` backdoor writes | Fuses a MIFARE Classic configuration with direct block-0 write disabled and a random non-persisted backdoor password, then verifies that known/default UID rewrite paths fail. |

The Gen4 `F1` fuse command by itself is **not** treated as a lock: public documentation notes that backdoor reads/writes remain available after configuration fusing. The app therefore combines the fuse with direct-write disable and a random password.

See `MAGIC_TAGS.md` for the detailed compatibility/safety matrix.

The magic-card research also covers DirectWrite OTP, OTP 2.0, MF4, USCUID/GDM/GDCUID/ZUID, and MIFARE Classic Super variants. The resulting policy is intentionally narrow:

- DirectWrite OTP / OTP 2.0 / FUID-style write-once behavior is trusted only after the post-write UID-change test proves it.
- MF4 is not written because its configuration aliases sector-trailer bytes, making an exact Bambu dump and a guaranteed Bambu on-air identity incompatible without tag-specific provisioning.
- UFUID uses its documented seal configuration.
- Gen3 uses its documented irreversible freeze command.
- Plain Gen1A/Gen1B, Gen2/CUID, ZUID, GDM/GDCUID, and Super variants are not given guessed "lock" commands when the public documentation only exposes reversible/backdoor mechanisms.
- Gen4/Ultimate uses permanent configuration fusing plus direct-write disable and a random non-persisted password, followed by actual UID rewrite attempts; the app does not claim `F1` alone removes the backdoor.

## Saved tag bundle

Each successful scan is written to:

```text
/ext/apps_data/bambu_rfid/tags/<UID>/
├── hf-mf-<UID>-dump.bin
├── hf-mf-<UID>-dump.json
├── hf-mf-<UID>-key.bin
├── hf-mf-<UID>.nfc
└── README.md
```

The four RFID files follow the same conventions used by `queengooborg/Bambu-Lab-RFID-Library/convert.py`:

- `hf-mf-<UID>-dump.bin` — exactly 1024 bytes: blocks 0 through 63 concatenated in order.
- `hf-mf-<UID>-dump.json` — Proxmark-style `mfc v2` JSON containing card data, all blocks, sector keys, access bytes, and decoded access-condition text.
- `hf-mf-<UID>-key.bin` — exactly 192 bytes: all 16 six-byte Key A values first, followed by all 16 six-byte Key B values, extracted from the sector trailers.
- `hf-mf-<UID>.nfc` — Flipper NFC device Version 4 / MIFARE Classic 1K text format with all 64 blocks.
- `README.md` — human-readable parsed information for that spool/tag.

The `.nfc` export reverses the two ATQA bytes as required by the Flipper file convention, matching the library converter.

## Key derivation

The app implements the public derivation documented by Bambu Research Group:

- IKM: the 4-byte tag UID
- HKDF-SHA256 salt: `9A759CF2C4F7CAFF222CB9769B41BC96`
- Key A info/context: `RFID-A\0`
- Key B info/context: `RFID-B\0`
- output per context: 96 bytes, split into sixteen 6-byte MIFARE Classic keys

The implementation uses mbedTLS HMAC-SHA256, which is already available to Flipper external apps through `fap_libs=["mbedtls"]`.

## Data layout

The parser follows the documented Bambu blocks:

- Block 1: variant ID / material ID
- Block 2: filament type
- Block 4: detailed filament type
- Block 5: RGBA color, spool weight, diameter
- Block 6: drying/bed/hotend settings
- Block 8: X-Cam data and nozzle diameter
- Block 9: tray UID
- Block 10: spool width
- Blocks 12-13: production timestamps
- Block 14: filament length
- Block 16: multi-color metadata
- Block 17: currently-unknown two-byte field
- Sectors 10-15: RSA signature bytes are retained in the saved dump even though the app does not attempt to validate the signature.

## Building

### GitHub Actions

Push the project to GitHub. The included workflow builds against both the Flipper release and dev SDK channels and uploads `.fap` artifacts. Tags matching `vX.Y.Z` publish the release-channel FAP as a GitHub release.

### Local uFBT

With uFBT installed:

```sh
ufbt
```

The resulting FAP can be copied to the NFC apps directory on the Flipper SD card.

## Installing / using

1. Install the built `bambu_rfid.fap` under `apps/NFC` on the Flipper SD card.
2. Launch **Bambu RFID**.
3. Choose **Scan tag** and hold the spool RFID tag against the Flipper.
4. Keep the tag steady while all 16 sectors are read.
5. The five-file bundle is saved automatically under `/ext/apps_data/bambu_rfid/tags/<UID>/`.
6. Choose **Browse scans** to inspect previously saved tags. The RGB LED follows the primary filament color of the currently highlighted tag, and saved dump keys are synchronized into Flipper's MIFARE Classic user dictionary.
7. Open a saved tag, choose **Write**, and hold **OK** to program a blank magic MIFARE Classic 1K tag. Keep the tag stationary until the app reports a verified result.
8. To erase a previously programmed Bambu magic tag, choose **Advanced → Wipe Bambu magic tag**, hold **OK**, and keep the tag stationary until all 64 canonical blank blocks verify with default keys.

To inspect existing dumps, either copy a library-style UID folder under `/ext/apps_data/bambu_rfid/tags/`, or place a legacy Flipper `.nfc` / raw 1024-byte `.bin` file directly in that directory.

## References

This implementation was written from the public research and examples in:

- `Bambu-Research-Group/RFID-Tag-Guide` — KDF and tag memory documentation.
- `queengooborg/Bambu-Lab-RFID-Library` — real-world dumps and canonical format conversion logic.
- `DanTheMan827/AmiiboZero` — ViewDispatcher/custom-view UI structure and GitHub uFBT workflow pattern.
- `flipperdevices/flipperzero-firmware` — public NFC/MIFARE Classic and storage APIs.
- `RfidResearchGroup/proxmark3` — Bambu `RFID-A\0` / `RFID-B\0` UID key-generation implementation and `doc/magic_cards_notes.md` documentation for Gen1A, FUID/UFUID, Gen2, Gen3, USCUID/GDM, and Gen4/Ultimate magic cards.
- `flipperdevices/flipperzero-good-faps/nfc_magic` — Flipper-native Gen1A/Gen2/Gen4 protocol implementations used as an API/behavior reference for raw NFC transactions.

## Notes

- Bambu tags contain an RSA signature. Reading a tag does not give the private signing key and this app does not attempt to forge signatures.
- The Flipper screen is monochrome, so colors are displayed as RGBA hex codes on-screen; the RGB status LED mirrors the primary tag color while browsing and in details. A black (`000000`) filament therefore intentionally leaves the RGB LED off.
- The user dictionary sync uses Flipper's native MIFARE Classic dictionary service/path and adds keys only when they are not already present.
- The Bambu KDF derives both sector key types using the `RFID-A\0` and `RFID-B\0` contexts. The app supplies both sets during the read and still verifies that all 64 blocks were actually read before exporting.
- Classic sector key bytes are represented in the Flipper read structure from the derived keys; the exported trailer blocks therefore contain the same A/B key layout used by the reference library dumps.
- The reader requires all 64 blocks before saving. If the tag moves during the operation, retry while holding it steady.
- Writing/locking is inherently destructive. The app requires a long-OK confirmation and refuses to send an irreversible command unless the live UID/SAK/ATQA, complete dump, and exact 16-byte block 0 all match the selected Bambu dump immediately beforehand.
- Gen3 is a special safety case because its UID can differ from block 0; this is why the live anticollision UID is checked independently.
- A successful lock result means the app's post-lock UID rewrite test failed while the original Bambu UID/data remained readable and exact. Writer verification authenticates with the exact Key A/Key B values stored in the selected dump trailers, not regenerated replacements.

### Browser LED and MIFARE dictionary sync

- Browse and tag-details views drive the Flipper RGB LED from the tag's primary filament color. LED updates use the non-blocking notification API so Up/Down input never waits on the notification service.
- Bambu MIFARE keys are UID-derived. Saved-tag enumeration therefore collects only unique 4-byte UIDs in RAM; it does not reopen every dump to extract keys.
- After enumeration completes, one background dictionary job derives all 16 Key A and 16 Key B values per UID, opens `/ext/nfc/assets/mf_classic_dict_user.nfc` once, reads existing keys once, appends only missing keys, and closes it once.
- Successful live scans queue their UID into the same serialized background dictionary path after the NFC scan is complete.
