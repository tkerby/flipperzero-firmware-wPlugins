#!/usr/bin/env python3
"""Title screen art ("Elementis Zeta", contract section 21): frozen art in assets/art/title/ -> nucleo/files/title.bin.

  python3 -I tools/titlec.py                    build title.bin from the frozen art (deterministic, no fonts needed)
  python3 -I tools/titlec.py check              verify title.bin is up to date (exit 1 otherwise)
  python3 -I tools/titlec.py --refresh-logo     re-render the logo + ZETA with tools/logo_px.py (needs the macOS fonts) into
                                                assets/art/title/{logo,zeta}.lv.png, then build
  python3 -I tools/titlec.py --refresh-word     re-render ONLY the second word (ZETA) into assets/art/title/zeta.lv.png (the logo and the
                                                Cristal stay frozen: contract 58, the rename from ZERO), then build
  python3 -I tools/titlec.py --refresh-crystal  re-render the 12 Cristal frames with tools/crystal3d.py into
                                                assets/art/title/crystal_NN.lv.png, then build

The art (approved mockup "B, Emblema", tools/title_mock.py scene_b):
  logo.lv.png     logo_px.word("ELEMENTIS", "arialblack", 11, track=1)
  zeta.lv.png     logo_px.word("ZETA", "arialblack", 8, track=1)
  crystal_NN.lv.png (NN = 00..11)  crystal3d.frames(42, 12): the spinning Cristal, 60 degrees in 12 frames = a seamless loop
All of it is frozen (the build never depends on a font or on Pillow's polygon rasteriser); only the --refresh options re-render.

title.bin (little-endian). The file IS the heap block the scene builds: the first coff bytes (header + logo + ZETA) are read into
memory as they are, then one padded crystal record at a time is read after them. Pixel formats of contract section 2 inside a record.
  header, 16 bytes:  "NTTL", u8 version = 1, u8 n = crystal frames (12), u16 zoff (offset of the ZETA record), u16 coff (offset of the
                     first crystal record = the size of header + logo + ZETA), u16 clen (size of one crystal record), u8 hero_x,
                     u8 hero_y (top-left of the hero's south-west picture on the screen, see below), u16 0
  record:            u8 w, u8 h, 2 bpp pixels (4 px per byte, row stride (w+3)/4), then a 1 bpp LSB-first mask (stride (w+7)/8)
  at 16              the logo record, at zoff the ZETA record, at coff + k * clen crystal frame k (every crystal record is padded to
                     clen = the largest, so frame k is one fixed-size read)
hero_x / hero_y: the hero is not in this file (nucleo/files/hero.bin); titlec reads its south-east frame 0 and bakes the place where the
mirrored picture (facing south-west) has its feet on the last row (y 63) and its right-most opaque column on x 127, so the scene
needs no scan. `titlec.py check` fails when hero.bin changed since.
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
import lvlib  # noqa: E402

ART = os.path.join(ROOT, "assets", "art", "title")
OUT = os.path.join(ROOT, "nucleo", "files", "title.bin")
HERO = os.path.join(ROOT, "nucleo", "files", "hero.bin")
N_CRYSTAL = 12
CRYSTAL_H = 42
LOGO = ("ELEMENTIS", 11)
ZETA = ("ZETA", 8)


def png(name):
    return os.path.join(ART, name + ".lv.png")


def refresh_logo(only=None):
    import logo_px
    for name, (text, cap) in (("logo", LOGO), ("zeta", ZETA)):
        if only and name != only:
            continue
        lv, mk = logo_px.word(text, "arialblack", cap, track=1)
        lvlib.write_lv_png(png(name), lv, mk, 0, len(lv) - 1, force=True)
        print("refreshed", png(name), len(lv[0]), "x", len(lv))


def refresh_crystal():
    import crystal3d
    for k, (lv, mk) in enumerate(crystal3d.frames(CRYSTAL_H, N_CRYSTAL)):
        lvlib.write_lv_png(png("crystal_%02d" % k), lv, mk, 0, len(lv) - 1, force=True)
    print("refreshed", N_CRYSTAL, "crystal frames")


def load(name):
    lv, mk, meta = lvlib.read_lv_png(png(name))
    return lv, mk, meta["w"], meta["h"]


def record(name):
    lv, mk, w, h = load(name)
    if w > 255 or h > 255:
        raise SystemExit("titlec: sprite too big")
    return bytes((w, h)) + lvlib.pack_2bpp(lv, 0, 0, w - 1, h - 1) + lvlib.pack_mask(mk, 0, 0, w - 1, h - 1)


def hero_place():
    """Top-left of the hero's mirrored south-east frame 0 (= south-west): feet on row 63, right-most opaque column on x 127."""
    d = open(HERO, "rb").read()
    w, h, bpr, mbpr, frames, dirs = d[5:11]
    if d[:4] != b"NHRO" or d[4] != 1 or dirs < 2:
        raise SystemExit("titlec: unexpected hero.bin")
    off = 16 + 1 * frames * (bpr * h + mbpr * h) + frames * bpr * h  # direction 1 (south-east); its masks start after its pixels
    rows = [int.from_bytes(d[off + y * mbpr:off + (y + 1) * mbpr], "little") for y in range(h)]
    bottom = max(y for y, r in enumerate(rows) if r)
    cols = 0
    for r in rows:
        cols |= r
    left = (cols & -cols).bit_length() - 1  # left-most opaque source column = the right-most one once mirrored
    return 128 - w + left, 63 - bottom


def build():
    logo, zeta = record("logo"), record("zeta")
    crys = [record("crystal_%02d" % k) for k in range(N_CRYSTAL)]
    clen = max(len(c) for c in crys)
    zoff = 16 + len(logo)
    coff = zoff + len(zeta)
    hx, hy = hero_place()
    head = b"NTTL" + struct.pack("<BBHHHBBH", 1, N_CRYSTAL, zoff, coff, clen, hx, hy, 0)
    assert len(head) == 16
    return head + logo + zeta + b"".join(c.ljust(clen, b"\0") for c in crys)


def main():
    args = sys.argv[1:]
    if "--refresh-logo" in args:
        refresh_logo()
    if "--refresh-word" in args:
        refresh_logo("zeta")
    if "--refresh-crystal" in args:
        refresh_crystal()
    blob = build()
    if "check" in args:
        cur = open(OUT, "rb").read() if os.path.exists(OUT) else b""
        if cur != blob:
            print("titlec: title.bin is out of date (run tools/titlec.py)")
            return 1
        print("titlec: title.bin OK (%d bytes)" % len(blob))
        return 0
    os.makedirs(os.path.dirname(OUT), exist_ok=True)
    with open(OUT, "wb") as f:
        f.write(blob)
    print("title.bin: %d bytes, %d crystal frames" % (len(blob), N_CRYSTAL))
    return 0


if __name__ == "__main__":
    sys.exit(main())
