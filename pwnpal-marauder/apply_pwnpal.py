#!/usr/bin/env python3
"""Add the `pwnpal` command to an ESP32 Marauder checkout.

Anchored, idempotent source surgery (automated PATCH.md): finds quoted anchor
strings and inserts our hooks, aborting loudly if an anchor is missing.

Usage:
    python3 apply_pwnpal.py /path/to/marauder            # dir with esp32_marauder/
    python3 apply_pwnpal.py /path/to/marauder/esp32_marauder

Verified against justcallmekoko/ESP32Marauder and bpmcircuits/ESP32Marauder_FEBERIS.
"""

import shutil
import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent


class AnchorError(SystemExit):
    pass


def _read(p: Path) -> str:
    return p.read_text(encoding="utf-8", errors="surrogateescape")


def _write(p: Path, s: str) -> None:
    p.write_text(s, encoding="utf-8", errors="surrogateescape")


def insert_after(text, anchor, addition, tag):
    """Insert `addition` immediately after the line that ends the `anchor` match.

    The anchor may span multiple lines; we anchor to the end of the whole match,
    not its first newline, so a `sig\\n{` anchor inserts after the `{`.
    """
    if tag in text:
        return text, False  # already applied
    idx = text.find(anchor)
    if idx == -1:
        raise AnchorError(f"anchor not found: {anchor!r}")
    anchor_end = idx + len(anchor)
    line_end = text.find("\n", anchor_end)
    if line_end == -1:
        line_end = len(text)
    return text[: line_end + 1] + addition + text[line_end + 1 :], True


def insert_before(text, anchor, addition, tag):
    """Insert `addition` immediately before the line containing `anchor`."""
    if tag in text:
        return text, False
    idx = text.find(anchor)
    if idx == -1:
        raise AnchorError(f"anchor not found: {anchor!r}")
    line_start = text.rfind("\n", 0, idx) + 1
    return text[:line_start] + addition + text[line_start:], True


def replace_once(text, old, new, tag):
    if tag in text:
        return text, False
    if old not in text:
        raise AnchorError(f"replace target not found: {old!r}")
    return text.replace(old, new, 1), True


def find_src(root: Path) -> Path:
    if (root / "esp32_marauder.ino").exists():
        return root
    sub = root / "esp32_marauder"
    if (sub / "esp32_marauder.ino").exists():
        return sub
    raise SystemExit(f"could not find esp32_marauder.ino under {root}")


