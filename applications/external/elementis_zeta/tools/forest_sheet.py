#!/usr/bin/env python3
"""Forest-light before/after sheet: LCD-look pieces side by side. Usage: forest_sheet.py <before_tiles_dir> <after_tiles_dir> <out.png>"""
import sys, os
sys.path.insert(0, os.path.dirname(__file__))
from PIL import Image
import lvlib

NAMES = ["pine_dark_a", "pine_dark_b", "pine_dark_c", "pine_a", "pine_b", "void_dark", "litter_a", "litter_c", "trail_a"]


def load(d, n):
    im = Image.open(os.path.join(d, n + ".lv.png")).convert("RGBA")
    W, H = im.size
    lv = [[0] * W for _ in range(H)]
    mk = [[True] * W for _ in range(H)]
    p = im.load()
    for y in range(H):
        for x in range(W):
            r, g, b, a = p[x, y]
            lv[y][x] = 3 - r // 85
            mk[y][x] = a > 0
    return lv, mk


def main(before, after, out):
    rows = []
    for d in (before, after):
        tiles = []
        for n in NAMES:
            lv, mk = load(d, n)
            tiles.append(lvlib.lcd_image(lv, mk, scale=6, ground=1 if n.startswith("pine") else None))
        rows.append(tiles)
    pad = 8
    wmax = [max(rows[0][i].width, rows[1][i].width) for i in range(len(NAMES))]
    hmax = max(t.height for r in rows for t in r)
    W = sum(wmax) + pad * (len(NAMES) + 1)
    sheet = Image.new("RGB", (W, hmax * 2 + pad * 3), (60, 60, 60))
    for ri, r in enumerate(rows):
        x = pad
        for i, t in enumerate(r):
            sheet.paste(t, (x, pad + ri * (hmax + pad)))
            x += wmax[i] + pad
    sheet.save(out)


main(*sys.argv[1:4])
