# Onkyo Remote

A [Flipper Zero](https://flipperzero.one/) application (`.fap`) that drives an
Onkyo AV receiver over infrared.

**Tested on an Onkyo GX-30ARC.** Onkyo command bytes are model-specific, so
this may not work with other Onkyo products — see [The IR codes](#the-ir-codes).

The full design is in [`docs/SPECIFICATION.md`](docs/SPECIFICATION.md).

## Using it

The screen runs in portrait, so turn the Flipper clockwise until the screen
stands tall. The D-pad ends up to the left of the screen, and the IR emitter on
the device's top edge points to your right — aim that at the receiver.

| Input | Effect |
| ----- | ------ |
| D-pad | Move between buttons |
| OK    | Send the focused button's command |
| Back  | Exit |

Buttons, top to bottom: power and mute, play/pause, volume down and up,
previous and next track, flat EQ, then the input list — USB, Bluetooth, HDMI,
AUX, RCA, OPT. The grid is taller than the screen and scrolls as focus moves.

Holding a button does not repeat the command; each press sends exactly one
burst of two frames. The LED blinks magenta while transmitting.

## Requirements

- `ufbt`, the micro Flipper Build Tool
- A Flipper Zero on official firmware (this repo currently builds against SDK
  `1.4.3`, API `87.1`)

Install `ufbt`:

```bash
sudo pacman -S --needed python-pipx
pipx install ufbt
pipx ensurepath   # restart the shell afterwards
```

## Build

```bash
ufbt                 # build -> dist/onkyo_remote.fap
ufbt launch          # build, upload over USB, and run it on the Flipper
ufbt cli             # open the Flipper serial console
ufbt lint            # clang-format check; must pass before committing
ufbt format          # rewrite sources to firmware style
ufbt update          # pull a newer firmware SDK
ufbt -c              # clean
```

`ufbt` keeps the SDK and toolchain in `~/.ufbt`, not in this repo, so the first
build downloads a few hundred MB and later ones are fast.

Serial access needs your user in the `uucp` group (Arch puts `/dev/ttyACM*`
there). Check with `id -nG`; if `uucp` is missing, `sudo usermod -aG uucp $USER`
and log back in. If the port is busy, something else already owns it — a
`ufbt cli` session, qFlipper, or a browser tab holding a Web Serial connection
to lab.flipper.net. `fuser -v /dev/ttyACM*` names the culprit. With more than
one serial device attached, point ufbt at the right one:

```bash
ufbt launch FLIP_PORT=/dev/ttyACM1
```

## The IR codes

`onkyo_commands.c` holds the table. Every command is **NECext** — a 16-bit
address and a 16-bit command:

```c
[OnkyoCmdVolUp] = ONKYO_CMD("Volume Up", 0x03D2, 0xFD02),
```

The values were captured from the original remote with the Flipper's learn
mode; the raw capture is kept at `res/Onkyo.ir`. `.ir` files store both fields
least-significant byte first, so `command: 02 FD 00 00` becomes `0xFD02` in the
table.

Every address ends in `0xD2`, the Onkyo customer code, with the high byte
selecting a device group:

| Address | Group | Commands |
| ------- | ----- | -------- |
| `0x03D2` | Main receiver | Mute, volume, flat, HDMI, AUX, RCA, OPT |
| `0x04D2` | System | Power |
| `0x09D2` | Transport | Previous, Next |
| `0x1AD2` | Player | Play |
| `0x1ED2` | Network / USB | USB, Bluetooth |

**These codes were captured from an Onkyo GX-30ARC's own remote and tested
against that system. They are not a general Onkyo remote.** Onkyo command bytes
vary between models: the addresses above, the device grouping, and the command
bytes themselves may all differ on another unit, and nothing here has been
tried on one. If your receiver ignores the app, assume the table is wrong for
your model and re-capture it.

One oddity worth knowing if you do re-capture: USB and Bluetooth are the only
two entries that fail the NEC complement check the rest satisfy (`0x51` would
want a high byte of `0xAE`, but the capture holds `0x0F` and `0x0E`). That
appears to be genuine for the `0x1ED2` network/USB device group rather than a
bad capture. Section 4.2 of the specification has the details.

To re-capture a button:

1. Open `Infrared` → `Learn New Remote` on the Flipper and press it.
2. Save, then read `/ext/infrared/*.ir` on the SD card for `protocol`,
   `address` and `command`.
3. Copy the values into `onkyo_commands.c`.

`ufbt cli` then `ir rx` prints decoded frames live, which is quicker for
checking a handful of buttons.
[Flipper-IRDB](https://github.com/Lucaslhm/Flipper-IRDB) has community-captured
Onkyo files that are a reasonable cross-check.

## Layout

| Path                 | Purpose                                              |
| -------------------- | ---------------------------------------------------- |
| `application.fam`    | App manifest — id, entry point, icon, linked libs    |
| `onkyo_remote.c`     | App lifecycle, input queue, transmit                 |
| `onkyo_layout.c/h`   | Button grid geometry, navigation, drawing            |
| `onkyo_commands.c/h` | The IR command table                                 |
| `images/`            | `fap_icon` plus the generated 16x16 button icons     |
| `res/`               | The original IR capture and the icon SVGs            |
| `tools/svg2icons.py` | Rasterises `res/*.svg` into `images/*.png`           |
| `tools/screenshot.py`| Captures a catalog-ready screenshot over RPC         |
| `docs/screenshots/`  | Screenshots for the Apps Catalog                    |
| `docs/`              | Specification, changelog, catalog description        |
| `dist/`              | Build output (ignored)                               |

Icons in `images/` are compiled by `fap_icon_assets` into a generated
`onkyo_remote_icons.h`; `power_16x16.png` becomes the symbol `I_power_16x16`.
They must be 1-bit PNGs, and the 10x10 `fap_icon` must be 1-bit too or the
build fails.

The button icons and the app icon are generated from the Material Design SVGs
in `res/` (`speaker.svg` is the app icon, the rest are buttons):

```bash
python3 tools/svg2icons.py --preview   # needs rsvg-convert and Pillow
```

Re-run it after editing anything in `res/`, then rebuild.

## Credits

The button icons and the app icon are
[Material Design Icons](https://pictogrammers.com/library/mdi/) by
Pictogrammers, used under the Apache License 2.0. The SVG sources in `res/` are
unmodified; the 1-bit PNGs in `images/` are generated from them. See
[`NOTICE`](NOTICE) for details, and [`LICENSE`](LICENSE) for this project's own
terms (GPL-3.0).