def main():
    if len(sys.argv) != 2:
        raise SystemExit(__doc__)
    src = find_src(Path(sys.argv[1]).resolve())
    print(f"patching Marauder at {src}")

    # 0. copy the module in; bake the fw-source commit into pwnpal_commit.h (fw=<hash> on
    # PWNPAL_ADV). use the last commit touching pwnpal-marauder/ (not HEAD) so the hash tracks the
    # firmware itself — the app compares it to the same value to nudge a reflash. app-only commits
    # leave it unchanged, so a current board never gets nagged.
    import subprocess
    try:
        commit = subprocess.check_output(
            ["git", "-C", str(HERE), "log", "-1", "--abbrev=7", "--format=%h", "--", ":/pwnpal-marauder"],
            text=True).strip()
    except Exception:
        commit = "nogit"
    if not commit:
        commit = "nogit"
    (HERE / "pwnpal_commit.h").write_text(
        '#pragma once\n#define PWNPAL_FW_COMMIT "%s"\n' % commit)
    print(f"  fw commit: {commit}")

    for name in ("Pwnpal.h", "Pwnpal.cpp", "pwnpal_frames.h", "pwnpal_commit.h"):
        shutil.copy2(HERE / name, src / name)
        print(f"  copied {name}")

    steps = 0

    # 1. esp32_marauder.ino — declare the global object + enlarge the command RX buffer.
    p = src / "esp32_marauder.ino"
    t = _read(p)
    t, done = insert_after(
        t,
        "WiFiScan wifi_scan_obj;",
        '#include "Pwnpal.h"\nPwnpal pwnpal_obj;\n',
        tag="Pwnpal pwnpal_obj;",
    )
    steps += done
    # the pwnpal advertise command grows with the whitelist + target (~370B); the default
    # 256B UART RX buffer overflows mid-WiFi-burst, dropping the trailing '\n' -> -ch/-target
    # lost and the channel never pins. bigger buffer survives the loop stall (cf. Serial2 GPS fix).
    t, dbuf = insert_before(
        t,
        "Serial.begin(115200);",
        "  Serial.setRxBufferSize(1024);  // pwnpal: hold the full advertise command\n",
        tag="Serial.setRxBufferSize(1024)",
    )
    _write(p, t); steps += dbuf

    # 2. WiFiScan.h — scan-mode id + runner declaration.
    p = src / "WiFiScan.h"
    t = _read(p)
    t, d1 = insert_after(
        t, "#define WIFI_SCAN_PWN ", "#define WIFI_SCAN_PWNPAL 111\n",
        tag="WIFI_SCAN_PWNPAL")
    t, d2 = insert_after(
        t, "void RunPwnScan(uint8_t scan_mode, uint16_t color);",
        "    void RunPwnpalScan(uint8_t scan_mode, uint16_t color);\n",
        tag="RunPwnpalScan")
    _write(p, t); steps += d1 + d2

    # 3. WiFiScan.cpp — include, runner, dispatch, sniff-report, main() tick, sniffer guard.
    p = src / "WiFiScan.cpp"
    t = _read(p)

    t, d = insert_after(
        t, '#include "WiFiScan.h"',
        '#include "Pwnpal.h"\nextern Pwnpal pwnpal_obj;\n',
        tag="extern Pwnpal pwnpal_obj;")
    steps += d

    runner = '''void WiFiScan::RunPwnpalScan(uint8_t scan_mode, uint16_t color) {
  (void)scan_mode; (void)color;
  // Real scan start: clear the per-session capture dedup tables here (NOT in
  // configureFromArgs, which the Flipper re-runs every ~15s to refresh the
  // persona) so a persona refresh doesn't re-count pwnd APs / inflate pwnd_tot.
  pwnpal_obj.beginSession();
  startPcap("pwnpal");
  // Mirror Marauder's beacon-attack TX init EXACTLY. The AP config
  // (esp_wifi_set_config) is REQUIRED: without it the AP iface never fully
  // comes up and esp_wifi_80211_tx(WIFI_IF_AP) silently radiates nothing
  // (verified on-air: 0 frames). Promiscuous rx is layered on so the friend
  // still sniffs peers while broadcasting.
  ap_config.ap.ssid_hidden = 1;
  ap_config.ap.beacon_interval = 10000;
  ap_config.ap.ssid_len = 0;
  packets_sent = 0;
  esp_wifi_init(&cfg);
  #ifdef HAS_IDF_3
    esp_wifi_set_country(&country);
  #endif
  esp_wifi_set_storage(WIFI_STORAGE_RAM);
  esp_wifi_set_mode(WIFI_MODE_AP);
  esp_wifi_set_config(WIFI_IF_AP, &ap_config);
  esp_wifi_start();
  this->setMac();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_promiscuous_filter(&filt);
  esp_wifi_set_promiscuous_rx_cb(&beaconSnifferCallback);
  this->changeChannel(this->set_channel);
  this->wifi_initialized = true;
  initTime = millis();
  esp_wifi_set_max_tx_power(pwnpal_obj.saverLevel() ? 40 : 78);  // battery-saver TX cut
}

'''
    t, d = insert_before(
        t, "void WiFiScan::RunPwnScan(uint8_t scan_mode, uint16_t color)",
        runner, tag="RunPwnpalScan(uint8_t")
    steps += d

    t, d = insert_after(
        t,
        "    RunPwnScan(scan_mode, color);",
        "  else if (scan_mode == WIFI_SCAN_PWNPAL)\n"
        "    RunPwnpalScan(scan_mode, color);\n",
        tag="RunPwnpalScan(scan_mode, color);")
    steps += d

    # broadcast on every main() tick in pwnpal mode, skip the rest. also feed the GPS
    # fix status through so the friend can emit PWNPAL_GPS (throttled inside reportGps).
    t, d = insert_after(
        t,
        "void WiFiScan::main(uint32_t currentTime)\n{",
        "  if (currentScanMode == WIFI_SCAN_PWNPAL) {\n"
        "    int pf_sv = pwnpal_obj.saverTick(currentTime);  // deep saver: duty-cycle radio\n"
        "    if (pf_sv == 1) { esp_wifi_set_promiscuous(false); esp_wifi_stop(); }  // doze off\n"
        "    else if (pf_sv == 2) {  // wake: bring the radio back up (no beginSession, keep state)\n"
        "      esp_wifi_start();\n"
        "      esp_wifi_set_promiscuous(true);\n"
        "      esp_wifi_set_promiscuous_filter(&filt);\n"
        "      esp_wifi_set_promiscuous_rx_cb(&beaconSnifferCallback);\n"
        "      this->changeChannel(this->set_channel);\n"
        "      esp_wifi_set_max_tx_power(pwnpal_obj.saverLevel() ? 40 : 78);\n"
        "      initTime = currentTime;\n"
        "    }\n"
        "    if (pf_sv == 1 || pf_sv == 3) return;  // dozing: skip broadcast\n"
        "    if (currentTime - initTime >= 500) {\n"
        "      initTime = millis();\n"
        "      pwnpal_obj.broadcast();\n"
        "      #ifdef HAS_GPS\n"
        "        pwnpal_obj.reportGps(gps_obj.getFixStatus(), gps_obj.getNumSats(),\n"
        "                                gps_obj.getAccuracy(), gps_obj.getLat().c_str(),\n"
        "                                gps_obj.getLon().c_str());\n"
        "      #endif\n"
        "    }\n"
        "    return;\n"
        "  }\n",
        tag="pwnpal_obj.broadcast();")
    steps += d

    # let the sniffer callback process our mode and branch to reportPeer.
    t, d = replace_once(
        t,
        "      (wifi_scan_obj.currentScanMode == WIFI_SCAN_PWN)) {",
        "      (wifi_scan_obj.currentScanMode == WIFI_SCAN_PWN) ||\n"
        "      (wifi_scan_obj.currentScanMode == WIFI_SCAN_PWNPAL)) {",
        tag="WIFI_SCAN_PWNPAL)) {")
    steps += d

    t, d = replace_once(
        t,
        "          wifi_scan_obj.processPwnagotchiBeacon(snifferPacket->payload, len);",
        "          if (wifi_scan_obj.currentScanMode == WIFI_SCAN_PWNPAL) {\n"
        "            #ifdef HAS_GPS\n"
        "              bool pf_fix = gps_obj.getFixStatus() && gps_obj.getNumSats() >= 4;\n"
        "              double pf_lat = pf_fix ? atof(gps_obj.getLat().c_str()) : 0.0;\n"
        "              double pf_lon = pf_fix ? atof(gps_obj.getLon().c_str()) : 0.0;\n"
        "            #else\n"
        "              bool pf_fix = false; double pf_lat = 0.0; double pf_lon = 0.0;\n"
        "            #endif\n"
        "            pwnpal_obj.reportPeer(snifferPacket->payload, len, "
        "snifferPacket->rx_ctrl.rssi, snifferPacket->rx_ctrl.channel, pf_fix, pf_lat, pf_lon);\n"
        "          } else\n"
        "            wifi_scan_obj.processPwnagotchiBeacon(snifferPacket->payload, len);",
        tag="pwnpal_obj.reportPeer(")
    steps += d

    # 3b. capture path: EAPOL/PMKID come as DATA frames; beaconSnifferCallback only handles
    # MGMT, so handle pwnpal DATA first and append EAPOL to the pcap. len here is still
    # rx_ctrl.sig_len (correct for DATA).
    t, d = insert_before(
        t,
        "  if ((wifi_scan_obj.currentScanMode == WIFI_SCAN_PROBE) ||",
        "  if ((wifi_scan_obj.currentScanMode == WIFI_SCAN_PWNPAL) &&\n"
        "      (type == WIFI_PKT_DATA)) {\n"
        "    #ifdef HAS_GPS\n"
        "      bool pf_fix = gps_obj.getFixStatus() && gps_obj.getNumSats() >= 4;\n"
        "      double pf_lat = pf_fix ? atof(gps_obj.getLat().c_str()) : 0.0;\n"
        "      double pf_lon = pf_fix ? atof(gps_obj.getLon().c_str()) : 0.0;\n"
        "    #else\n"
        "      bool pf_fix = false; double pf_lat = 0.0; double pf_lon = 0.0;\n"
        "    #endif\n"
        "    // Harvest the client station from every DATA frame so active mode can\n"
        "    // UNICAST-deauth it (broadcast deauth is ignored by modern clients).\n"
        "    pwnpal_obj.reportClient(snifferPacket->payload, len);\n"
        "    if (pwnpal_obj.reportHandshake(snifferPacket->payload, len,\n"
        "                                      snifferPacket->rx_ctrl.rssi,\n"
        "                                      snifferPacket->rx_ctrl.channel,\n"
        "                                      pf_fix, pf_lat, pf_lon))\n"
        "      buffer_obj.append(snifferPacket, len);\n"
        "    return;\n"
        "  }\n",
        tag="pwnpal_obj.reportHandshake(")
    steps += d

    # 3b2. de-cloak: a client's (re)assoc request (mgmt subtype 0x00/0x20) names a HIDDEN AP in
    # the clear. harvest it -> reportDecloak adopts the ESSID + emits PWNPAL_AP so the capture
    # becomes crackable. runs before the PROBE branch; doesn't return (beacons fall through).
    t, d = insert_before(
        t,
        "  if ((wifi_scan_obj.currentScanMode == WIFI_SCAN_PROBE) ||",
        "  if ((wifi_scan_obj.currentScanMode == WIFI_SCAN_PWNPAL) &&\n"
        "      (type == WIFI_PKT_MGMT) &&\n"
        "      (((snifferPacket->payload[0] & 0xf0) == 0x00) ||\n"
        "       ((snifferPacket->payload[0] & 0xf0) == 0x20))) {\n"
        "    #ifdef HAS_GPS\n"
        "      bool pf_fix = gps_obj.getFixStatus() && gps_obj.getNumSats() >= 4;\n"
        "      double pf_lat = pf_fix ? atof(gps_obj.getLat().c_str()) : 0.0;\n"
        "      double pf_lon = pf_fix ? atof(gps_obj.getLon().c_str()) : 0.0;\n"
        "    #else\n"
        "      bool pf_fix = false; double pf_lat = 0.0; double pf_lon = 0.0;\n"
        "    #endif\n"
        "    pwnpal_obj.reportDecloak(snifferPacket->payload, len - 4,\n"
        "                                snifferPacket->rx_ctrl.rssi,\n"
        "                                snifferPacket->rx_ctrl.channel,\n"
        "                                pf_fix, pf_lat, pf_lon);\n"
        "  }\n",
        tag="pwnpal_obj.reportDecloak(")
    steps += d

    # 3c. recon: dedup non-pwngrid beacons into PWNPAL_AP lines. sits inside
    # if(type==MGMT)->if(payload[0]==0x80) after the pwngrid mac_match return, so peers
    # never reach it. len here is FCS-stripped (-4).
    t, d = insert_before(
        t,
        "        if (wifi_scan_obj.currentScanMode == WIFI_SCAN_PWN) {",
        "        if (wifi_scan_obj.currentScanMode == WIFI_SCAN_PWNPAL) {\n"
        "          #ifdef HAS_GPS\n"
        "            bool pf_fix = gps_obj.getFixStatus() && gps_obj.getNumSats() >= 4;\n"
        "            double pf_lat = pf_fix ? atof(gps_obj.getLat().c_str()) : 0.0;\n"
        "            double pf_lon = pf_fix ? atof(gps_obj.getLon().c_str()) : 0.0;\n"
        "          #else\n"
        "            bool pf_fix = false; double pf_lat = 0.0; double pf_lon = 0.0;\n"
        "          #endif\n"
        "          if (pwnpal_obj.reportAP(snifferPacket->payload, len,\n"
        "                                     snifferPacket->rx_ctrl.rssi,\n"
        "                                     snifferPacket->rx_ctrl.channel,\n"
        "                                     pf_fix, pf_lat, pf_lon))\n"
        "            buffer_obj.append(snifferPacket, len);\n"
        "          return;\n"
        "        }\n",
        tag="pwnpal_obj.reportAP(")
    steps += d
    _write(p, t)

    # 3d. GPS: drop the per-boot $PSTMSRR reset in GpsInterface::begin(). resetting the
    # Teseo GNSS engine on every power-up throws away a backup-powered hot start and slows
    # TTFF; the constellation mask still persists via SAVEPAR, so the reset is redundant.
    # tolerant: some Marauder bases don't send PSTM commands at all.
    p = src / "GpsInterface.cpp"
    if p.exists():
        t = _read(p)
        try:
            t, d = replace_once(
                t,
                '  MicroNMEA::sendSentence(Serial2, "$PSTMSRR");',
                "  // pwnpal: no per-boot $PSTMSRR reset -- restarting the GNSS engine every\n"
                "  // boot discards a backup-powered hot start and slows TTFF (Teseo only; a\n"
                "  // reset is only needed to *change* the mask, which SAVEPAR already persisted).",
                tag="pwnpal: no per-boot $PSTMSRR")
            _write(p, t); steps += d
        except AnchorError:
            print("  note: $PSTMSRR absent in GpsInterface.cpp; skipped GPS-reset patch")

        # the FEBERIS module is a CASIC 'URANUS' (AT6558-class), which ignores $PSTM and
        # defaults to GPS+BeiDou. enable GLONASS too (mask 7 = GPS+BDS+GLONASS) for more
        # birds / faster locks. runtime only (no $PCAS00 save) so it reverts on power-cycle.
        t = _read(p)
        try:
            t, d = replace_once(
                t,
                '  MicroNMEA::sendSentence(Serial2, "$PSTMSAVEPAR");',
                '  MicroNMEA::sendSentence(Serial2, "$PSTMSAVEPAR");\n'
                '  MicroNMEA::sendSentence(Serial2, "$PCAS04,7");  // pwnpal: GPS+BDS+GLONASS (CASIC)',
                tag='"$PCAS04,7"')
            _write(p, t); steps += d
        except AnchorError:
            print("  note: $PSTMSAVEPAR absent; skipped CASIC constellation enable")

        # GPS UART: enlarge the RX ring. the default 256B overflows in ~22ms @115200, and
        # broadcast()'s per-channel delay(1) sweep + attack bursts block the loop longer than
        # that -> corrupted NMEA -> the fix drops while scanning/walking. 4KB buffers ~350ms.
        t = _read(p)
        try:
            t, d = replace_once(
                t,
                "  Serial2.begin(9600, SERIAL_8N1, GPS_TX, GPS_RX);",
                "  Serial2.setRxBufferSize(4096);  // pwnpal: survive loop stalls during WiFi bursts\n"
                "  Serial2.begin(9600, SERIAL_8N1, GPS_TX, GPS_RX);",
                tag="Serial2.setRxBufferSize(4096)")
            _write(p, t); steps += d
        except AnchorError:
            print("  note: Serial2.begin(9600 ...) not found; skipped GPS RX-buffer bump")

    # 4. CommandLine.h — the command string.
    p = src / "CommandLine.h"
    t = _read(p)
    t, d = insert_after(
        t, 'const char PROGMEM SNIFF_PWN_CMD[] = "sniffpwn";',
        'const char PROGMEM PWNPAL_CMD[] = "pwnpal";\n',
        tag="PWNPAL_CMD")
    _write(p, t); steps += d

    # 5. CommandLine.cpp — include + command handler.
    p = src / "CommandLine.cpp"
    t = _read(p)
    t, d1 = insert_after(
        t, '#include "CommandLine.h"',
        '#include "Pwnpal.h"\nextern Pwnpal pwnpal_obj;\n',
        tag="extern Pwnpal pwnpal_obj;")
    handler = '''    else if (cmd_args.get(0) == PWNPAL_CMD) {
      if (!pwnpal_obj.configureFromArgs(&cmd_args)) {
        Serial.println(F("PWNPAL_ERR bad -id (need 64 hex)"));
      } else if (wifi_scan_obj.currentScanMode == WIFI_SCAN_PWNPAL) {
        // Already running: configureFromArgs() already refreshed + rebuilt the
        // persona. Do NOT StartScan again -- that would tear down/re-init WiFi,
        // wipe recon, and reset the channel hop. The ~15s command re-send is
        // only a live persona update.
        Serial.println(F("pwnpal persona updated"));
      } else {
        Serial.print(F("Starting pwnpal. Stop with "));
        Serial.println(STOPSCAN_CMD);
        wifi_scan_obj.StartScan(WIFI_SCAN_PWNPAL, TFT_MAGENTA);
      }
    }
'''
    # Insert BEFORE the `if (!wifi_scan_obj.scanning())` guard so the handler is always processed,
    # even while a scan is running -- otherwise re-advertises (live persona/-ch/-target updates) are
    # dropped once WIFI_SCAN_PWNPAL starts, and only the launch command ever configures the ESP.
    t, d2 = insert_before(
        t, "  if (!wifi_scan_obj.scanning()) {",
        handler, tag="cmd_args.get(0) == PWNPAL_CMD")
    _write(p, t); steps += d1 + d2

    print(f"done ({steps} insertion(s) applied; already-applied steps skipped)")


if __name__ == "__main__":
    main()
