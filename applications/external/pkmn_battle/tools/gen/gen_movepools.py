#!/usr/bin/env python3
"""Generate the legal move pools (files/movepools.bin). Rules live in legality.py.

movepools.bin layout (little endian):
  0    char[4]  "PKMP"
  4    u8       format version (3)
  5    u8       number of species (NUM_POKEMON = 251)
  6    u16      reserved (0)
  8    u16      offsets[NUM_POKEMON + 1]; species s (1-based) owns bytes
                [offsets[s - 1], offsets[s]) of the file
  512  records, one per species:
         u8     min_level: lowest level at which the species can exist
         u8     evolve_level: level by which it would normally have evolved
                (0 = fully evolved; legality.evolve_level)
         (u8 move, u8 level)[]: every legal move with the earliest level at
                which the species can know it, ascending by move id

Species eligibility is decided on the device per level (min_level <= level
and at least 4 moves legal at that level).

Usage: python tools/gen/gen_movepools.py [--check]
"""
import argparse
import struct
import sys

import gen_tables
import legality

POOL_FILE = gen_tables.REPO / "files" / "movepools.bin"
MAGIC = b"PKMP"
VERSION = 3


def build_pools(data):
    n = data.num_pokemon
    header_len = 8 + 2 * (n + 1)
    body, offs = bytearray(), []
    for sid in range(1, n + 1):
        offs.append(header_len + len(body))
        body += bytes((data.min_level(sid), data.evolve_level(sid)))
        for mv, lvl in sorted(data.move_levels(sid).items()):
            assert 1 <= lvl <= 100, (sid, mv, lvl)
            body += bytes((mv, lvl))
    offs.append(header_len + len(body))
    assert offs[-1] <= 0xFFFF
    return (
        MAGIC
        + struct.pack("<BBH", VERSION, n, 0)
        + struct.pack(f"<{n + 1}H", *offs)
        + body
    )


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="fail if outputs are stale")
    args = ap.parse_args()

    data = gen_tables.load()
    legality.check_expected_exclusions(data)

    stale = []
    blob = build_pools(data)
    old = POOL_FILE.read_bytes() if POOL_FILE.exists() else None
    if old != blob:
        stale.append(POOL_FILE.name)
        if not args.check:
            POOL_FILE.parent.mkdir(parents=True, exist_ok=True)
            POOL_FILE.write_bytes(blob)
    if args.check and stale:
        sys.exit(f"stale generated files: {stale}")
    print(
        f"movepools.bin {len(blob)} bytes;", "stale:" if stale else "no changes", *stale
    )


if __name__ == "__main__":
    main()
