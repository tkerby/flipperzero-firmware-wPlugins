# AGENTS.md

Guidance for AI coding agents (and anyone else) working in this repository.
Read this before changing code, screenshots, or the catalog submission.

Upstream rules that override anything here:
the Flipper Apps Catalog `documentation/Contributing.md`, `documentation/Manifest.md`
and `AGENTS.md` in <https://github.com/flipperdevices/flipper-application-catalog>.

## What this app is

Bleepsentry is a passive sub-GHz RF sentry for the Flipper Zero. It calibrates a
noise floor per frequency, then alerts and logs when RSSI rises above
baseline + 12 dB.

**The app is receive only.** It has no transmit path and must never gain one
without region gating: any future transmit must be refused unless both
`furi_hal_region_is_frequency_allowed(freq)` and
`furi_hal_subghz_is_frequency_valid(freq)` return true.

## Repository map

| Path | Purpose |
|---|---|
| `bleepsentry.c` | The app: message queue loop, timer, input handling, radio control, draw callback |
| `bleepsentry_logic.c` / `.h` | Pure logic with no Flipper SDK dependency: threshold math, alert decision, index wrap, CSV formatting |
| `tests/test_logic.c` | Host-side assert suite for the pure logic, run in CI |
| `application.fam` | FAP manifest: `appid`, `name`, category, version, icon, sources |
| `screenshots/` | Catalog screenshots plus `README.md` describing how they were captured |
| `docs/description.md` | Catalog description text, written in the restricted markdown subset |
| `changelog.md` | Version history, loaded by the catalog manifest |
| `.github/workflows/build.yml` | CI: host logic tests plus a `ufbt` build on dev and release SDK channels |

## Build, test, device loop

```
ufbt            # build, output in dist/ and ~/.ufbt/build/
ufbt lint       # SDK format/tidy gate, must stay clean
ufbt launch     # build, install to /ext/apps/Sub-GHz/bleepsentry.fap, start
```

Host tests, no device needed:

```
cd tests
gcc -std=c11 -Wall -Wextra -I.. test_logic.c ../bleepsentry_logic.c -lm -o test_logic
./test_logic
```

Pitfalls that cost time:

- **`ufbt launch` can end with `Application "Bleepsentry" has to be closed manually`.**
  The install already succeeded; the loader simply cannot kill a plain `ViewPort`
  app remotely. Send `Back` over RPC (`gui_send_input_event_request` with
  `PRESS`/`SHORT`/`RELEASE`), then start the app again. Do not re-flash or rewrite
  code chasing that message.
- **`MissingImports` on load** means the FAP was built for a newer API than the
  device runs. Check `firmware_api_major/minor` over RPC; this repo targets
  API 88.2, tested on official firmware 1.5.1.
- **A display change that does not appear** is usually a stale FAP on the device,
  not a draw bug. Rebuild, reinstall, and re-capture before digging into code.
- `sources=["*.c*", "!tests"]` in `application.fam` is deliberate. The default
  glob is recursive and would compile the host test file into the FAP.

## Coding rules

- Match the Flipper coding style. `.clang-format` is in the repo; run `ufbt lint`
  before pushing, it is a local gate and is not part of the GitHub workflow.
- Keep `bleepsentry_logic.c` free of Flipper headers so it stays host-testable.
  New arithmetic, state transitions, or string formatting belong there, with a
  case in `tests/test_logic.c`.
- Never block while holding `app->mutex`: the draw callback takes the same lock.
  The vibro pulse runs after the mutex is released for exactly this reason.
- Write only to `APP_DATA_PATH(...)`, which resolves to
  `/ext/apps_data/bleepsentry/`. Never write into `/ext/apps/` or another app's
  directory. Shared folders such as `/ext/subghz` are read-only.
- Free every allocation on every exit path. One `File` and one `Storage` record
  exist here; both must be released in `bs_log_close`.
- No dead code, no leftover debug logging, no placeholder branches in shipped
  sources.
- All on-screen strings are ASCII and English. The font has no glyphs for em
  dashes, ellipses, or accented characters; they render as blanks.

## Display layout contract

The screen is 128x64. Positions are in pixels and are load-bearing, do not
"tidy" them without re-capturing the screenshots:

