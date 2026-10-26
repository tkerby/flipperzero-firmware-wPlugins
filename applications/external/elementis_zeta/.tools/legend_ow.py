#!/usr/bin/env python3
"""Overworld sprites of the legendary birds (contract 52): the creature art (assets/art/elementis/<species>_c, 80 px pro, 8 facings) scaled down to a map actor.

  python3 tools/legend_ow.py [build|check|sheet] [species ...]

build (default) writes assets/art/npcs/<species>_ow/<facing>.png (south, south-east, east, north-east, north: the catalog's `mirror_west = true` folds the three west facings)
       from assets/art/elementis/<species>_c/<facing>.png. Every facing of a species is scaled by the SAME factor (the tallest facing's box becomes TARGET_H rows), cut at alpha >= 128
       and padded to one canvas, so the bird does not change size when it turns. No PixelLab, no generated art: a premultiplied-alpha resample of the picture that already exists.
check exits 1 when a file is missing or older than its source (the build step of tools/build_assets.sh runs it).
sheet writes build/legend/ow_sheet.png: every bird, every facing, converted with the real catalog entry (tools/spritec.py), on the light floors of tools/c7_mock.py (ice, metal, rock).
The typecast of the catalog entries (assets/catalog.toml): the LIGHT one for all three (dark_cut 25 and adaptive_split [0.35, 0.75] on every facing, as the species' pictures:
species.toml art_dark_cut = 25, art_split = [0.35, 0.75]); the default one turns Aurowl's navy wing feathers black (a hole in the ice at 36 rows).
"""
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image  # noqa: E402

SPECIES = [
    "aurowl",
    "fulgurion",
    "ignicondor",
    "arkegen",
    "genesa",
]  # C9 (contract 56): + Arkegen, the Cueva Secreta's legend; D1 (contract 68): + Genesa, the Ruinas del Origen's one chance
FACINGS = ["south", "south-east", "east", "north-east", "north"]
MTIME_SLACK = 60  # seconds: a fresh clone / CI checkout writes source and derived files in arbitrary order within a few seconds
TARGET_H = 36  # rows of the tallest facing (the hero is ~32 rows, a tile is 28 x 14)
SRC = os.path.join(ROOT, "assets", "art", "elementis")
DST = os.path.join(ROOT, "assets", "art", "npcs")


def bbox(im):
    return im.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()


def premul_resize(im, w, h):
    """RGBA -> RGBA at w x h: the colours are resampled weighted by alpha, so the transparent border does not bleed black into the edge."""
    r, g, b, a = im.split()
    ch = []
    for c in (r, g, b):
        pm = Image.composite(c, Image.new("L", im.size, 0), a)
        ch.append(pm.resize((w, h), Image.LANCZOS))
    a2 = a.resize((w, h), Image.LANCZOS)
    out = Image.new("RGBA", (w, h))
    px, pa = out.load(), a2.load()
    cp = [c.load() for c in ch]
    for y in range(h):
        for x in range(w):
            al = pa[x, y]
            if al < 128:
                px[x, y] = (0, 0, 0, 0)
                continue
            f = 255.0 / al
            px[x, y] = tuple(
                min(255, int(round(cp[k][x, y] * f))) for k in range(3)
            ) + (255,)
    return out


def build(species):
    files = {}
    for f in FACINGS:
        im = Image.open(os.path.join(SRC, species + "_c", f + ".png")).convert("RGBA")
        files[f] = im.crop(bbox(im))
    top = max(i.size[1] for i in files.values())
    k = TARGET_H / top
    out = {
        f: premul_resize(i, max(1, round(i.size[0] * k)), max(1, round(i.size[1] * k)))
        for f, i in files.items()
    }
    cw = max(i.size[0] for i in out.values())
    ch = max(i.size[1] for i in out.values())
    d = os.path.join(DST, species + "_ow")
    os.makedirs(d, exist_ok=True)
    for f, i in out.items():
        c = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        c.alpha_composite(
            i, ((cw - i.size[0]) // 2, ch - i.size[1])
        )  # centred, standing on the bottom row
        c.save(os.path.join(d, f + ".png"))
    return cw, ch


def check(species):
    bad = []
    for f in FACINGS:
        s = os.path.join(SRC, species + "_c", f + ".png")
        d = os.path.join(DST, species + "_ow", f + ".png")
        if not os.path.exists(d):
            bad.append(d + " missing")
        elif os.path.getmtime(d) + MTIME_SLACK < os.path.getmtime(s):
            bad.append(d + " older than its source")
    return bad


def sheet(names):
    import spritec

    cat = spritec.load_catalog(os.path.join(ROOT, "assets", "catalog.toml"))
    floors = {"ice": 0, "metal": 1, "rock": 1}
    cell_w, cell_h = 56, 52
    img = Image.new(
        "RGB",
        (cell_w * len(FACINGS) * 1, cell_h * len(names) * len(floors)),
        (255, 255, 255),
    )
    ink = [(255, 255, 255), (170, 170, 170), (85, 85, 85), (0, 0, 0)]
    # the floors are drawn as flat swatches of the lightest tones (ice = white + a dither, metal = light grey, rock = mid-light with specks)
    row = 0
    for n in names:
        sp = cat.sprite(n + "_ow")
        for fl in floors:
            for c, f in enumerate(FACINGS):
                idx = spritec.ORDER8.index(f)
                lv, mk = sp.frames[idx][0]
                x0, y0 = c * cell_w, row * cell_h
                for y in range(cell_h):
                    for x in range(cell_w):
                        v = 255
                        if fl == "metal":
                            v = 255 if (x + y) % 4 else 200
                        elif fl == "rock":
                            v = 200 if (x * 7 + y * 13) % 11 else 130
                        else:
                            v = 255 if (x // 2 + y) % 9 else 225
                        img.putpixel((x0 + x, y0 + y), (v, v, v))
                ox, oy = x0 + (cell_w - sp.w) // 2, y0 + cell_h - 4 - sp.h
                for y in range(sp.h):
                    for x in range(sp.w):
                        if mk[y][x]:
                            img.putpixel((ox + x, oy + y), ink[lv[y][x]])
            row += 1
    out = os.path.join(ROOT, "build", "legend")
    os.makedirs(out, exist_ok=True)
    p = os.path.join(out, "ow_sheet.png")
    img.resize((img.size[0] * 4, img.size[1] * 4), Image.NEAREST).save(p)
    print("wrote", p)


def main():
    cmd = (
        sys.argv[1]
        if len(sys.argv) > 1 and sys.argv[1] in ("build", "check", "sheet")
        else "build"
    )
    names = [a for a in sys.argv[1:] if a in SPECIES] or SPECIES
    if cmd == "build":
        for n in names:
            print(n, "%dx%d" % build(n))
    elif cmd == "check":
        bad = [m for n in names for m in check(n)]
        for m in bad:
            print("legend_ow:", m)
        sys.exit(1 if bad else 0)
    else:
        sheet(names)


if __name__ == "__main__":
    main()
