# Onkyo Remote — Specification

| | |
| --- | --- |
| Status | Draft |
| Date | 2026-09-19 |
| Target | Flipper Zero, official firmware, SDK `1.4.3` / API `87.1` |
| Artifact | `dist/onkyo_remote.fap` |

A dedicated infrared remote for an Onkyo AV receiver, replacing the generic
Infrared app's saved-remote list with a purpose-built, single-screen button
grid.

---

## 1. Scope

### 1.1 In scope

A single-screen app presenting every control the user actually needs as a
directly addressable on-screen button: power, mute, transport, volume, the flat
EQ preset, and the six source inputs. One D-pad move per neighbouring control,
one OK press per transmission.

### 1.2 Out of scope

- Learning or recording IR codes on-device (the stock Infrared app does this).
- Editing the code table at runtime, or loading `.ir` files from the SD card.
  Codes are compiled in.
- Any receiver state read-back. IR is one-way; the app never claims to know
  what the receiver is doing.
- Zone 2, tuner presets, discrete power-on / power-off, listening modes other
  than Flat, and any non-IR radio.

---

## 2. Hardware and build

Builds with `ufbt` against the pinned SDK.

The manifest carries no `fap_libs`. It used to link `libinfrared.a` statically,
on the belief that `infrared_send` was absent from the firmware's exported
symbol table; at API `87.1` it is present, the FAP loader resolves it at launch,
and dropping the line took the binary from 29,892 bytes to 8,876.

Manifest changes required by this specification:

| Key | Value | Why |
| --- | --- | --- |
| `fap_icon_assets` | `"images"` | Already set; now actually used — the button icons are compiled from `images/*.png` into `onkyo_remote_icons.h`. |
| `stack_size` | `2 * 1024` | Unchanged. All layout data is `static const`; nothing large lives on the stack. |

`ufbt lint` must pass and `ufbt` must build clean before any commit.

---

## 3. User-facing behaviour

### 3.1 Orientation and grip

The view port runs in portrait: `view_port_set_orientation(vp,
ViewPortOrientationVertical)`, giving a **64 × 128** canvas. The Flipper is
turned **clockwise** so the screen stands tall, which puts the **D-pad to the
left of the screen** and points the device's top edge — where the IR emitter
sits — to the user's right, the direction to aim at the receiver.

This handedness was measured, not assumed. A framebuffer captured over the RPC
screen stream needs a 90° clockwise rotation to read upright, so the logical top
of the UI sits along the physical screen's left edge; turning the device
clockwise is what stands it up, and that carries the D-pad to the left.

The firmware rotates D-pad key codes together with the canvas, so the app
handles `InputKeyUp` / `Down` / `Left` / `Right` in screen space and does no
remapping of its own.

> For the mirror grip — D-pad on the right, device turned counter-clockwise —
> switch the constant to `ViewPortOrientationVerticalFlip`. Canvas and key
> mapping flip together; it is a one-line change and the layout is unaffected.

### 3.2 Screen layout

Content is taller than the screen (214 px of buttons in a 128 px window), so
the view scrolls vertically. Focus never leaves the visible area; scrolling is
a consequence of moving focus, never a separate mode.

```
   scroll = 0 (top)              scroll = 86 (bottom)
 ┌────────────────┐            ┌────────────────┐
 │ ┌─────┐┌─────┐ │            │ ┌────────────┐ │
 │ │ PWR ││MUTE │ │            │ │    FLAT    │ │
 │ └─────┘└─────┘ │            │ └────────────┘ │
 │ ┌────────────┐ │            │ INPUT ───────  │
 │ │ PLAY/PAUSE │ │            │ ┌────────────┐ │
 │ └────────────┘ │            │ │    USB     │ │
 │ ┌─────┐┌─────┐ │            │ └────────────┘ │
 │ │VOL- ││VOL+ │ │            │ ┌────────────┐ │
 │ └─────┘└─────┘ │            │ │ Bluetooth  │ │
 │ ┌─────┐┌─────┐ │            │ └────────────┘ │
 │ │PREV ││NEXT │ │            │ ┌────────────┐ │
 │ └─────┘└─────┘ │            │ │    HDMI    │ │
 │ ┌────────────┐ │            │ └────────────┘ │
 │ │    FLAT    │ │            │ ┌────────────┐ │
 │ └────────────┘ │            │ │    AUX     │ │
 │ INPUT ───────  │            │ └────────────┘ │
 │ ┌────────────┐ │            │ ┌────────────┐ │
 │ │    USB     │ │            │ │    RCA     │ │
 │ └────────────┘ │            │ └────────────┘ │
 │ ┌────────────┐ │            │ ┌────────────┐ │
 │ │ Bluetooth  │ │            │ │    OPT     │ │
 └────────────────┘            └────────────────┘
```

