#!/usr/bin/env python3
"""Title logo in 4 grays at the final pixel size: a bold typeface rasterized at the target cap height
(8x supersampled, 50% coverage), then the logo treatment: white top / light-gray bottom fill (bevel),
1 px black outline and a dark extrusion down-right (the 3D block look of a logo).

  python3 -I tools/logo_px.py <out.png> -> comparison sheet of typefaces / sizes
Library: word(text, font, cap, ...) -> (levels, mask)
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image, ImageDraw, ImageFont  # noqa: E402

SUP = "/System/Library/Fonts/Supplemental/"
FONTS = {
    "arialblack": (SUP + "Arial Black.ttf", 0),
    "impact": (SUP + "Impact.ttf", 0),
    "dincond": (SUP + "DIN Condensed Bold.ttf", 0),
    "dinalt": (SUP + "DIN Alternate Bold.ttf", 0),
    "futura": (SUP + "Futura.ttc", 4),
    "rockwell": (SUP + "Rockwell.ttc", 2),
    "copperplate": (SUP + "Copperplate.ttc", 2),
}
SS = 8


def raster(text, font, cap, track=1):
    """binary mask (rows of 0/1) of `text` with capital height `cap` px; track = extra px between letters."""
    path, idx = FONTS[font]
    # find the point size whose 'E' is cap*SS tall
    size = cap * SS
    for _ in range(6):
        f = ImageFont.truetype(path, size, index=idx)
        bb = f.getbbox("E")
        h = bb[3] - bb[1]
        size = max(4, round(size * cap * SS / h))
    f = ImageFont.truetype(path, size, index=idx)
    bb = f.getbbox("E")
    top = bb[1]
    # letter by letter so the tracking is in whole final pixels
    cols = []
    x = 0
    glyphs = []
    for ch in text:
        if ch == " ":
            x += cap // 2
            continue
        gb = f.getbbox(ch)
        gw = gb[2] - gb[0]
        im = Image.new("L", (gw + SS, cap * SS + SS), 0)
        ImageDraw.Draw(im).text((-gb[0], -top), ch, font=f, fill=255)
        w = (gw + SS - 1) // SS
        H = cap
        px = im.load()
        g = [[0] * w for _ in range(H)]
        for yy in range(H):
            for xx in range(w):
                s = 0
                for a in range(SS):
                    for b in range(SS):
                        X, Y = xx * SS + b, yy * SS + a
                        if X < im.width and Y < im.height:
                            s += px[X, Y]
                g[yy][xx] = 1 if s >= 255 * SS * SS * 0.5 else 0
        glyphs.append((x, g))
        x += w + track
    W = x - track
    out = [[0] * W for _ in range(cap)]
    for gx, g in glyphs:
        for yy in range(cap):
            for xx in range(len(g[0])):
                if g[yy][xx] and gx + xx < W:
                    out[yy][gx + xx] = 1
    return out


def treat(m, depth=2, fill_split=0.5, mid=1, ext=2):
    """mask -> (levels, mask) with fill (white top, `mid` bottom), outline 3, extrusion `ext` down-right."""
    H, W = len(m), len(m[0])
    P = 1 + depth
    h, w = H + 2 * P, W + 2 * P
    lv = [[0] * w for _ in range(h)]
    mk = [[0] * w for _ in range(h)]
    inside = [[0] * w for _ in range(h)]
    for y in range(H):
        for x in range(W):
            if m[y][x]:
                inside[y + 1][x + 1] = 1
    split = round(H * fill_split)
    for y in range(h):
        for x in range(w):
            if inside[y][x]:
                mk[y][x] = 1
                lv[y][x] = 0 if (y - 1) < split else mid
    # outline (8-neighbour so the corners close)
    ol = [[0] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            if inside[y][x]:
                continue
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    yy, xx = y + dy, x + dx
                    if 0 <= yy < h and 0 <= xx < w and inside[yy][xx]:
                        ol[y][x] = 1
    for y in range(h):
        for x in range(w):
            if ol[y][x]:
                mk[y][x] = 1
                lv[y][x] = 3
    # extrusion: the outlined shape shifted down-right, behind
    solid = [[1 if mk[y][x] else 0 for x in range(w)] for y in range(h)]
    for d in range(1, depth + 1):
        for y in range(h):
            for x in range(w):
                yy, xx = y - d, x - d
                if 0 <= yy < h and 0 <= xx < w and solid[yy][xx] and not mk[y][x]:
                    mk[y][x] = 1
                    lv[y][x] = ext if d < depth else 3
    return lv, mk


def word(text, font, cap, track=1, **kw):
    return treat(raster(text, font, cap, track), **kw)


def main():
    import lvlib
    out = sys.argv[1]
    items = []
    for fn in FONTS:
        for cap in (11, 13):
            items.append(("%s %d" % (fn, cap), word("ELEMENTIS", fn, cap, track=1)))
    scale = 4
    W = 128
    sheet = Image.new("RGB", (W * scale * 2 + 30, (len(items) // 2) * 26 * scale + 20), (40, 40, 40))
    for k, (name, (lv, mk)) in enumerate(items):
        field = [[0] * W for _ in range(24)]
        ox = max(0, (W - len(lv[0])) // 2)
        for y in range(len(lv)):
            for x in range(len(lv[0])):
                if mk[y][x] and x + ox < W and y + 2 < 24:
                    field[y + 2][x + ox] = lv[y][x]
        img = lvlib.lcd_image(field, None, scale=scale, grid=False)
        sheet.paste(img, ((k % 2) * (W * scale + 10) + 5, (k // 2) * 26 * scale + 5))
        print("%-16s w=%d" % (name, len(lv[0])))
    sheet.save(out)


if __name__ == "__main__":
    main()
