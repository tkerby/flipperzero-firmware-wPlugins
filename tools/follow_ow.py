#!/usr/bin/env python3
"""Overworld sprites of the FOLLOWER (contract 69): the first creature of the party walks one tile behind the hero. ONE file for ALL species, no PixelLab, no drawn art.

  python3 tools/follow_ow.py [build|check|sheet]

build writes nucleo/files/follow.bin from assets/art/elementis/<art>/{south,south-east,east,north-east,north}.png (64 px, the same pictures tools/roam_ow.py and tools/monc.py read; a picture that is not an
       image is replaced by its neighbour facing, as roam_ow.load_facing does). Per species: the five stored facings (S SE E NE N; the three west facings are the engine's mirrors) are cropped, scaled by
       the SAME factor (the tallest facing = ROWS rows, 12 at 0.3 m, +2.5 per doubling of the species' `height`, at most FOL_H; never wider than FOL_W), alpha cut at 128 (premultiplied Lanczos, the
       recipe of roam_ow.py), centred on a FOL_W x FOL_H canvas standing on the bottom row, and converted to the 4-gray levels + a 1-bit mask with THE SPECIES' OWN tone mapping (levels = "adaptive", dark_cut =
       its art_dark_cut, adaptive_split = its art_split: the light typecast for the dark creatures, as the roamers and the pictures have it; a facing that still comes out a black silhouette is converted
       again with half the cut, then by the quantile split [0.35, 0.75]).
       Layout (little endian; the engine reads it with one read_at per facing, nucleo/map.c map_actor_view, SPRF_STREAM semantics):
           0 "NFOL" 4 magic
           4 version 1 (1)
           5 count 1 species stored (ids 1 .. count)
           6 w, h 2 FOL_W, FOL_H (nucleo/map.h)
           8 frame 2 bytes of one facing's frame: pixels ((w + 3) / 4 * h) then the mask ((w + 7) / 8 * h)
           10 record 2 bytes of one species: 5 facings x frame
           12 pad 4
           16 top[count] 1 B each, padded with 0 to FOL_HDR (176): the first row with an opaque pixel in any facing (the emote bubble sits above it)
           FOL_HDR + (species - 1) * record + facing * frame the facings in the order S SE E NE N
check exits 1 when nucleo/files/follow.bin is not byte for byte what `build` writes (the build step of tools/build_assets.sh runs it).
sheet writes build/follow/follow_sheet.png: every species on a light and a dark floor swatch, the five stored facings, at 4x, as the LCD gets them.
"""
import math
import os
import re
import struct
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image  # noqa: E402
import lvlib  # noqa: E402
from legend_ow import bbox, premul_resize  # noqa: E402
import roam_ow  # noqa: E402

FACINGS = roam_ow.FACINGS          # south, south-east, east, north-east, north
OUT = os.path.join(ROOT, "nucleo", "files", "follow.bin")
MAP_H = os.path.join(ROOT, "nucleo", "map.h")
MIN_ROWS, BASE_M, PER_DOUBLING = 12, 0.3, 2.5
HDR = 16


def engine_consts():
    """FOL_W, FOL_H, FOL_HDR from nucleo/map.h (the engine's side of the file format)."""
    src = open(MAP_H, encoding="utf-8").read()
    out = {}
    for k in ("FOL_W", "FOL_H", "FOL_HDR"):
        m = re.search(r"#define %s (\d+)" % k, src)
        if not m:
            sys.exit("follow_ow: %s not found in nucleo/map.h" % k)
        out[k] = int(m.group(1))
    return out["FOL_W"], out["FOL_H"], out["FOL_HDR"]


def rows_for(height, fol_h):
    return max(MIN_ROWS, min(fol_h, round(MIN_ROWS + PER_DOUBLING * math.log2(max(height, 0.01) / BASE_M))))


def species_list():
    """[(id, key, data)] in species.toml order (id = position, 1-based)."""
    with open(roam_ow.SPECIES_TOML, "rb") as f:
        d = tomllib.load(f)
    return [(i + 1, k, v) for i, (k, v) in enumerate(d.items())]


