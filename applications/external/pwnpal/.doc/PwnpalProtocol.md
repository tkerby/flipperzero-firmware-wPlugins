# Pwnpal Protocol

Pwnpal makes the Flipper Zero act as a social **pwngrid** peer so that a nearby
Pwnagotchi detects it, greets it ("Hello friend! Nice to meet you."), and — because the
friend keeps a stable identity — counts encounters over time and promotes it to a
"good friend" with the ♥‿‿♥ face.

There are two independent wire formats involved:

1. **The pwngrid air protocol** — the 802.11 beacon the ESP32 broadcasts. This is what
   the Pwnagotchi actually hears. It is fixed by `evilsocket/pwngrid`; we just reproduce it.
2. **The Flipper ↔ ESP32 serial protocol** — how the Flipper tells its ESP32 board what
   persona to advertise and receives reports of detected units. This one is ours.

---

## 1. pwngrid air protocol (ESP32 → air → Pwnagotchi)

A Pwnagotchi's mesh is handled by the `pwngrid-peer` daemon. It advertises itself, and
discovers others, using **802.11 management beacon frames** with a fixed signature. Both
`pwngrid` (`wifi/pack.go`, `mesh/peer.go`) and ESP32 Marauder (`WiFiScan.cpp`,
`processPwnagotchiBeacon`) agree on this layout.

### Frame layout

```
off  bytes  field
  0    2    Frame Control      0x80 0x00   (type=mgmt, subtype=beacon)
  2    2    Duration           0x00 0x00
  4    6    Address1 (DST)     ff ff ff ff ff ff        broadcast
 10    6    Address2 (SRC)     de ad be ef de ad        THE pwngrid signature MAC
 16    6    Address3 (BSSID)   <6-byte session id>      random, stable per session
 22    2    Seq-ctl            0x00 0x00                (hw overwrites)
 24    8    Timestamp          any                      (hw overwrites)
 32    2    Beacon interval    0x64 0x00                100 TU
 34    2    Capability info    0x00 0x00
 36    1    IE id              0xDE (222)               IDWhisperPayload
 37    1    IE length          <json length, <=255>
 38   ...   IE data            <advertisement JSON, UTF-8>
```

Detection is keyed entirely on **Address2 == `de:ad:be:ef:de:ad`**. Marauder locates the
JSON by scanning from offset 36 for the first `{` and back from the end for the last `}`
(`WiFiScan.cpp:processPwnagotchiBeacon`), so a single uncompressed payload IE — exactly
what a real Pwnagotchi sends — is what we reproduce.

> pwngrid *can* also emit IE 223 (compression), 224 (identity), 225 (signature) and
> 226 (stream header) for its message-routing layer. None are needed to be seen as a
> peer: `mesh/peer.go` accepts an advertisement with only `identity` and **no public
> key and no signature** (the signature block is commented out; a missing public key is
> only a debug log). We therefore send one plain payload IE and nothing else.

### Advertisement JSON

Minimum for the Pwnagotchi to accept and greet the peer:

```json
{
  "name": "flippy",
  "identity": "3b1e...<64 lowercase hex chars>...9f2a"
}
```

`identity` **must** match `^[a-fA-F0-9]{64}$` (it is normally a public-key fingerprint;
we just use a random-but-stable 32-byte value rendered as hex). The Pwnagotchi keys peers
by `identity`, so keeping it constant is what makes the friendship *persist and grow*.

Full persona we actually send, so the friend renders like a real unit on the Pwnagotchi's
screen and in Marauder's `sniffpwn` output:

```json
{
  "name": "flippy",
  "identity": "<64 hex>",
  "version": "1.0.0",
  "grid_version": "1.10.3",
  "session_id": "de:ad:be:ef:de:ad",
  "face": "(♥‿‿♥)",
  "pwnd_run": 0,
  "pwnd_tot": 0,
  "uptime": 12345,
  "epoch": 0,
  "policy": { "advertise": true, "deauth": false, "bond_encounters_factor": 20000 },
  "timestamp": 1690000000
}
```

