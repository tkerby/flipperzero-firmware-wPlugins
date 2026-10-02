#!/usr/bin/env python3
"""Writes twentyq.png: 10x10, every pixel pure black or fully transparent,
as the app catalog requires (SPEC 7.2). Pure stdlib -- no Pillow.

The glyph is a question mark inside a border.
"""
import struct
import sys
import zlib
from pathlib import Path

GLYPH = [
    "..........",
    ".########.",
    ".#..##..#.",
    ".#.#..#.#.",
    ".#....#.#.",
    ".#...##.#.",
    ".#...#..#.",
    ".#......#.",
    ".#...#..#.",
    ".########.",
]


def chunk(tag, data):
    return (struct.pack(">I", len(data)) + tag + data
            + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF))


def main(out):
    raw = bytearray()
    for row in GLYPH:
        raw.append(0)  # filter: none
        for c in row:
            raw += b"\x00\x00\x00\xff" if c == "#" else b"\x00\x00\x00\x00"
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", 10, 10, 8, 6, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(raw), 9))
           + chunk(b"IEND", b""))
    Path(out).write_bytes(png)
    print(f"wrote {out} ({len(png)} bytes)")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "twentyq.png")
