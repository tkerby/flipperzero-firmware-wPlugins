# Adding `pwnpal` to ESP32 Marauder

`Pwnpal.{h,cpp}` are self-contained. To wire them into a Marauder build you drop
both files into `esp32_marauder/` and make six small edits to existing files. Line
numbers below are approximate (they drift between Marauder releases) — search for the
quoted anchor text instead.

Tested against the `esp32_marauder` layout as of ESP32Marauder 2025. The additions do
not touch any existing behaviour; `sniffpwn` etc. keep working unchanged.

---

## 0. Copy the module

```
cp pwnpal-marauder/Pwnpal.h   <marauder>/esp32_marauder/
cp pwnpal-marauder/Pwnpal.cpp <marauder>/esp32_marauder/
```

Marauder already vendors `ArduinoJson` and `LinkedList`, which the module uses.

---

## 1. `esp32_marauder.ino` — instantiate the broadcaster

Near the other global objects (e.g. `WiFiScan wifi_scan_obj;`), add:

```cpp
#include "Pwnpal.h"
Pwnpal pwnpal_obj;
```

and add `extern Pwnpal pwnpal_obj;` wherever the other `extern ... _obj;`
declarations live (typically `configs.h` or the top of `WiFiScan.cpp`).

---

## 2. `WiFiScan.h` — new scan mode

Alongside `#define WIFI_SCAN_PWN 3` add an unused id, e.g.:

```cpp
#define WIFI_SCAN_PWNPAL 111
```

and declare the runner next to `RunPwnScan`:

```cpp
void RunPwnpalScan(uint8_t scan_mode, uint16_t color);
```

---

## 3. `WiFiScan.cpp` — the runner (copy of RunPwnScan)

Right after `WiFiScan::RunPwnScan`, add:

```cpp
void WiFiScan::RunPwnpalScan(uint8_t scan_mode, uint16_t color) {
  (void)scan_mode; (void)color;
  pwnpal_obj.beginSession();   // clear per-session capture dedup at REAL start
  startPcap("pwnpal");
  esp_wifi_init(&cfg2);
  #ifdef HAS_IDF_3
    esp_wifi_set_country(&country);
    esp_event_loop_create_default();
  #endif
  // AP mode + promiscuous: AP is the interface Marauder's own beacon TX uses
  // (esp_wifi_80211_tx on WIFI_IF_AP actually radiates; STA-mode TX silently
  // dropped frames), and promiscuous rx still fires for sniffing peers.
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_start();
  this->setMac();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&beaconSnifferCallback);
  this->changeChannel(this->set_channel);
  this->wifi_initialized = true;
  initTime = millis();
}
```