Exact geometry, in **content space** (y = 0 is the top of the scrollable
content, not the top of the screen). Screen x runs 0–63; drawing subtracts the
scroll offset from y.

| Row | Button | x | y | w | h |
| --- | --- | --- | --- | --- | --- |
| 0 | Power | 2 | 2 | 29 | 20 |
| 0 | Mute | 33 | 2 | 29 | 20 |
| 1 | Play/Pause | 2 | 24 | 60 | 20 |
| 2 | Volume Down | 2 | 46 | 29 | 20 |
| 2 | Volume Up | 33 | 46 | 29 | 20 |
| 3 | Previous | 2 | 68 | 29 | 20 |
| 3 | Next | 33 | 68 | 29 | 20 |
| 4 | Flat | 2 | 90 | 60 | 20 |
| — | `INPUT` header | 2 | 112 | 60 | 10 |
| 5 | USB | 2 | 124 | 60 | 13 |
| 6 | Bluetooth | 2 | 139 | 60 | 13 |
| 7 | HDMI | 2 | 154 | 60 | 13 |
| 8 | AUX | 2 | 169 | 60 | 13 |
| 9 | RCA | 2 | 184 | 60 | 13 |
| 10 | OPT | 2 | 199 | 60 | 13 |

Content height is 214 px; the scroll offset is therefore clamped to `[0, 86]`.

Drawing rules:

- **Unfocused button** — `canvas_draw_rframe(..., 2)` outline, content in black.
- **Focused button** — `canvas_draw_rbox(..., 2)` filled, then
  `canvas_set_color(canvas, ColorWhite)` for the icon or label, restored to
  `ColorBlack` afterwards.
- **Sending** — while a transmission is in flight the focused button grows by
  one pixel on every side: `rbox` at `(x-1, y-1, w+2, h+2)`, radius 3. An inset
  frame was the first choice, but at `y+2` it collides with a 16 px icon in a
  20 px cell; growing outward into the 2 px gutter does not. The draw callback
  runs on the GUI thread, so it repaints while the app thread is blocked inside
  `infrared_send()`.
- **Icon buttons** — a 16 × 16 icon centred in the cell.
- **Text buttons** — `FontSecondary`, centred via `canvas_draw_str_aligned(cx,
  cy, AlignCenter, AlignCenter, label)`.
- **`INPUT` header** — `FontSecondary`, left-aligned at x = 2, followed by a
  horizontal rule running to x = 61 at the text's vertical mid-point. It is a
  label, never focusable, and scrolls with the content.
- **No scrollbar.** At 64 px wide the 2 px it would cost is better spent on the
  buttons; partially clipped rows at the screen edge already signal that there
  is more content.

### 3.3 Navigation

Focus is a `(row, col)` pair over 11 rows. Rows 0, 2 and 3 have two columns;
every other row has one.

| Input | Effect |
| --- | --- |
| `Up` / `Down` | Move one row. Does **not** wrap at the first or last row. |
| `Left` / `Right` | Move one column within the current row. No effect on single-column rows; does not wrap. |
| `OK` | Transmit the focused button's command. |
| `Back` | Exit the app. |

A **sticky column** is remembered across vertical moves: leaving Power (row 0,
col 0) downward lands on Play, and continuing down lands on Volume Down (col 0)
rather than resetting to the left. The stored column is only overwritten by an
explicit Left/Right press.

After every focus change the scroll offset is corrected so the focused cell is
fully visible with a 2 px margin:

```
if(focus.y - offset < 2)                offset = focus.y - 2;
if(focus.y + focus.h - offset > 126)    offset = focus.y + focus.h - 126;
offset = clamp(offset, 0, 86);
```

Initial focus is Power (row 0, col 0), offset 0.

### 3.4 Transmission

One OK press sends one command: `infrared_send(&message, ONKYO_TX_FRAMES)` with
`ONKYO_TX_FRAMES = 2`, approximating the frame count a short press on the
original remote produces. `.repeat` in the `InfraredMessage` stays `false`.

