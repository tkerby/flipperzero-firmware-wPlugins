#!/usr/bin/env python3
"""Decode a binary battle log (.pbl) written by the Flipper app.

Usage: decode_log.py LOG.pbl [--party gb|flipper|both]

Prints every record; the parties exchanged in the log are decoded with
decode_party.py (the GB's party from the received block and patch list).
"""
import argparse
import struct
import sys

import decode_party

NAMES = {
    1: "rn_tx",
    2: "rn_rx",
    3: "party_tx",
    4: "party_rx",
    5: "patch_tx",
    6: "patch_rx",
    7: "exchange",
    8: "battle_end",
    9: "note",
}
RESULTS = {0: "unknown", 1: "win", 2: "lose", 3: "draw"}


def read_records(data):
    if data[:4] != b"PBL1":
        raise SystemExit("not a PBL1 log")
    version, flags, _, when = struct.unpack_from("<BBHI", data, 4)
    records = []
    off = 12
    while off + 3 <= len(data):
        typ, length = data[off], struct.unpack_from("<H", data, off + 1)[0]
        payload = data[off + 3 : off + 3 + length]
        if len(payload) < length:
            raise SystemExit(f"truncated record at offset {off}")
        records.append((typ, payload))
        off += 3 + length
    return version, when, records


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("--party", choices=("gb", "flipper", "both"), default="both")
    args = ap.parse_args()
    with open(args.log, "rb") as f:
        version, when, records = read_records(f.read())
    print(f"PBL version {version}, time {when}, {len(records)} records")
    blobs = {}
    for typ, payload in records:
        name = NAMES.get(typ, f"type{typ}")
        if typ == 7:
            idx, mine, gb, crc = struct.unpack("<HBBI", payload)
            print(
                f"exchange {idx:3d}: flipper ${0x80 | mine:02X}  gb ${0x80 | gb:02X}  crc {crc:08X}"
            )
        elif typ == 8:
            result, nbytes = struct.unpack("<BI", payload)
            print(f"battle end: {RESULTS.get(result, result)}, {nbytes} link bytes")
        elif typ == 9:
            print("note:", payload.decode("ascii", "replace"))
        else:
            blobs[name] = payload
            print(f"{name}: {len(payload)} bytes")
    sides = {"gb": ("party_rx", "patch_rx"), "flipper": ("party_tx", "patch_tx")}
    for side, (pk, pt) in sides.items():
        if args.party in (side, "both") and pk in blobs and pt in blobs:
            print(f"--- {side} party ---")
            sys.stdout.write(
                decode_party.decode(decode_party.unpack(blobs[pk], blobs[pt]))
            )


if __name__ == "__main__":
    main()
