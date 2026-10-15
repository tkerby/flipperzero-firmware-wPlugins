# NFC QR Presenter

Share a website, contact or short note from Flipper Zero as an on-screen QR code and an NFC NDEF tag at the same time. Cards stay on your SD card. No personal cards or demo payloads are installed automatically.

![QR and NFC sharing](screenshots/ss0.png)

## Requirements

- Flipper Zero with an SD card.
- Firmware and a matching application SDK. Both official firmware and Momentum builds use the native NFC stack.
- An NFC-capable phone/reader or a camera to receive a card. Phone behavior for contacts and text depends on the receiving app.

## Use

Open **Apps > Tools > NFC QR Presenter**. The interface is in English.

| Menu | Action |
| --- | --- |
| Share | Select a card; QR and NFC start together. Back stops NFC and returns to the card list. |
| Add card > Type URL / text | Enter a card name, then its content, and save with the keyboard's Save key. |
| Add card > Import .txt / .vcf | Browse the SD card, select a file, confirm its name and save a local copy. |
| Add card > Refresh SD cards | Rescan files copied from a computer. Share, Edit and Delete also rescan. |
| Edit card | Change the selected card's content. |
| Delete card | Remove the selected card and its generated NFC file after confirmation. |

In the sharing view, **NFC active** means the emulation listener has started. It is not a confirmation that a phone has read the card. Back always stops the listener. There is no startup banner.

## Add Cards From a Computer

The simplest method is one UTF-8 file per card:

1. Create `website.txt` containing just your URL, such as `https://example.com`.
2. Open the SD file manager in qFlipper and copy it into `/ext/apps_data/nfc_presenter/`.
3. Open the app and choose **Share**, or choose **Add card > Refresh SD cards** if it is already running.

The filename becomes the card name. You do **not** need to create a binary `.ndef` file. QR and NFC are generated from the same text when you share it.

You can also copy a file anywhere on the SD card and select it with **Add card > Import .txt / .vcf**. Import creates a named copy in the app's directory; the selected source file is kept. If the name already exists, the app adds a numbered suffix instead of overwriting it. Files already in the app directory can simply be refreshed rather than imported again.

### URL or Note: `.txt`

```text
https://example.com
```

A text file may instead contain a short note. URLs starting with `http://` or `https://` become NFC URI records; other text becomes an NFC text record. Do not add JSON, quotes or a `URL=` prefix.

### Contact: `.vcf`

```text
BEGIN:VCARD
VERSION:3.0
FN:Example Contact
TEL:+10000000000
END:VCARD
```

Standard multiline vCards are accepted. With the device keyboard, `\n` and `\r` escapes are converted to line breaks; the same normalization applies to imported cards. UTF-8 with or without a BOM and Windows CRLF line endings are supported. Save files as UTF-8, not UTF-16.

### Limits

- Up to 16 cards; keep filenames under 32 characters before the extension. Extensions are case-insensitive. The list is sorted by name.
- At most 256 bytes of text after removing a UTF-8 BOM and trailing line endings.
- The virtual NTAG203 supports **141 bytes of encoded NDEF**, including record overhead. Long cards are rejected with an English message; shorten the content or use a shorter URL. Plain NFC text has a 134-byte content limit; contact records have a 128-byte content limit. URL compression depends on the prefix.
- Invalid, empty, binary or oversized imports are rejected. File-copy discovery happens before content validation; selecting an invalid SD card shows an error rather than starting NFC.

## SD Layout

```text
/ext/apps_data/nfc_presenter/
  website.txt        # URL or note; sufficient by itself
  contact.vcf        # Standard contact; sufficient by itself
  <card>.ndef        # Generated on create/edit; optional for file-copy cards
```

Existing `.txt` / `.ndef` pairs remain compatible. The text or vCard file is the source of truth; sharing always regenerates NDEF in memory. Editing a `.vcf` preserves that extension. Deleting a card from the app deletes its source file and its `.ndef` companion. Use distinct stems for `.txt` and `.vcf` cards.

## Build

Install uFBT, then select an SDK that matches the firmware you will run:

```bash
python3 -m pip install --upgrade ufbt

# Official release SDK
ufbt update --channel=release
ufbt

# Install and launch on a connected device
ufbt launch
```

For Momentum, select its SDK instead before building:

```bash
ufbt update --index-url=https://up.momentum-fw.dev/firmware/directory.json --channel=release
ufbt
ufbt launch
```

The result is `dist/nfc_qr_presenter.fap`. Copy it to `/ext/apps/Tools/` using qFlipper, or use `ufbt launch`. Close qFlipper while using uFBT so it can access the USB serial port. Selecting the latest SDK does not update the device firmware; use a matching SDK for older firmware.

## Development and Checks

```bash
ufbt format
ufbt lint
ufbt
python3 tests/run.py
```

Host tests require GCC (`CC` may select another GCC executable). They compile the production storage and NDEF code against an in-memory SD fixture and cover URI/text/contact encoding, TXT/VCF discovery, UTF-8 BOM/CRLF handling, naming collisions, vCard edits, invalid input and length boundaries. Test fixtures are excluded from the FAP and are never installed on the SD card.

Device validation should include repeated full exit/relaunch cycles, sharing, importing, editing and delete confirmation. Verify over-the-air NFC reception separately with a phone or NFC reader.

Catalog screenshots are saved directly using qFlipper's screenshot button, without resizing or recoloring.

## Source Layout

```text
application.fam              Application metadata and dependencies
src/nfc_qr_presenter.c       English menus, import workflow, QR view and lifecycle
src/nfc_qr_storage.c         Card discovery, text normalization and NDEF records
src/nfc_qr_nfc.c             Native NTAG203 NFC listener and worker
src/qrcodegen.*              Project Nayuki QR encoder
images/                     App icon
screenshots/                qFlipper captures
tests/                      Host storage and encoding regression tests
docs/catalog_description.md Catalog description
docs/changelog.md           Release history
```

## License

MIT, copyright Roger F5. The Project Nayuki `qrcodegen` source retains its upstream MIT notice.