**Holding a button does not repeat.** Only `InputTypeShort` triggers a
transmission; `InputTypeLong` and `InputTypeRepeat` are ignored for OK. Volume
changes are made by repeated taps. (`InputTypeRepeat` remains accepted for the
four direction keys, so holding a direction still scrolls the grid.)

While `infrared_send()` blocks:

- The notification LED runs `sequence_blink_start_magenta`, stopped with
  `sequence_blink_stop` when the send returns.
- The focused button draws in its "sending" style (§3.2).
- Input events that arrive are drained from the queue and discarded, so a
  double-tap during a ~100 ms transmit cannot queue a second burst.

`infrared_send()` is called from the app thread only — never from the input or
draw callback.

### 3.5 Exit

A short `Back` press ends the run loop; the app frees everything and returns 0.
There is no confirmation prompt and no persisted state — the next launch starts
with focus on Power again.

The app also answers `FuriSignalExit` through
`furi_thread_set_signal_callback`, setting the same flag. Without that handler
the loader cannot replace a running instance: `ufbt launch` and `loader close`
both fail with "has to be closed manually", which makes every redeploy need a
physical button press. The callback is cleared during teardown, before the
context it points at is freed.

---

## 4. IR command set

Captured from the user's physical Onkyo remote with the Flipper's learn mode;
the raw capture is kept at `res/Onkyo.ir`. Every command is **NECext**
(`InfraredProtocolNECext`): a 16-bit address and a 16-bit command.

`.ir` files store both fields least-significant byte first, so `address: D2 03
00 00` is `0x03D2` and `command: 05 FA 00 00` is `0xFA05`.

| Button | `.ir` name | `.ir` address | `.ir` command | C address | C command | Inv. |
| --- | --- | --- | --- | --- | --- | --- |
| Power | Power | `D2 04` | `CB 34` | `0x04D2` | `0x34CB` | ok |
| Mute | Mute | `D2 03` | `05 FA` | `0x03D2` | `0xFA05` | ok |
| Play/Pause | Play | `D2 1A` | `5C A3` | `0x1AD2` | `0xA35C` | ok |
| Volume Up | Vol up | `D2 03` | `02 FD` | `0x03D2` | `0xFD02` | ok |
| Volume Down | Vol down | `D2 03` | `03 FC` | `0x03D2` | `0xFC03` | ok |
| Next | Next | `D2 09` | `8F 70` | `0x09D2` | `0x708F` | ok |
| Previous | Prew | `D2 09` | `90 6F` | `0x09D2` | `0x6F90` | ok |
| Flat | Flat | `D2 03` | `54 AB` | `0x03D2` | `0xAB54` | ok |
| USB | USB | `D2 1E` | `51 0F` | `0x1ED2` | `0x0F51` | **no** |
| Bluetooth | Bt | `D2 1E` | `51 0E` | `0x1ED2` | `0x0E51` | **no** |
| HDMI | HDMI | `D2 03` | `48 B7` | `0x03D2` | `0xB748` | ok |
| AUX | AUX | `D2 03` | `06 F9` | `0x03D2` | `0xF906` | ok |
| RCA | RCA | `D2 03` | `11 EE` | `0x03D2` | `0xEE11` | ok |
| OPT | OPT | `D2 03` | `57 A8` | `0x03D2` | `0xA857` | ok |

### 4.1 Address grouping

Every address ends in `D2` — the Onkyo customer code — with the high byte
selecting a device group:

| Address | Group | Commands |
| --- | --- | --- |
| `0x03D2` | Main receiver | Mute, Volume ±, Flat, HDMI, AUX, RCA, OPT |
| `0x04D2` | System / power | Power |
| `0x09D2` | Transport | Previous, Next |
| `0x1AD2` | Player | Play |
| `0x1ED2` | Network / USB | USB, Bluetooth |

### 4.2 Verification status

**Captured from an Onkyo GX-30ARC's own remote and tested against that system.**
They have not been tried on any other model, and Onkyo command bytes are
model-specific — the addresses, the device grouping and the commands themselves
may all differ on another unit. Treat the table as specific to the GX-30ARC.

Two details of the capture remain worth knowing:

1. **USB and Bluetooth fail the NEC inverse check.** In every other row the
   command's high byte is the bitwise complement of its low byte
   (`0xCB` → `0x34`, `0x05` → `0xFA`). For USB and Bluetooth the low byte is
   `0x51` in both cases, which would require a high byte of `0xAE`; the capture
   instead holds `0x0F` and `0x0E` — two values differing in a single bit.
   Either the `0x1ED2` device group genuinely uses non-inverted 16-bit
   commands, or these two frames were mis-decoded during learning. NECext does
   not enforce the complement, so the app transmits them verbatim as captured.
   On the GX-30ARC that appears to be genuine rather than a bad capture; on
   another model, re-capture both buttons before trusting them.