def render_species(sd, fol_w, fol_h):
    """-> [(levels, mask)] for the five facings on a fol_w x fol_h canvas."""
    crops = []
    for f in FACINGS:
        im = roam_ow.load_facing(sd["art"], f)
        crops.append(im.crop(bbox(im)))
    top = max(i.size[1] for i in crops)
    wid = max(i.size[0] for i in crops)
    k = min(rows_for(float(sd.get("height", BASE_M)), fol_h) / top, fol_w / wid)
    out = []
    for f, im in zip(FACINGS, crops):
        w, h = max(1, round(im.size[0] * k)), max(1, round(im.size[1] * k))
        w, h = min(w, fol_w), min(h, fol_h)
        s = premul_resize(im, w, h)
        s = s.crop(bbox(s))                                      # (the resample can leave the outer row below the alpha cut: the body stands on the bottom row whatever)
        c = Image.new("RGBA", (fol_w, fol_h), (0, 0, 0, 0))
        c.alpha_composite(s, ((fol_w - s.size[0]) // 2, fol_h - s.size[1]))      # centred, standing on the bottom row
        out.append(c)
    cut = int(sd.get("art_dark_cut", roam_ow.DARK_CUT))
    split = sd.get("art_split")
    res = []
    for f, c in zip(FACINGS, out):
        lv = mk = None
        # the species' own typecast first; a facing that comes out a black silhouette (>= 90 % of its pixels at level 3: a very dark back view) is converted again with half the cut, then by the
        # quantile split [0.35, 0.75] (what the very dark species of species.toml use), so it keeps some shading at 20 px
        for k, (cc, sp) in enumerate(((cut, split), (cut // 2, None), (25, [0.35, 0.75]))):
            if sp:
                tones = lvlib.quantile_tones([c], [float(sp[0]), float(sp[1])], cc)
            else:
                tones = lvlib.adaptive_tones([c], cc)
            lv, mk = lvlib.adaptive_levels(c, cc, tones)
            n = sum(map(sum, mk))
            if sum(1 for y in range(fol_h) for x in range(fol_w) if mk[y][x] and lv[y][x] == 3) * 10 < n * 9:
                break
        res.append((lv, mk))
    return res


def pack(facings, fol_w, fol_h):
    blob = b""
    for lv, mk in facings:
        blob += lvlib.pack_2bpp(lv, 0, 0, fol_w - 1, fol_h - 1) + lvlib.pack_mask(mk, 0, 0, fol_w - 1, fol_h - 1)
    return blob


def top_row(facings):
    for y in range(len(facings[0][1])):
        if any(any(mk[y]) for _, mk in facings):
            return y
    return 0


def build_bytes():
    fol_w, fol_h, fol_hdr = engine_consts()
    sp = species_list()
    frame = ((fol_w + 3) // 4) * fol_h + ((fol_w + 7) // 8) * fol_h
    rec = 5 * frame
    if HDR + len(sp) > fol_hdr:
        sys.exit("follow_ow: %d species do not fit the table before FOL_HDR %d" % (len(sp), fol_hdr))
    tops, recs = bytearray(), bytearray()
    for i, key, sd in sp:
        if not sd.get("art"):
            sys.exit("follow_ow: species %r has no art in species.toml" % key)
        fs = render_species(sd, fol_w, fol_h)
        tops.append(top_row(fs))
        recs += pack(fs, fol_w, fol_h)
    head = b"NFOL" + struct.pack("<BBBBHH4x", 1, len(sp), fol_w, fol_h, frame, rec)
    assert len(head) == HDR
    table = bytes(tops) + b"\0" * (fol_hdr - HDR - len(tops))
    assert len(recs) == rec * len(sp)
    return head + table + bytes(recs)


def sheet():
    fol_w, fol_h, fol_hdr = engine_consts()
    data = open(OUT, "rb").read()
    frame, rec = struct.unpack_from("<HH", data, 8)
    n = data[5]
    sp = species_list()
    cell_w, cell_h = fol_w + 6, fol_h + 6
    cols = 5 * 2
    rows = (n + 1) // 2
    img = Image.new("RGB", (cell_w * cols, cell_h * ((n + 1) // 2 + 0)), (255, 255, 255))
    ink = [(255, 255, 255), (170, 170, 170), (85, 85, 85), (0, 0, 0)]
    for s in range(n):
        side = s % 2           # even species on a light floor, odd ones on a dark floor (a sheet of 78 rows)
        row = s // 2
        for fc in range(5):
            x0 = (side * 5 + fc) * cell_w
            y0 = row * cell_h
            for y in range(cell_h):
                for x in range(cell_w):
                    v = (255 if (x // 2 + y) % 9 else 225) if side == 0 else (130 if (x * 7 + y * 13) % 11 else 85)
                    img.putpixel((x0 + x, y0 + y), (v, v, v))
            base = fol_hdr + s * rec + fc * frame
            bpr, mbpr = (fol_w + 3) // 4, (fol_w + 7) // 8
            for y in range(fol_h):
                for x in range(fol_w):
                    mb = data[base + bpr * fol_h + y * mbpr + (x >> 3)]
                    if (mb >> (x & 7)) & 1:
                        pv = (data[base + y * bpr + (x >> 2)] >> ((x & 3) * 2)) & 3
                        img.putpixel((x0 + 3 + x, y0 + 3 + y), ink[pv])
    out = os.path.join(ROOT, "build", "follow")
    os.makedirs(out, exist_ok=True)
    p = os.path.join(out, "follow_sheet.png")
    img.resize((img.size[0] * 4, img.size[1] * 4), Image.NEAREST).save(p)
    print("wrote", p, "(%d species, %d rows)" % (n, rows))


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] in ("build", "check", "sheet") else "build"
    if cmd == "sheet":
        sheet()
        return
    want = build_bytes()
    if cmd == "build":
        with open(OUT, "wb") as f:
            f.write(want)
        print("wrote", os.path.relpath(OUT, ROOT), "(%d species, %d bytes)" % (want[5], len(want)))
    else:
        have = open(OUT, "rb").read() if os.path.exists(OUT) else b""
        if have != want:
            print("follow_ow: nucleo/files/follow.bin is not what `python3 tools/follow_ow.py build` writes")
            sys.exit(1)


if __name__ == "__main__":
    main()