(Names like `filt`, `cfg2`, `setMac`, `beaconSnifferCallback` come straight from the
fork's own `RunPwnScan`, so they resolve wherever that does.)

`beginSession()` clears the per-session capture dedup tables (`_n_recon` /
`_n_pwnd_seen`). It lives here — at real scan start — and **not** in
`configureFromArgs()`, because the Flipper re-sends the whole `pwnpal` command every
~15s to refresh the persona; clearing on each refresh would re-count already-pwnd APs
and inflate `pwnd_tot` without bound.

---

## 4. `WiFiScan.cpp` — dispatch in `StartScan`

Next to `else if (scan_mode == WIFI_SCAN_PWN) RunPwnScan(scan_mode, color);` add:

```cpp
else if (scan_mode == WIFI_SCAN_PWNPAL)
  RunPwnpalScan(scan_mode, color);
```

---

## 5. `WiFiScan.cpp` — sniff + broadcast in `main()`

Add `WIFI_SCAN_PWNPAL` to the mode list at the top of `WiFiScan::main` that does the
channel-hop block (the `if ((currentScanMode == WIFI_SCAN_PROBE) || ... )`), then hang
the broadcast off the same tick:

```cpp
else if (currentScanMode == WIFI_SCAN_PWNPAL) {
  if (millis() - initTime >= 500) {   // ~pwngrid signaling cadence
    initTime = millis();
    pwnpal_obj.broadcast();        // hops channel + sends the friend beacon
  }
}
```

Broadcasting itself steps the channel, so no separate `channelHop()` is needed here.

Also add `WIFI_SCAN_PWNPAL` to the big `currentScanMode == ...` guard around line
2990 (the one that gates the promiscuous beacon path) and to any `scanning`/`sniffing`
predicate you want it treated as an active scan by (so `stopscan` and the status UI see
it). Search for `WIFI_SCAN_PWN` and mirror each occurrence.

---

## 6. `WiFiScan.cpp` — report sniffed peers

In `beaconSnifferCallback`, where a matched pwngrid MAC currently calls
`processPwnagotchiBeacon`, branch for our mode so we emit the structured
`PWNPAL_PEER` line (which carries rssi + channel that `processPwnagotchiBeacon`
doesn't have):

```cpp
if (mac_match) {
  if (wifi_scan_obj.currentScanMode == WIFI_SCAN_PWNPAL)
    pwnpal_obj.reportPeer(snifferPacket->payload, len,
                             snifferPacket->rx_ctrl.rssi,
                             snifferPacket->rx_ctrl.channel);
  else
    wifi_scan_obj.processPwnagotchiBeacon(snifferPacket->payload, len);
  return;
}
```

Make sure the outer `if ((currentScanMode == WIFI_SCAN_PROBE) || ... )` that wraps the
mgmt-frame branch also includes `WIFI_SCAN_PWNPAL`, otherwise the callback returns
before reaching this code.

---

## 6b. `WiFiScan.cpp` — capture EAPOL/PMKID (DATA frames)

The promiscuous filter already passes DATA (`WiFiScan.h`: `filt = MGMT | DATA`), but
`beaconSnifferCallback` only ever acts on `WIFI_PKT_MGMT`. To capture handshakes we
add a DATA-frame branch **before** the mgmt-only dispatch `if`, i.e. immediately
before the line

```cpp
if ((wifi_scan_obj.currentScanMode == WIFI_SCAN_PROBE) ||
```

insert:

```cpp
if ((wifi_scan_obj.currentScanMode == WIFI_SCAN_PWNPAL) &&
    (type == WIFI_PKT_DATA)) {
  #ifdef HAS_GPS
    bool pf_fix = gps_obj.getFixStatus();
    double pf_lat = pf_fix ? atof(gps_obj.getLat().c_str()) : 0.0;
    double pf_lon = pf_fix ? atof(gps_obj.getLon().c_str()) : 0.0;
  #else
    bool pf_fix = false; double pf_lat = 0.0; double pf_lon = 0.0;
  #endif
  if (pwnpal_obj.reportHandshake(snifferPacket->payload, len,
                                    snifferPacket->rx_ctrl.rssi,
                                    snifferPacket->rx_ctrl.channel,
                                    pf_fix, pf_lat, pf_lon))
    buffer_obj.append(snifferPacket, len);
  return;
}
```

`len` here is still `rx_ctrl.sig_len` (the mgmt path decrements it by 4 only inside the
`WIFI_PKT_MGMT` block), so DATA frames get their full length. `reportHandshake` detects
an EAPOL M2 (→ `type:"handshake"`) or an RSN PMKID KDE in M1 (→ `type:"pmkid"`), dedups
per BSSID for the session, and emits `PWNPAL_PWND` (geotagged with `lat`/`lon` when
the GPS has a fix). It also streams the full frame to the Flipper as a **self-describing**
`PWNPAL_HS <bssid12hex> <framehex>` line — the BSSID is derived from the frame's DS
bits and prefixed so the Flipper files the frame under the right per-BSSID pcap without
depending on a preceding `PWND` (protocol v2; raw binary would trip the serial CLI's
CR/XON handling, so we stream lowercase hex). It returns true for **any** EAPOL frame so
the caller also appends it to the on-board pcap if the ESP32 has an SD.

The `gps_obj` reference is only in scope under `#ifdef HAS_GPS` (the callback declares
`extern GpsInterface gps_obj;` in that same guard), so the GPS reads are guarded to keep
non-GPS boards building. `atof`/`getLat()`/`getLon()` mirror the fork's own wardrive paths.

## 6c. `WiFiScan.cpp` — recon non-pwngrid beacons

Inside the beacon branch (`payload[0] == 0x80`, after the pwngrid `mac_match` return),
immediately **before**

```cpp
if (wifi_scan_obj.currentScanMode == WIFI_SCAN_PWN) {
```

insert:

```cpp
if (wifi_scan_obj.currentScanMode == WIFI_SCAN_PWNPAL) {
  #ifdef HAS_GPS
    bool pf_fix = gps_obj.getFixStatus();
    double pf_lat = pf_fix ? atof(gps_obj.getLat().c_str()) : 0.0;
    double pf_lon = pf_fix ? atof(gps_obj.getLon().c_str()) : 0.0;
  #else
    bool pf_fix = false; double pf_lat = 0.0; double pf_lon = 0.0;
  #endif
  if (pwnpal_obj.reportAP(snifferPacket->payload, len,
                             snifferPacket->rx_ctrl.rssi,
                             snifferPacket->rx_ctrl.channel,
                             pf_fix, pf_lat, pf_lon))
    buffer_obj.append(snifferPacket, len);
  return;
}
```

`len` here is already FCS-stripped (`-4`), fine for SSID parsing. `reportAP` dedups each
non-pwngrid AP into one `PWNPAL_AP` line per BSSID (geotagged with `lat`/`lon` when
the GPS has a fix) and stores it (BSSID + ESSID + channel) so the deauth tick and the
`PWNPAL_PWND` SSID lookup have it. On the **first** beacon per BSSID it also streams
that beacon to the Flipper as a `PWNPAL_HS <bssid> <framehex>` line, so the per-BSSID
pcap contains the ESSID-bearing beacon (a mandatory WPA 22000 field) and is actually
crackable. The stored channel + ESSID are also what the `-deauth` opt-in ("active mode")
uses: on each throttled burst `broadcast()` walks the recon'd APs on the channel it is
currently parked on and, for each, sends an **association request** to solicit the AP's
RSN PMKID (EAPOL M1, no client needed) **and** a **deauth** to force a full 4-way
handshake — pwnagotchi's `associate` + `deauth` halves (agent.py), gated on the persona's
`-deauth` flag (default off; passive mode stays listen-only). No filter change is needed —
`filt` already passes DATA, and both frames go out on `WIFI_IF_AP` from the main loop.

---

## 7. `CommandLine.cpp` / `CommandLine.h` — the command

In `CommandLine.h`, next to `SNIFF_PWN_CMD`:

```cpp
const char PROGMEM PWNPAL_CMD[] = "pwnpal";
```

In `CommandLine.cpp`'s command dispatch (next to the `SNIFF_PWN_CMD` handler):

```cpp
else if (cmd_args.get(0) == PWNPAL_CMD) {
  if (!pwnpal_obj.configureFromArgs(&cmd_args)) {
    Serial.println(F("PWNPAL_ERR bad -id (need 64 hex)"));
  } else if (wifi_scan_obj.currentScanMode == WIFI_SCAN_PWNPAL) {
    // Already running: configureFromArgs() already refreshed + rebuilt the persona.
    // Do NOT StartScan again — that would tear down/re-init WiFi, wipe recon, and
    // reset the channel hop. The ~15s command re-send is only a live persona update.
    Serial.println(F("pwnpal persona updated"));
  } else {
    Serial.print(F("Starting pwnpal. Stop with "));
    Serial.println(STOPSCAN_CMD);
    wifi_scan_obj.StartScan(WIFI_SCAN_PWNPAL, TFT_MAGENTA);
  }
}
```

The `currentScanMode == WIFI_SCAN_PWNPAL` branch is what keeps the recon table, the
capture dedup, and the channel-hop state alive across the Flipper's 15s command re-sends
— only the very first `pwnpal` command starts a scan; later ones just update the
persona. Add `#include "Pwnpal.h"` and `extern Pwnpal pwnpal_obj;` at the top of
`CommandLine.cpp` if not already visible.

---

## Using it directly (without the Flipper app)

Over the board's serial CLI at 115200:

```
pwnpal -n lonelybot -id 3b1e9f...<64 hex>...2a -f 21 -pr 0 -pt 7 -u 3600
```

Your Pwnagotchi should, within a hop cycle or two, flip to a friendly face and announce
"Hello lonelybot! Nice to meet you." Stop with `stopscan`.

The Flipper `pwnpal` app drives exactly this command for you and manages the persona,
so you normally never type it by hand.
