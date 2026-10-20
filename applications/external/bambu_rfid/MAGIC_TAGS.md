# Magic MIFARE Classic write/lock notes

This file records the compatibility and safety policy used by the Bambu RFID writer. It is intentionally conservative: an irreversible operation is never sent unless the live ISO14443-A identity and the complete selected Bambu image have already been restored and verified.

## Non-negotiable pre-lock invariant

Immediately before any irreversible seal/freeze/fuse operation, the app requires all of the following at the same time:

1. Live 4-byte anticollision UID equals the selected Bambu UID.
2. Live SAK and ATQA equal the selected Bambu block-0 values.
3. All 64 MIFARE Classic blocks exactly equal the selected dump.
4. All 16 bytes of block 0 exactly equal the selected dump.
5. Authentication/verification uses the exact Key A and Key B bytes stored in the selected dump's sector trailers.

Gen3 requires the live UID check separately because its UID-only command can change the anticollision UID without changing block 0.

A temporary test UID is used only *after* the complete Bambu image is installed. The all-zero UID is never used as a probe.

## Blank-target rule

Never write a random/test UID to an unknown blank target. DirectWrite OTP tags can permanently consume their one block-0 write. For an unknown non-Gen1/non-Gen4 target:

- Gen3 is identified read-only by its unauthenticated block-0 read behavior.
- Otherwise, the first standard DirectWrite attempt to block 0 contains the **final selected Bambu block 0**.
- If that write is accepted, the app verifies the Bambu UID and exact block 0 before continuing.

This is specifically designed to prevent a write-once card from being sealed to a probe UID or probe manufacturer page.

## Family policy

| Family | Relevant public behavior | App policy |
|---|---|---|
| Gen1A / UID | `40(7)`, `43`, raw reads/writes | Program source, test UID mutability. No guessed permanent command. |
| Gen1B | `40(7)` then direct raw reads/writes | Program source, test UID mutability. No guessed permanent command. |
| DirectWrite OTP | Standard block-0 write works once | First possible block-0 write is always the final Bambu block 0. Post-write UID test determines whether it sealed. |
| OTP 2.0 | First block-0 edit disables Gen1 backdoor | Program block 0 last through the Gen1 path and verify UID cannot be changed afterward. |
| MF4 | Gen1-like, but hidden configuration is mirrored into sector-trailer bytes; UID personalization disables magic commands | **Rejected before writing** when the overlay is detected. Exact Bambu trailers would alter hidden personality/SAK/ATQA, so blindly personalizing would be unsafe. |
| FUID / alternate Gen1 | Write-once/sealable variants using alternate wakeup | Program source, then trust only a verified blocked UID rewrite. |
| UFUID | Sealable USCUID variant; documented `E100` seal configuration | Restore/verify source, send the documented seal configuration, then perform three UID-change attempts. |
| Gen2 / CUID | Standard authenticated DirectWrite to block 0 | No documented permanent lock assumed. Test UID mutability; if still mutable, restore source and then blank rather than leave a mutable Bambu clone. |
| Gen3 / APDU | `90 FB` changes UID independently of block 0; `90 FD 11 11 00` freezes UID changes | Use UID-only probes. Restore/verify source, send permanent freeze, then perform three more UID-change attempts. |
| ZUID | USCUID variant whose Gen1 mode is documented as not disableable | No permanent lock claimed. |
| GDM / GDCUID | USCUID configuration / magic-auth / CUID mechanisms | No permanent lock claimed from reversible configuration mechanisms. |
| Gen4 / Ultimate | Password-protected backdoor; direct-write mode; `F1` permanently fuses configuration, but backdoor R/W still exists with the password | Fuse MFC/4-byte/direct-write-disabled configuration with a random password that is never persisted. Verify both default-backdoor and ordinary DirectWrite UID changes are blocked. This is effective sealing against known/default access, not a claim that the silicon has no backdoor. |
| MIFARE Classic Super variants | Variant-specific backdoors; limited Gen4 commands on some models | No undocumented irreversible operation is guessed. |

## Lock verification

A single failed command or RF timeout does not count as a lock. The app requires three separate blocked UID-change attempts, and the exact Bambu identity/dump must still verify between attempts.

If a post-lock UID change succeeds, the app restores and re-verifies the selected Bambu image before taking any further action. If no verified permanent lock is available, it attempts to leave the fully verified canonical empty magic MIFARE Classic 1K image with UID `00 01 02 03` and default keys; the UI then warns that the mutable tag should not be placed in the AMS as a Bambu clone.

## Research sources

Primary references used for the implementation and policy:

- `RfidResearchGroup/proxmark3/doc/magic_cards_notes.md`
- `RfidResearchGroup/proxmark3/doc/commands.md`
- `RfidResearchGroup/proxmark3/armsrc/mifarecmd.c`
- `flipperdevices/flipperzero-good-faps/nfc_magic/magic/protocols/gen1a/`
- `flipperdevices/flipperzero-good-faps/nfc_magic/magic/protocols/gen2/`
- `flipperdevices/flipperzero-good-faps/nfc_magic/magic/protocols/gen4/`
- `flipperdevices/flipperzero-firmware/lib/nfc/`


## Advanced wipe image

The Advanced wipe operation uses the canonical empty magic MIFARE Classic 1K image used by the app:

- block 0: `00 01 02 03 00 08 04 00 00 00 00 00 00 00 00 00`
- all ordinary data blocks: `00` x 16
- every sector trailer: `FF FF FF FF FF FF FF 07 80 69 FF FF FF FF FF FF`

The live UID is read first and the Bambu A/B keys are derived from it. A complete Bambu-key read is required before the wipe proceeds. The operation reports success only after all 64 blank blocks can be read back using the default `FFFFFFFFFFFF` keys.