| Element | Position | Font |
|---|---|---|
| Title `Bleepsentry` | x=2, baseline y=10 | `FontPrimary` |
| Frequency | x=2, baseline y=22 | `FontSecondary` |
| `RSSI: n dBm` | x=2, baseline y=32 | `FontSecondary` |
| Baseline/threshold, `Sampling noise floor`, or `No baseline (press OK)` | x=2, baseline y=42 | `FontSecondary` |
| RSSI meter | x=2, y=45, 120x8 | frame plus filled bar |
| Status line | x=2, baseline y=61; `ALERT` at x=70 | `FontSecondary` / `FontPrimary` |

- `FontSecondary` caps are 7 rows ending on the baseline and descenders run two
  rows below it. y=61 keeps the tails off the last pixel row; verify with a
  captured frame (maximum ink row should be 62) instead of assuming.
- The numeric readout and the meter share one range, `RSSI_MIN_DBM` to
  `RSSI_MAX_DBM` (-115 to -25). Keep it that way: a number outside the meter's
  scale with an empty bar under it reads as a bug.
- Do not show a prompt that contradicts the current mode. A running calibration
  must not say "press OK", that restarts it.

## Screenshots

Catalog rule: at least one screenshot, exactly 512x256, landscape, exactly two
colours, orange `(254,138,44)` and black `(0,0,0)`, no borders, annotations,
alpha, greys, or anti-aliasing.

- Polarity is qFlipper's, not the marketing look: qFlipper's `StreamOverlay.qml`
  sets a black foreground over the `#fe8a2c` page and saves the 128x64 canvas at
  4x nearest-neighbour. A genuine export is therefore black ink on orange paper,
  roughly 85 to 90 percent orange. Do not invert it.
- The catalog bundler treats bright pixels as transparent paper and dark pixels
  as ink, so an inverted screenshot also renders wrong on the app page.
- Capture from the device, not from a mock-up: read the 128x64 page-packed
  framebuffer over the RPC screen stream
  (`flipperzero_protobuf`, `rpc_gui_snapshot_screen`), then render at 4x with the
  exact colour pair above. Drive each state with RPC input events and wait out the
  app's own timing (2 s calibration, 1 s sweep dwell) before grabbing a frame.
- Provenance lives in `screenshots/README.md`. Do not describe captures in
  `changelog.md` or the README as scaled, rescaled, recoloured, or converted, even
  if a script did the rendering; describe the artifact (512x256, two colours, on
  device) and point at the note.
- After any UI change, re-shoot all four states and verify each file: 512x256,
  exactly two colours, and layout equal to a fresh capture of the same build.

## Catalog submission

The catalog repository only holds `manifest.yml`; the code and screenshots stay
here.

- Manifest path: `applications/Sub-GHz/bleepsentry/manifest.yml`, folder name,
  `id`, and `application.fam`'s `appid` must agree, and the category must match
  `fap_category`.
- `sourcecode.location.commit_sha` must be the full 40-character SHA of the commit
  that contains the code **and** the screenshots. Any new commit means updating
  that field and pushing it. A stale SHA makes CI build an older tree.
- `description` and `changelog` load from a repo file only with a leading `@`.
- Bump `fap_version` (`major.minor`) for every resubmission and add a matching
  `## x.y` section to `changelog.md`.
- The catalog markdown subset for `changelog.md` and `docs/description.md` is
  narrow: H1 and H2, bold, italic, lists, links. No H3, no backticks, no fenced
  code, no images, no tables, no blockquotes, no HTML.
- Validate locally before touching the catalog repo:
  `python3 tools/bundle.py --nolint applications/Sub-GHz/bleepsentry/manifest.yml bundle.zip`
  from a checkout of the catalog repository.
- A first-time contributor's workflow run sits at `action_required` until a
  maintainer approves it. That is not a failure and produces no review comment.

## Definition of done for a change

1. `ufbt` builds clean and `ufbt lint` passes.
2. Host logic tests pass; new logic has a test.
3. The app is installed and exercised on hardware (launch, each affected mode,
   `Back` to exit).
4. If anything visible changed, screenshots are re-shot and verified, and
   `screenshots/README.md` still matches reality.
5. `fap_version` and `changelog.md` are updated.
6. Pushed, CI green, and the catalog manifest's `commit_sha` points at that commit.
