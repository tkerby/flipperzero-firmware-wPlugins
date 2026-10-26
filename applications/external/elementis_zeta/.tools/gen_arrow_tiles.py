#!/usr/bin/env python3
"""gen_arrow_tiles.py - (C4b: SUPERSEDED by tools/gen_tiles.py arrow_plate; the .lv.locked files keep this script from rewriting the pngs) PLACEHOLDER art for the arrow floor tiles (C4-A, contract 35.3): assets/art/tiles/arrow_<facing>.lv.png (+ .json), 28 x 15, the black arrow lying on the floor
(drawn in the tile plane and projected like the floor: it points along the facing). The catalog gives them `slide = "<facing>"` and `fill = 0` (a white diamond under the art), so
only the arrow is drawn here (level 0 = transparent, no mask). the maintainer / the maintainer may redraw them (a .locked file next to a png keeps this script from rewriting it).
  python3 tools/gen_arrow_tiles.py
"""
import json, math, os, sys
from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "art", "tiles")
FACINGS = ["S", "SE", "E", "NE", "N", "NW", "W", "SW"]
DI = [1, 1, 1, 0, -1, -1, -1, 0]
DJ = [1, 0, -1, -1, -1, 0, 1, 1]
GRAYS = (255, 170, 85, 0)
W, H, AX, AY = 28, 15, 14, 7


def tile(f):
    di, dj = DI[f], DJ[f]
    n = math.hypot(di, dj)
    ux, uy = di / n, dj / n
    px = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            sx, sy = x - AX, y - AY
            i, j = (sx / 14 + sy / 7) / 2, (sy / 7 - sx / 14) / 2
            if abs(i) + abs(j) > 0.62 and (abs(i) > 0.5 or abs(j) > 0.5):
                continue
            al = i * ux + j * uy
            pe = -i * uy + j * ux
            shaft = -0.42 <= al <= 0.12 and abs(pe) <= 0.085
            head = 0.12 <= al <= 0.47 and abs(pe) <= 0.30 * (0.47 - al) / 0.35
            if shaft or head:
                px[y][x] = 3
    return px


def main():
    for f, nm in enumerate(FACINGS):
        base = os.path.join(OUT, f"arrow_{nm.lower()}")
        if os.path.exists(base + ".lv.locked"):
            continue
        lv = tile(f)
        im = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        p = im.load()
        for y in range(H):
            for x in range(W):
                if lv[y][x]:
                    g = GRAYS[lv[y][x]]
                    p[x, y] = (g, g, g, 255)
        im.save(base + ".lv.png")
        json.dump(
            {"w": W, "h": H, "ax": AX, "ay": AY, "fw": 1, "fh": 1, "kind": "tile"},
            open(base + ".json", "w"),
        )
        if "--show" in sys.argv:
            print(nm)
            for row in lv:
                print("".join(".#"[v == 3] for v in row))


if __name__ == "__main__":
    main()
