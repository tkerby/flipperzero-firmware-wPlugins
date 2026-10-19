# Detection algorithm — improvement plan

What "the detection algorithm" is in this project, and where it stands vs state of
the art as of this plan:

- **Reference tools compared**: latest pwnagotchi master (delegates all packet
  detection to bettercap v2), bettercap's `wifi` module
  (`modules/wifi/wifi_recon_handshakes.go`, `packets/dot11.go`), hcxdumptool
  (the SOTA ESP32 PMKID/4-way tool — closest hardware analog), gopacket
  (bettercap's frame parser).
- **Confirmed correct** (no action): pwnpal's EAPOL offsets match gopacket
  byte-for-byte (`key_info` @ `eo+5`, nonce @ `eo+17`, KDL @ `eo+97`,
  key data @ `eo+99`); the PMKID OUI check (`DD 00 0F AC 04`, non-zero) matches;
  the crackability rule (PMKID alone, or M1+M2) matches bettercap's
  `Any() = PMKID || Half() || Complete()`.
- **Confirmed gaps**, in priority order:

| # | Gap | Reference behaviour | Phase |
|---|-----|---------------------|-------|
| 1 | Handshakes tracked per-AP, not per-client → M1 from client A + M2 from client B counts as a "handshake" that won't crack | bettercap pairs per (AP, station); hcxdumptool adds same replay counter + 50 ms M1→M2 window | C |
| 2 | No network classification → deauths open nets (useless), assoc/PMKID-solicits WPA3/SAE-only APs (useless) | bettercap `Dot11ParseEncryption` + `deauthOpen=false` default | D |
| 3 | Static channel ordering (most-populated first) | pwnagotchi's RL agent learns channel/attack policy from epoch features (pwnpal already emits the same telemetry) | E |
| 4 | No SAE/WPA3 handshake capture | hcxdumptool + bettercap both capture SAE 4-ways | F (phase 2) |

Cross-cutting: the EAPOL classification logic lives in `Pwnpal.cpp` (hardware
file) and has **no host tests**, while `pwnpal_frames.h` is the established
home for pure, host-testable parsers. Phases A/B fix that first, because every
later phase builds on it.

---

## Status

- **Phase A — DONE** (`4f30d2e`): EAPOL parsing extracted to `pwnpal_frames.h`
  (`pwnpal_eapol_locate` / `pwnpal_eapol_key` / `pwnpal_ds_bssid`); `reportHandshake` rewired
  with no behaviour change; `tests/test_eapol.cpp` added; `run.sh` now runs ASan+UBSan.
- **Phase B — DONE** (`a890a2d`): per-(AP, client, replay) 4-way pairing (32-slot table).
- **Phase B.1 — count vs crackable split** (`pwnpal_hs_note`, two verdicts per frame): **loose** —
  pwnagotchi/bettercap `Complete()`: PMKID, or M2 with an M1/M3, no replay/time match → the pwn
  count / skull; **strict** — a replay-matched M1+M2 **or M2+M3** in a 2 s window, or a PMKID →
  the per-AP *crackable* ✓. Adds the previously-missing **M2+M3** case. (Also fixed here: the
  `PWNPAL_HS ` prefix length regressed to 13 in the rebrand, so a from-HEAD build dropped every
  handshake line.)
- **Phase B.2 — strict check moved to the Flipper + WPA3 exclusion**: the crackable verdict now
  runs *on the Flipper*, over the exact bytes it writes to `<bssid>.pcap` (`pwnpal_note_crackable`),
  not in the ESP's ephemeral RAM. So ✓ means "the saved file will crack," it can flip true later as
  more frames land, it survives ESP reboots, and it needs **no firmware match** (the fap owns it;
  `PWNPAL_CRACK` is gone). WPA3-SAE (`kdv 0`) is excluded from strict — hashcat can't take it —
  while still counting loose. Parsers stay in `pwnpal_frames.h` (pure C, compiled into both).
- **Phase B.3 — "Verify captures" re-read**: the live check is forward-only (it can't label loot
  already on the SD). A menu action (`pwnpal_verify_thread` / `pwnpal_pcap_crackable`) walks
  `handshakes/*.pcap` on a background thread, runs the same parsers over each file with a generous
  window, and sets `crackable` on the matching AP (persisted). Validated: on a 7-capture session it
  flags exactly the 3 hcxpcapngtool calls crackable, 0 false positives.
- **► NEXT: field-validate on hardware** — capture fresh + run Verify captures, confirm the `crack`
  tally matches `hcxpcapngtool` on the pulled pcaps, then Phase C.
- **Phase C / D / E — not started.**

Unit-test status: `tests/run.sh` green under `-fsanitize=address,undefined` (test_eapol,
test_pairing, test_frames, test_geo). Firmware compiles at 67% flash, +1.3 KB RAM.

---

## Phase A — Extract EAPOL parsing into `pwnpal_frames.h` + host tests  ✅ DONE (`4f30d2e`)

Goal: the frame logic of `reportHandshake()` becomes pure, tested code. No
behaviour change to the device yet.

**A1. New pure parsers in `pwnpal_frames.h`** (signature style:
`static inline bool pwnpal_x(const uint8_t* f, int len, ...)`):

- `pwnpal_eapol_locate(f, len) -> int eo` — the EAPOL offset. Replaces the
  hardcoded `payload[30]/[32]` etherType check with a proper walk: scan for
  `88 8e` in the first ~40 bytes (bounded, bounds-checked) instead of two
  magic offsets. Strictly more robust (handles any header layout), zero cost.
- `pwnpal_eapol_key(f, len, eo) -> struct PwnpalEapolKey` with fields
  `key_ack, key_mic, secure, install`, `nonce[32]`, `nonce_nz`,
  `replay[8]`, `pmkid[16]`, `pmkid_nz`. Reimplements the existing logic from
  `reportHandshake()` (Pwnpal.cpp:1169) verbatim, bounds-checked.
- `pwnpal_ds_bssid(f, len, client) -> const uint8_t*` — BSSID derivation from
  DS bits (already in `reportHandshake`/`reportClient`; shared, one place).

**A2. New test file `tests/test_eapol.cpp`** (same style as `test_frames.cpp`,
run by the existing `tests/run.sh` + CI):

- Synthetic frames built to the real layout (EAPOL hdr, key descr, key info,
  key length, replay, nonce, IV, RSC, key id, MIC, KDL, key data):
  - M1 with PMKID (`key_ack`, KDL=22, `DD 14 00 0F AC 04` + 16 non-zero) →
    classified M1, pmkid found, nonce banked.
  - M1 with **zero** PMKID → not classified as pmkid (the "fake PMKID" case).
  - M1 with PMKID but KDL=18 (WPA1 layout is `DD 00 50 F2 02` — assert
    *not* matched; we don't support it yet, must not false-positive).
  - M2 (no ack, mic set, non-zero nonce, same replay as M1) → M2.
  - M3 (install+ack+mic), M4 (ack+mic+secure) → correct bits.
  - Truncated frames at every field boundary → no overread (ASan run in CI:
    add `-fsanitize=address` to `run.sh`).
  - Non-EAPOL frames (DATA non-EAPOL, mgmt) → `pwnpal_eapol_locate` returns none.

**A3. Rewire `reportHandshake()`** to call the new parsers. Device behaviour
identical; diff review should show logic moved, not changed.

Exit criteria: `tests/run.sh` green incl. ASan; PMKID/M2 counts in a field
capture session unchanged from before the rewire.

---

## Phase B — Per-client handshake pairing (the correctness fix)  ✅ DONE (`a890a2d`), validating

Goal: a "handshake" only counts when M1 and M2 are from the **same client**,
carry the **same replay counter**, and land within a **time window** —
exactly hcxdumptool's M12 rule (50 ms; relaxed for us, see B3).

**B1. State.** Replace `ReconAP::hs_anonce` / `hs_m2` (Pwnpal.h:200-201) with a
global half-handshake table (802.11 4-ways are per-(AP, client), and clients
roam, so it can't live per-AP):

```c
struct HsHalf {
  uint8_t  ap_idx;     // index into _recon
  uint8_t  client[6];  // Addr1 of M1/M2 (the client in both directions)
  uint8_t  replay[8];  // EAPOL replay counter
  uint32_t ms;         // millis() when seen
  bool     is_m1;      // M1 (ANonce) vs M2 (SNonce)
};
#define MAX_HS_HALFS 32
```

~20 B/entry → ~640 B RAM. Eviction: overwrite the oldest `ms` (one linear
scan, 32 entries, in the rx callback — cheap). The table is cleared when the
recon table flushes (`endEpoch` MAX_RECON path) so `ap_idx` stays valid.

**B2. Matching rule** in `reportHandshake()`:
- M1 seen → upsert half `(ap, client, replay, now, is_m1=true)`.
- M2 seen → look for a half with same `ap_idx` + `client` + `replay`,
  `is_m1=true`, age ≤ `HS_PAIR_WINDOW_MS` → **handshake** (mark pwnd, clear
  the halves for that client). No match → store the M2 half anyway
  (handles M2-before-M1 capture order, which is possible on a hop boundary),
  but mark pwnd only if a matching M1 half arrives within the window.
- PMKID path unchanged (self-contained, no pairing).

**B3. Window.** hcxdumptool uses 50 ms because it camps one channel; pwnpal
hops every 1.2 s, so M1/M2 can straddle a hop with a few hundred ms gap.
Use `#define HS_PAIR_WINDOW_MS 300` — still 6× tighter than today's
effectively-unbounded pairing, rejects stale cross-client pairs, tolerates a
hop. Make it a named constant so a field test can tighten it if false
positives persist.

**B4. Tests** (`tests/test_eapol.cpp` or new `tests/test_pairing.cpp`): the
table + matcher as a pure struct/function in `pwnpal_frames.h`
(`pwnpal_hs_match(...)`); cases: same client same replay in window → match;
different client → no match; different replay → no match; out of window → no
match; M2 before M1 → matches when M1 lands; table full → evicts oldest;
recon flush clears stale `ap_idx`.

**B5. Protocol.** No wire change — `PWNPAL_PWND type=handshake` now means
what it claims to. Optionally add `"pair":"<client>"` to the line for the
Flipper AP-detail view. Confirmed safe: the Flipper parses lines with
`line_extract_*` key-greps (`pwnpal_app.c`), which ignore unknown keys, so
additive JSON keys never require a `PWNPAL_PROTO` bump.

Exit criteria: in a field session, every reported `handshake` pcap actually
cracks (or is accepted by `hcxpcaptool` as a complete 4-way); zero
cross-client pairs in the log.

---

## Phase C — Network classification + attack gating (the efficiency fix)

Goal: know what we're attacking; stop wasting airtime (and user goodwill) on
nets where no key material can exist.

**C1. Pure classifier in `pwnpal_frames.h`**, reusing the tagged-IE walker
already there (the one `pwnpal_rsn_requires_pmf` uses):

```c
enum PwnpalNetClass {
  NET_OPEN = 0,   // no WPA/WPA2/RSN IE, no privacy bit
  NET_WPA1,       // vendor OUI 00:50:F2 (WPA)
  NET_WPA2,       // RSN IE
  NET_WPA3_SAE,   // RSN with SAE AKM (00:0F:AC:08) and no PSK/PMK AKM
  NET_WPA3_PSK,   // RSN with PMK AKM (00:0F:AC:01) — EAPOL, PMKID-capable
};
// returns class; also out-params: akm_psk (PMKID-capable), akm_sae
static inline int pwnpal_classify_net(const uint8_t* f, int len, ...);
```

Classification rules (mirror bettercap `Dot11ParseEncryption`):
- privacy capability bit in the beacon fixed params + RSN AKM suite scan:
  any AKM `00:0F:AC:02` (PSK) or `00:0F:AC:01` (802.1X-PMK) → PMKID-capable;
  only `00:0F:AC:08` (SAE) → SAE-only; WPA vendor IE (OUI `00:50:F2`) → WPA1;
  none of the above → OPEN.
- Store in `ReconAP` as one byte (`uint8_t net_class`) + bool
  `pmkid_capable`. Memory: +2 B per AP entry (fine at MAX_RECON=128).

**C2. Gating** (the behaviour change):
- `attackable()`: require `pmkid_capable` for the assoc/PMKID path —
  don't auth+assoc SAE-only or open APs.
- `deauthable()`: additionally require class != OPEN and class != SAE-only
  (a deauth on an open net kicks clients for nothing; on SAE-only it only
  buys handshakes we can't use yet — until Phase F lands, SAE-only deauth is
  off by default, behind a flag so Phase F can switch it on).
- New epoch telemetry in `PWNPAL_EPOCH`: `"skipped_open":N,"skipped_sae":N`
  so the airtime saving is visible in the log.

**C3. Tests** (`tests/test_frames.cpp` already has the `RSN_IE` macro — extend
it): open (no IEs), WPA1 vendor, WPA2 PSK, WPA2 802.1X, WPA3 SAE-only,
WPA3 transitional (SAE+PSK → PMKID-capable), truncated IEs, RSN after other
IEs.

**C4. Protocol/Flipper.** Additive `"enc":"wpa2|open|wpa3-sae|..."` key to
`PWNPAL_AP` (and `PWNPAL_PWND`); Flipper AP list can show it later. No
`PWNPAL_PROTO` bump needed (parser ignores unknown keys — see B5).

Exit criteria: field session log shows zero deauth/assoc bursts aimed at
open or SAE-only APs; `skipped_*` counters non-zero in a mixed environment;
capture rate on WPA2 nets unchanged.

---

## Phase D — Channel-yield weighting ("AI-lite")

Goal: replace static most-populated-first ordering with per-channel learned
yield, using only on-device RAM — the ESP32-appropriate version of
pwnagotchi's RL agent.

**D1. State.** Per-channel (14 slots):
```c
struct ChanStat { uint16_t attacks; uint16_t yields; };  // since session start
```
- credit `attacks` in `attackChannel()` per attackable AP touched;
- credit `yields` in `emitPwnd()` for the frame's channel.

**D2. Use.**
- `buildAttackList()`: sort key = `yields` desc, tie-break AP count desc
  (a channel that produces captures beats a busy dead channel).
- `channelDwellMs()`: multiply the current weight formula by
  `1 + min(yields, 4) / max(attacks, 1)` — channels with a proven
  handshake-per-attack ratio get up to 5× airtime.
- Aging: at `endEpoch()` decay both counters (`/2`, saturating) so a channel
  that was productive a session ago doesn't dominate forever — mirrors
  pwnagotchi's per-epoch feature reset.

**D3. Tests.** Extract the sort+weight into a pure function over the
`ChanStat` table in `pwnpal_frames.h`; test ordering, tie-breaks, decay
saturating at 0, empty table.

Exit criteria: in a multi-channel field session, the log shows dwell
concentrating on the channel(s) that actually produced `PWNPAL_PWND` within
2–3 epochs, vs uniform dwell before.

---

## Phase E — SAE / WPA3 capture (phase 2, feature-sized)

Goal: capture SAE 4-ways so WPA3-PSK/SOE networks become crackable with
`hashcat -m 20200` / hcxtools. Out of scope until Phases B–D land; sketched
here so the design is agreed:

- Detect SAE auth frames (mgmt subtype 4 = commit, 5 = confirm) — the DS-bit
  client is the station, as with EAPOL.
- Pair commit/confirm per (AP, client) with the same Phase B table (reuse
  `HsHalf`, extend `is_m1` to a 2-bit frame kind: eapol-m1 / eapol-m2 /
  sae-commit / sae-confirm).
- Stream into the per-BSSID pcap under a new `type` (`"sae"`) so the Flipper
  writes the same pcap path; `hcxpcaptool` handles SAE extraction.
- Only worth enabling once C2's SAE-only deauth gate can be switched on.

---

## Cross-cutting

**Ordering & dependencies:** A → B → C → D, each independently shippable and
field-testable; E depends on B's table. A is a pure refactor (no behaviour
change), B is precision (fewer *false* pwnds), C is efficiency (fewer wasted
attacks), D is targeting (more airtime where it works).

**Protocol bumps:** none — all new JSON keys are additive and the Flipper's
key-grep parser ignores unknowns (verified in `pwnpal_app.c`). `PWNPAL_PROTO`
stays at 6.

**RAM budget:** +~1.3 KB total (half-table 640 B, net class +2 B × 128,
channel stats ~100 B) — negligible on ESP32 (320 KB+ free after Marauder's
allocation; Marauder itself runs ~200 AP tables).

**Test infra:** `tests/run.sh` already runs any `test_*.cpp` with
`-Werror`; add `-fsanitize=address -fsanitize=undefined` to the compile line
so every parser gets overread protection for free. New test files:
`test_eapol.cpp` (A/B), extend `test_frames.cpp` (C), `test_channels.cpp` (D).

**Field validation protocol** (do this between every phase, it's the real
ground truth):
1. Park the friend next to 2–3 known WPA2 test APs you own.
2. Record a full session with deauth on; pull the pcaps.
3. Run `hcxpcaptool -o . <bssid>.pcap` / `hashcat --example-hashes`-style
   validation: every `type=handshake`/`pmkid` report must yield a complete
   4-way or PMKID; count `type=handshake` false positives before/after Phase B.
4. Diff `PWNPAL_EPOCH` lines: attack counts should drop after Phase C (skips),
   pwnd counts should not.

**What we deliberately do NOT do:** full on-device RL (pwnagotchi's
`ai/` gym trains offline on epoch logs; the honest future step is training
*offline* on the `PWNPAL_EPOCH` telemetry we already emit and porting the
policy weights on-device — a later project, not this plan).
