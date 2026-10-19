#!/usr/bin/env python3
"""Validate the pwnpal beacon against what a real pwnagotchi/Marauder accepts.

We can't emulate the WiFi radio, but the whole trick is the *frame*, and that we
can check exactly. This mirrors the frame Pwnpal.cpp::rebuild() builds and
asserts every invariant the receivers actually enforce:

  * pwngrid (evilsocket/pwngrid mesh/peer.go): the advertisement JSON must parse,
    carry an `identity` matching ^[a-fA-F0-9]{64}$, and needs NO public key /
    signature to be accepted as a peer.
  * ESP32 Marauder (WiFiScan.cpp processPwnagotchiBeacon): src MAC must be
    de:ad:be:ef:de:ad, subtype must be beacon (0x80); it finds the JSON by
    scanning from frame offset 36 for '{' and needs keys `name` and `pwnd_tot`.

Run: python3 tools/validate_beacon.py
"""

import json
import re
import sys

SIG_MAC = bytes([0xDE, 0xAD, 0xBE, 0xEF, 0xDE, 0xAD])
IE_WHISPER_PAYLOAD = 0xDE  # 222


def build_json(name, identity, face, pwnd_run, pwnd_tot, uptime, deauth=False):
    """Mirror of Pwnpal.cpp::buildJson (same field order/shape). Compact so it
    fits in a single vendor IE (<=255 bytes)."""
    return (
        '{"name":"%s","identity":"%s","version":"1.0.0",'
        '"face":"%s","pwnd_run":%d,"pwnd_tot":%d,"uptime":%d,'
        '"policy":{"deauth":%s}}'
    ) % (name, identity, face, pwnd_run, pwnd_tot, uptime,
         "true" if deauth else "false")


def build_frame(payload_json, session_id):
    """Mirror of Pwnpal.cpp::rebuild(): 38-byte header + IE 222 + JSON."""
    header = bytes([
        0x80, 0x00,                          # frame control: mgmt / beacon
        0x00, 0x00,                          # duration
        0xff, 0xff, 0xff, 0xff, 0xff, 0xff,  # Addr1 dst: broadcast
        0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,  # Addr2 src: pwngrid signature
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  # Addr3 bssid: session id (patched)
        0x00, 0x00,                          # seq-ctl
        0, 0, 0, 0, 0, 0, 0, 0,              # timestamp
        0x64, 0x00,                          # beacon interval
        0x00, 0x00,                          # capability info
    ])
    frame = bytearray(header)
    frame[16:22] = session_id  # Addr3
    jb = payload_json.encode("utf-8")
    assert len(jb) <= 255, "JSON exceeds a single IE (255 bytes)"
    frame.append(IE_WHISPER_PAYLOAD)
    frame.append(len(jb))
    frame.extend(jb)
    return bytes(frame)


FING_RE = re.compile(r"^[a-fA-F0-9]{64}$")


def check(cond, label):
    print(("  PASS  " if cond else "  FAIL  ") + label)
    return cond


def main():
    # A representative persona the Flipper would push.
    identity = "3b1e9f2a" * 8  # 64 hex
    session_id = bytes(int(identity[i * 2:i * 2 + 2], 16) for i in range(6))
    js = build_json(
        name="lonelybot", identity=identity, face="(♥‿‿♥)",
        pwnd_run=2, pwnd_tot=7, uptime=3600)
    frame = build_frame(js, session_id)

    # Worst case: 16-char name + big counters must still fit one IE.
    js_max = build_json(
        name="x" * 16, identity=identity, face="(♥‿‿♥)",
        pwnd_run=999999, pwnd_tot=999999, uptime=9999999)

    print("advertised JSON (%d bytes):" % len(js.encode()))
    print("  " + js)
    print("frame (%d bytes): %s..." % (len(frame), frame[:20].hex()))
    print()

    ok = True

    print("single-IE fit (<=255 bytes, so Marauder's contiguous scan works):")
    ok &= check(len(js.encode()) <= 255,
                "sample JSON is %d bytes" % len(js.encode()))
    ok &= check(len(js_max.encode()) <= 255,
                "worst-case JSON (16-char name) is %d bytes" % len(js_max.encode()))
    print()

    print("pwngrid (mesh/peer.go) acceptance:")
    try:
        doc = json.loads(js)
        ok &= check(True, "advertisement is valid JSON")
    except Exception as e:  # noqa
        ok &= check(False, "advertisement is valid JSON (%s)" % e)
        doc = {}
    ok &= check("identity" in doc, "carries an identity")
    ok &= check(bool(FING_RE.match(doc.get("identity", ""))),
                "identity matches ^[a-fA-F0-9]{64}$")
    ok &= check("public_key" not in doc,
                "no public key required (pwngrid logs debug, still accepts)")
    print()

    print("ESP32 Marauder (processPwnagotchiBeacon) detection:")
    ok &= check(frame[0] == 0x80, "subtype is beacon (payload[0]==0x80)")
    ok &= check(frame[10:16] == SIG_MAC, "src MAC == de:ad:be:ef:de:ad")
    # Marauder scans from offset 36 for '{' and back for '}'.
    start = 36
    while start < len(frame) and frame[start] != ord('{'):
        start += 1
    end = len(frame)
    while end > start and frame[end - 1] != ord('}'):
        end -= 1
    ok &= check(start < end, "JSON locatable by scan from offset 36")
    ok &= check(frame[38] == ord('{'), "JSON begins right after IE 222 header")
    try:
        scanned = json.loads(frame[start:end].decode("utf-8"))
    except Exception:  # noqa
        scanned = {}
    ok &= check("name" in scanned, "has key 'name'")
    ok &= check("pwnd_tot" in scanned, "has key 'pwnd_tot'")
    print()

    print("pwnagotchi Peer (mesh/peer.py) render fields present:")
    for k in ("name", "identity", "face", "pwnd_run", "pwnd_tot"):
        ok &= check(k in doc, "adv['%s']" % k)

    print()
    print("RESULT:", "ALL CHECKS PASSED" if ok else "FAILURES ABOVE")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