Field use on the receiving Pwnagotchi (`pwnagotchi/mesh/peer.py`, `ui/view.py`):

| field       | used for                                                        |
| ----------- | --------------------------------------------------------------- |
| `name`      | greeting text and the friend label at bottom-left               |
| `identity`  | peer key; **required**, 64 hex                                  |
| `face`      | the little friend face drawn next to the signal bars            |
| `pwnd_run`  | first number in `name N (M)`                                    |
| `pwnd_tot`  | second number `(M)`                                             |
| `version`   | shown in logs / Marauder                                        |
| `policy.deauth` | read by Marauder's detector for its readout                 |

RSSI/bars are derived by the Pwnagotchi from the received signal, not from JSON.

### Channels

Pwnagotchis hop channels. The ESP32 must broadcast across the common 2.4 GHz channels
(1–13) on a rotation so it is heard regardless of where the Pwnagotchi currently sits.
The friend beacon is re-sent every ~half second per the pwngrid `SignalingPeriod` feel.

---

## 2. Flipper ↔ ESP32 serial protocol

Transport: **UART, 115200 8N1**, on the Flipper's default USART (`FuriHalSerialIdUsart`,
GPIO 13 TX / 14 RX) — the same link Marauder's CLI already uses, so it works on the
official Wi-Fi Dev Board and on the Feberis Pro without rewiring.

Commands are newline-terminated ASCII lines, matching Marauder's CLI parser
(`CommandLine.cpp`). Pwnpal adds one command.

### Flipper → ESP32

```
pwnpal -n <name> -id <64hex> -f <faceIdx> -pr <pwnd_run> -pt <pwnd_tot> -u <uptime> -e <epoch> [-ch <n>] [-deauth]
```