2. **Play/Pause is a single code.** Only `Play` was captured, from a different
   device group (`0x1AD2`) than the inputs. Whether it toggles pause, or only
   ever starts playback, is unknown. The button is labelled Play/Pause because
   that is the requested control; if a separate Pause code exists on the
   original remote, capture it and the button becomes a two-state control in a
   follow-up.

Also unconfirmed: whether `Power` is a toggle (assumed — only one code was
captured, so no discrete on/off is possible) and whether `Flat` sets the flat
preset outright or cycles through EQ presets.

---

## 5. Software design

### 5.1 Files

| Path | Contents |
| --- | --- |
| `onkyo_commands.h/.c` | `OnkyoCmdId` enum and the IR table. **The only place IR codes appear.** |
| `onkyo_layout.h/.c` | Button descriptors, geometry, navigation maths, draw callback. |
| `onkyo_remote.c` | App lifecycle, view port, input queue, run loop, transmit. |
| `images/*.png` | 10 × 10 app icon plus the 16 × 16 button icons (generated). |
| `res/*.svg` | Material Design sources for the button icons. |
| `tools/svg2icons.py` | Rasterises `res/*.svg` into `images/*.png`. |
| `tools/screenshot.py` | Captures a catalog-ready screenshot over RPC. |
| `docs/screenshots/*.png` | Apps Catalog screenshots, 512 × 256. |

### 5.2 Command table

```c
typedef enum {
    OnkyoCmdPower,
    OnkyoCmdMute,
    OnkyoCmdPlay,
    OnkyoCmdVolDown,
    OnkyoCmdVolUp,
    OnkyoCmdPrev,
    OnkyoCmdNext,
    OnkyoCmdFlat,
    OnkyoCmdInputUsb,
    OnkyoCmdInputBluetooth,
    OnkyoCmdInputHdmi,
    OnkyoCmdInputAux,
    OnkyoCmdInputRca,
    OnkyoCmdInputOpt,
    OnkyoCmdCount,
} OnkyoCmdId;
```

`onkyo_commands[]` is indexed by `OnkyoCmdId` — entry order and enum order must
match, which a `static_assert` on `COUNT_OF(onkyo_commands) == OnkyoCmdCount`
enforces at compile time. The existing `OnkyoCommand` struct (`name`,
`protocol`, `address`, `command`) is kept; the addresses stop being uniform, so
the current `ONKYO_ADDR_MAIN` / `ONKYO_CMD()` shorthand is replaced by a macro
taking both fields.

### 5.3 Layout table

```c
typedef struct {
    OnkyoCmdId cmd;
    const Icon* icon;    /* NULL for a text button */
    const char* label;   /* NULL for an icon button */
    uint8_t x, y, w, h;  /* content space, §3.2 */
    uint8_t row, col;
} OnkyoButton;
```

A `static const OnkyoButton onkyo_buttons[]` holds all 14 entries, plus a
`static const uint8_t onkyo_row_cols[11]` giving each row's column count for
the navigation maths. Nothing in the layout table repeats an IR code — it
references commands by `OnkyoCmdId` only.

### 5.4 Icon assets

`fap_icon_assets="images"` compiles every PNG in `images/` into a generated
`onkyo_remote_icons.h`; a file `power_16x16.png` becomes the symbol
`I_power_16x16`. An 8-bit PNG is rejected by the build, so all of them are
1-bit. `onkyo_10px.png` is the `fap_icon` and is harmlessly compiled into the
same header alongside the buttons.

Every PNG is generated from the Material Design sources in `res/` by
`tools/svg2icons.py`. Each 24 × 24 SVG is rendered with an 8× supersample at a
size slightly larger than its target and then centre-cropped, which scales
MDI's 20-unit live area up to fill the cell instead of keeping its padding:

| Target | Render | Crop | Threshold |
| --- | --- | --- | --- |
| Button icons | 18 px | 16 × 16 | 35% |
| `fap_icon` | 12 px | 10 × 10 | 45% |

35% is low enough to hold the power ring together on a 1-bit screen. The app
icon needs the higher threshold instead, or the speaker's two driver cutouts
fill in and it becomes a black rectangle. Re-run the script after editing
anything in `res/`, then rebuild.