- `-n`   persona name (no spaces; use `_`, rendered back to space by the app if desired)
- `-id`  64-hex identity (stable across the persona's life)
- `-f`   face index into the shared face table (see below); ESP32 maps it to the glyph
- `-pr`  pwnd this run — now the count of **real** handshakes/PMKIDs captured this session
- `-pt`  pwnd total — lifetime count of real captures (no longer a social score)
- `-u`   uptime seconds
- `-e`   epoch
- `-ch`  optional: pin to a single channel instead of hopping
- `-deauth`  optional flag, **appended only when the user has opted in** (default off).
  When present the friend may actively deauth to force a handshake, and advertises
  `policy.deauth: true` so the mesh sees the real policy. Absent = passive only. See
  [§4 Safety & authorization](#4-safety--authorization).

On receipt the ESP32 (re)builds the advertisement JSON, starts/refreshes broadcasting,
and simultaneously sniffs for other Pwnagotchis. `stopscan` (existing Marauder command)
stops it.

To update only the live persona without restarting, the same line may be re-sent; the
broadcaster picks up the new values on its next beacon.

### ESP32 → Flipper

The ESP32 emits one line per detected Pwnagotchi, prefixed so the Flipper can filter it
out of Marauder's other chatter:

```
PWNPAL_PEER {"name":"kitty","identity":"<hex-or-empty>","pwnd_tot":42,"pwnd_run":3,"uptime":1234,"rssi":-55,"channel":6,"deauth":false}
```

and a heartbeat when broadcasting is (re)confirmed:

```
PWNPAL_ADV name=flippy ch=6 sent=128
```

In full pwnagotchi mode the ESP32 also scans APs and captures handshakes, and reports
those with three more events.

An access point seen while scanning (drives the on-screen **APS** count and the pool of
capturable targets), deduped per BSSID. `lat`/`lon` are appended only when the GPS has a
fix — see [§5 GPS geotagging](#5-gps-geotagging--wardrive-log):

```
PWNPAL_AP {"bssid":"aa:bb:cc:dd:ee:ff","ssid":"NAME","channel":6,"rssi":-61[,"lat":48.208,"lon":16.373]}
```

For the first beacon of each BSSID, `reportAP()` **also** streams that beacon as a
`PWNPAL_HS` line (below). That is what puts the ESSID-bearing beacon into the network's
pcap and makes it actually crackable.

A captured handshake / PMKID — the friend's **earned pwnd**, deduped per BSSID per
session; `lat`/`lon` again appended only on a GPS fix:

```
PWNPAL_PWND {"bssid":"aa:bb:cc:dd:ee:ff","ssid":"NAME","type":"handshake","channel":6,"rssi":-61[,"lat":48.208,"lon":16.373]}
```

`type` is `handshake` for an EAPOL M2, or `pmkid` for an RSN PMKID from M1. `ssid` is
sanitized the same way as `PWNPAL_PEER` names and may be `""` for a hidden AP. The

**Miss (active mode only)** — after the ESP32 has assoc/deauth'd an AP `MISS_ATTEMPTS`
(4) times with no capture, it emits a single line so the brain can flash the demotivated
face (pwnagotchi's `on_miss`):

```
PWNPAL_MISS aa:bb:cc:dd:ee:ff
```

Flipper de-dupes both AP and capture lines by `bssid` across the whole app session, so the
command being re-sent every ~15 s never double-counts a network and the `-pt`/`-pr` counts
fed back stay honest.

The raw 802.11 frame behind each capture (and the first beacon per BSSID), for the
crackable pcap — one frame per line, **self-describing**: the BSSID as 12 lowercase hex
chars with no colons, a space, then the full frame as lowercase hex:

```
PWNPAL_HS <bssid12hex> <lowercase-hex-of-the-full-802.11-frame>
```

e.g. `PWNPAL_HS aabbccddeeff 8000000000...`. The Flipper files the frame into
`<bssid>.pcap` by parsing the BSSID on **this** line — it no longer depends on a preceding
`PWNPAL_PWND` to know the target. This matters because most EAPOL frames (M1/M3/M4 and
PMKID) never produce a `PWND`, and lines from the rx-callback can arrive out of order; a
self-describing HS line can't be misfiled under the wrong (or the default) pcap.

Hex, not raw binary, because the Flipper CLI UART mangles raw CR / XON / XOFF bytes; hex
is line-safe and self-synchronising on the `\n` boundary. Only beacons (one per BSSID) and
EAPOL / PMKID frames are streamed (never bulk data), so a line stays small. See
[§4 Safety & authorization](#4-safety--authorization) for the pcap format and location.

Lines are `\n`-terminated. Any line not starting with `PWNPAL_` is ordinary Marauder
output and the app ignores it. **Each `PWNPAL_*` line is emitted as one atomic
`Serial.write`** of a fully built buffer (line + trailing `\n`): the WiFi rx-callback task
and the main loop both print, and a byte-at-a-time write would let their lines interleave
and garble.

---

## 3. Shared face table

Both sides use the same ordering as `pwnpal/include/face.h`, so a single `-f <index>` selects the same face on the Flipper's
own screen and in the advertised glyph:

| idx | name         | glyph      |
| --- | ------------ | ---------- |
| 8   | Awake        | (◕‿‿◕)   |
| 12  | Happy        | (•‿‿•)    |
| 14  | Excited      | (ᵔ◡◡ᵔ)    |
| 15  | Motivated    | (☼‿‿☼)    |
| 17  | Lonely       | (ب__ب)     |
| 21  | Friend       | (♥‿‿♥)    |

(Full list in `pwnagotchi.h`.) The friend's face is chosen by its mood, which is driven
by how many peers it has met recently — see the app README.

---

## 4. Safety & authorization

Presence (advertise + peer sniff) is the default and is unrestricted — it only broadcasts
a beacon and listens. Capture and deauth are **opt-in and off by default on both ends**:

- The Flipper app stays in presence-only mode until you explicitly enable capture. Deauth
  is a further, separate opt-in that resets to off every launch; it is only meaningful
  with capture on, and requires its own confirmation.
- `-deauth` is **only appended to the `pwnpal` command when the user has turned deauth
  on**. Absent = passive only, which is the default. There is no default-on path to
  transmitting a deauth.
- Whenever deauth is active the advertised `policy.deauth` is `true`; the friend never
  deauths while advertising `deauth:false`, so the mesh always sees the real policy.

> Handshake/PMKID capture and deauth are only legal on Wi-Fi networks you own or are
> explicitly authorized to test. Unauthorized use may be a crime where you live. You are
> responsible for how you use this. Educational use only.

### pcap output

Frames streamed via `PWNPAL_HS` are hex-decoded on the Flipper and appended to a
standard libpcap file — **linktype 105 (LINKTYPE_IEEE802_11**, bare 802.11, matching
Marauder's own pcap byte-for-byte), so aircrack-ng, `hcxpcapngtool` and Wireshark read it
directly. There is **one pcap per network, named by BSSID**, on the Flipper SD:

```
/ext/apps_data/pwnpal/handshakes/<bssid>.pcap
```

Because `reportAP()` streams the first beacon of each BSSID and every capture's
`PWNPAL_HS` line carries its own BSSID, each file holds that network's ESSID-bearing
beacon **plus** its EAPOL/PMKID frames. That makes it directly crackable — feed it to
`hcxpcapngtool` → hashcat mode 22000 (or aircrack-ng) with no manual `-e`/ESSID needed,
because the ESSID is a mandatory 22000 field and now lives in the file.

Each file grows a 24-byte global header once, then one 16-byte record header + frame per
captured frame; the file stays valid even if the board is yanked mid-capture.

---

## 5. GPS geotagging & wardrive log

Boards with a GPS (the **Feberis Pro**) geotag what they see. The ESP32 reads Marauder's
global `gps_obj` and, **only when it reports a fix**, appends `lat`/`lon` (decimal-degree
floats) to the JSON of each `PWNPAL_AP` and `PWNPAL_PWND` line:

```
PWNPAL_AP   {"bssid":"aa:bb:cc:dd:ee:ff","ssid":"NAME","channel":6,"rssi":-61,"lat":48.208,"lon":16.373}
PWNPAL_PWND {"bssid":"aa:bb:cc:dd:ee:ff","ssid":"NAME","type":"handshake","channel":6,"rssi":-61,"lat":48.208,"lon":16.373}
```

With **no fix, both keys are omitted entirely** — never sent as `0` or `null`. All GPS
reads are wrapped in `#ifdef HAS_GPS`, so non-GPS boards (the Wi-Fi Dev Board) build
unchanged and simply never emit `lat`/`lon`.

The Flipper turns every geotagged line into a **WiGLE-importable** wardrive CSV on its SD:

```
/ext/apps_data/pwnpal/wardrive.csv
```

A WiGLE pre-header line, then the column header, then one `WIFI` row per geotagged AP (and
optionally per capture). Rows without lat/lon are **skipped** — only geotagged sightings
are logged:

```
WigleWifi-1.4,appRelease=pwnpal,model=flipper,release=1.0,device=pwnpal,display=,board=esp32,brand=marauder
MAC,SSID,AuthMode,FirstSeen,Channel,RSSI,CurrentLatitude,CurrentLongitude,AltitudeMeters,AccuracyMeters,Type
aa:bb:cc:dd:ee:ff,NAME,[WPA2],2026-09-19 12:00:00,6,-61,48.208,16.373,140.0,5.0,WIFI
```

Import it straight into wigle.net. The capture authorization caveat in §4 applies to any
handshake rows; plain AP/meeting locations are benign presence data.