| Source | Icon |
| --- | --- |
| `res/power.svg` | `power_16x16` |
| `res/volume-off.svg` | `mute_16x16` |
| `res/play-pause.svg` | `play_pause_16x16` |
| `res/volume-minus.svg` | `vol_down_16x16` |
| `res/volume-plus.svg` | `vol_up_16x16` |
| `res/skip-previous.svg` | `prev_16x16` |
| `res/skip-next.svg` | `next_16x16` |
| `res/equalizer.svg` | `flat_16x16` |
| `res/speaker.svg` | `onkyo_10px` (the `fap_icon`) |

`volume-off` is the weakest of the set at this size: MDI draws a speaker, a
diagonal strike and two sound-wave arcs, and at 16 × 16 the arcs and the strike
run together. It is legible in context but not crisp. If it needs to be better,
either the icon cells grow to 22 px so the icons can be 18 × 18, or the source
is swapped for a simpler glyph such as MDI `volume-mute`.

### 5.5 Threading

Unchanged from the scaffold, and required by the SDK's rules:

- Input callback does nothing but `furi_message_queue_put()`.
- The run loop on the app thread drains the queue and handles events.
- Shared state (`focus`, `scroll`, `sending`) lives in one struct behind a
  `FuriMutex`; the draw callback takes a snapshot under the mutex and releases
  it before drawing.
- No blocking work — above all no `infrared_send()` — inside either callback.
- Every `*_alloc` is paired with `*_free`, every `furi_record_open` with
  `furi_record_close`, and the view port is removed from the GUI before it is
  freed.

### 5.6 Changes from the current scaffold

The scaffold in `onkyo_remote.c` / `onkyo_commands.c` is a vertical list of
eight **guessed** codes on plain `InfraredProtocolNEC` with address `0x4B`.
None of it survives:

- The code table is replaced wholesale by §4 — different protocol, different
  addresses, different commands, 14 entries instead of 8.
- The `Up`/`Down` list view becomes the portrait button grid of §3.2, in a new
  `onkyo_layout.c/.h` module; `onkyo_remote.c` keeps only the app lifecycle.
- The view port gains an orientation; the app gains icon assets.
- The transmit path, notification blink, mutex and queue structure are kept.
- `README.md` describes the old list UI and the old code table; it needs
  updating alongside the implementation.

---

## 6. Acceptance criteria

1. `ufbt` builds clean and `ufbt lint` passes.
2. The app launches into a portrait 64 × 128 screen, focus on Power.
3. All 14 buttons are reachable with the D-pad; the focused button is always
   fully visible.
4. The sticky column holds: Power → Down → Down lands on Volume Down.
5. `Back` exits from anywhere, immediately.
6. OK transmits exactly one burst of 2 frames; holding OK transmits nothing
   further; the LED blinks for the duration.
7. Each button's transmission is confirmed against the receiver, or explicitly
   recorded as unconfirmed in §4.2 and in `README.md`.
8. No leaks: repeated launch/exit cycles leave `free` in `ufbt cli` steady.

---

## 7. Open items

| # | Item | Resolution path |
| --- | --- | --- |
| 1 | USB / Bluetooth commands fail the NEC inverse check (§4.2) | Re-capture both buttons; `ufbt cli` → `ir rx` shows decoded frames live. |
| 2 | Does `Play` pause as well as play? | Test against the receiver; capture a separate Pause code if one exists. |
| 3 | Does `Flat` set or cycle EQ presets? | Test against the receiver. |
| 4 | Is `Power` a toggle, with no discrete on/off? | Test; if the remote has discrete codes, capture them. |
| 5 | Is `ONKYO_TX_FRAMES = 2` enough for reliable reception? | Test at normal viewing distance and off-axis. |
| 6 | ~~Is `ViewPortOrientationVertical` the right handedness?~~ | Measured: it puts the D-pad on the **left**, device turned clockwise (§3.1). `VerticalFlip` gives the mirror grip. |
| 8 | Synthetic input does not reach the app | Neither the `input send` CLI command nor RPC `gui_send_input_event_request` moves focus, though both report success. Navigation can only be exercised by hand, and a screenshot of a scrolled view needs the device scrolled manually first. |
| 7 | ~~`fap_libs=["infrared"]` appears to be unnecessary at API `87.1`~~ | Dropped. `infrared_send` is in the exported symbol table, the FAP loader resolves it at launch, and the binary fell from 29,892 to 8,628 bytes. The flipper-app-dev skill still records the old behaviour. |
