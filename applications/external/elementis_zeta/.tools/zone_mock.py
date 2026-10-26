#!/usr/bin/env python3
"""Zone-identity mockups (M4 polish): one screen per outdoor zone at the final size, in 4 grays, built from the game's own
pieces (assets/art/tiles/*.lv.png, drawn once by tools/gen_tiles.py zone_pieces(): pavers, pond, pines, lamps, fountain, bushes...)
and the optional zone-name banner. Same geometry as the engine (tools/gen_tiles.py: 28x14 diamonds, object anchor = bottom vertex of the
front tile, camera centred on the hero's tile at (64, 32)).

  python3 -I tools/zone_mock.py
writes build/zone_gate/<zone>.gif, build/zone_gate/sheet.png and build/zone_gate/anim.bin (the anim_gate viewer's
format, see tools/title_mock.py: copy it to /ext/apps_data/anim_gate/anim.bin).
"""
import math
import os
import random
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image  # noqa: E402
import battle_mock as bm  # noqa: E402
import gen_tiles as gt  # noqa: E402
import lvlib  # noqa: E402

SW, SH = 128, 64
OUT = os.path.join(ROOT, "build", "zone_gate")
TICK = 100
NF = 40  # frames per scene
BANNER_F = 22  # frames the zone name stays up


# ---------------------------------------------------------------- sprites in memory
class Sp:
    def __init__(self, lv, mk, ax, ay, fw=1, fh=1):
        self.lv, self.mk, self.ax, self.ay, self.fw, self.fh = lv, mk, ax, ay, fw, fh


def old(name):
    """A piece of the game: the generated assets/art/tiles/<name>.lv.png (tools/gen_tiles.py draws each piece ONCE; run it first)."""
    lv, mk, m = gt.load_lv(name)
    return Sp(lv, mk, m["ax"], m["ay"], m["fw"], m["fh"])


def pick(names):
    """Variant lookup: names[k % len] of the loaded pieces (the game picks by a hash of the cell; the mock by a fixed rule)."""
    pcs = [old(n) for n in names]
    return lambda k: pcs[k % len(pcs)]


def hero(view):
    s = bm.load(os.path.join(ROOT, "models_dl/pixellab/heroV3txt", view + ".png"))
    return Sp(s.lv, s.mk, s.w // 2, s.h - 1)


def npc(path):
    s = bm.load(os.path.join(ROOT, path))
    return Sp(s.lv, s.mk, s.w // 2, s.h - 1)


# ---------------------------------------------------------------- composition
def centre(i, j, ci, cj):
    return 64 + ((i - ci) - (j - cj)) * 14, 32 + ((i - ci) + (j - cj)) * 7


def blit(p, s, x0, y0):
    for y, row in enumerate(s.lv):
        for x, v in enumerate(row):
            if s.mk[y][x]:
                X, Y = x0 + x, y0 + y
                if 0 <= X < SW and 0 <= Y < SH:
                    p[Y][X] = v


def compose(grid, legend, hero_at, hero_spr, bg=0, f=0, extra=()):
    """grid rows = j, columns = i. legend: char -> (ground Sp or callable(i, j, f), object Sp / callable or None)."""
    p = [[bg] * SW for _ in range(SH)]
    ci, cj = hero_at
    rows = [r for r in grid.strip("\n").split("\n")]
    items = []
    for j, row in enumerate(rows):
        for i, ch in enumerate(row):
            g, o = legend[ch]
            if callable(g):
                g = g(i, j, f)
            if g is not None:
                cx, cy = centre(i, j, ci, cj)
                blit(p, g, cx - g.ax, cy - g.ay)
            if o is not None:
                if callable(o):
                    o = o(i, j, f)
                if o is not None:
                    items.append((i, j, o))
    for i, j, o in extra:
        items.append((i, j, o(i, j, f) if callable(o) else o))
    seq = []
    for i0, j0, o in items:
        i1, j1 = i0 + o.fw - 1, j0 + o.fh - 1
        seq.append((i1 + j1, 0, i0, j0, o))
    seq.append((ci + cj, 1, ci, cj, hero_spr))
    seq.sort(key=lambda t: (t[0], t[1], t[2]))
    for _, kind, i0, j0, o in seq:
        if kind == 1:
            cx, cy = centre(i0, j0, ci, cj)
            blit(p, o, cx - o.ax, cy + 1 - o.ay)
        else:
            cx, cy = centre(i0 + o.fw - 1, j0 + o.fh - 1, ci, cj)
            blit(p, o, cx - o.ax, cy + 7 - o.ay)
    return p


def banner(p, name, f):
    """Zone name plate, top-left, style: slides down, holds, slides up."""
    if f >= BANNER_F:
        return
    w = len(name) * 6 + 7
    h = 13
    y0 = -h + min(h, (f + 1) * 4) if f < BANNER_F - 3 else -(f - (BANNER_F - 3) + 1) * 4
    for y in range(y0, y0 + h):
        for x in range(0, w):
            if 0 <= y < SH:
                edge = x in (0, w - 1) or y in (y0, y0 + h - 1)
                p[y][x] = 3 if edge else 0
                if not edge and y == y0 + h - 2:
                    p[y][x] = 1
    x = 4
    for ch in name:
        for gx, gy in bm.FONT.g.get(ch, []):
            X, Y = x + gx, y0 + 3 + gy
            if 0 <= X < SW and 0 <= Y < SH:
                p[Y][X] = 3
        x += 6


# ---------------------------------------------------------------- the five zones
# Every piece below is a generated game asset (tools/gen_tiles.py zone_pieces(); `python3 tools/gen_tiles.py` first): the mock has no drawing
# of its own, so what it shows is what ships. The game picks tile / object variants by a hash of the cell; the mock by a fixed rule.
GRASS = pick(["grass_a", "grass_a", "grass_b"])
TALL = old("tall_grass")
PATH = old("path_a")
TREE = old("tree_code")


def grass_at(i, j, f):
    return GRASS(i * 7 + j * 3)


def path_at(i, j, f):
    return PATH


def water_at(W):
    """The pond: 2 frames per tile (water_<sides facing land>_0 / _1, the ripples drift); frame 1 is not in the game yet (Villa's memory budget)."""

    def wat(i, j, f):
        e = [
            k
            for k, d in (
                ("tl", (i - 1, j)),
                ("tr", (i, j - 1)),
                ("br", (i + 1, j)),
                ("bl", (i, j + 1)),
            )
            if d not in W
        ]
        return old("water_%s_%d" % ("_".join(e) or "c", (f // 6) % 2))

    return wat


def cells(grid, chars):
    return {
        (i, j)
        for j, row in enumerate(grid.strip("\n").split("\n"))
        for i, ch in enumerate(row)
        if ch in chars
    }


def run(grid, legend, hero_at, view, name, bg=0, extra=()):
    out = []
    for f in range(NF):
        p = compose(grid, legend, hero_at, hero(view), bg=bg, f=f, extra=extra)
        banner(p, name, f)
        out.append(p)
    return out


# Layouts: 13 x 13, the hero on (6, 6). On screen: -i = up-left, -j = up-right, +i = down-right, +j = down-left;
# only about 3 tiles round the hero are visible.
def villa():
    grid = """
.............
.............
.....T==.....
....f.==.f...
....f.==.....
..T.f.==..f..
...bbm==.....
...HHq==..ww.
..fHHq==..ww.
....fq==.....
......==.....
.............
.............
"""
    legend = {
        ".": (grass_at, None),
        "=": (path_at, None),
        "f": (grass_at, old("flowerbed")),
        "T": (grass_at, TREE),
        "H": (grass_at, None),
        "b": (grass_at, old("flowerbed")),
        "q": (grass_at, old("picket_i")),
        "m": (grass_at, old("mailbox")),
        "w": (water_at(cells(grid, "w")), None),
    }
    return run(
        grid,
        legend,
        (6, 6),
        "south-west",
        "VILLA ALISO",
        extra=[(3, 7, old("fb_house"))],
    )


def ruta1():
    grid = """
.............
.............
.....B==.....
....ggg=B....
...ggg==.B...
..Bggg==.S...
..B..f==..f..
...B..==..B..
..vvvv==vvvv.
....f.==ggg..
....B.==ggg..
.............
.............
"""
    flowers = old("flowers")
    legend = {
        ".": (grass_at, None),
        "=": (path_at, None),
        "f": (flowers, None),
        "g": (TALL, None),
        "B": (grass_at, old("bush")),
        "v": (old("ledge_sw"), None),
        "S": (grass_at, old("sign")),
    }
    return run(grid, legend, (6, 6), "north-east", "RUTA 1")


def nodo():
    pv, pz = old("pave_a"), old("plaza_a")
    grid = """
=============
=============
=============
=============
===L=====L===
====PPPPP====
===tPPPPPt===
====PPPPP====
====PPPPP====
===bPPPPPb===
====PPPPP====
===L=====L===
=============
"""
    legend = {
        "=": (pv, None),
        "P": (pz, None),
        "L": (pv, old("lamp")),
        "t": (pv, old("planter")),
        "b": (pz, old("bench")),
    }
    return run(
        grid,
        legend,
        (6, 9),
        "north-east",
        "CIUDAD NODO",
        extra=[(6, 7, old("fountain"))],
    )


def ruta2():
    pn = pick(["pine_a", "pine_b"])
    grid = """
.............
.............
....PP==PP...
...PT.==.PP..
..PP..==..P..
..Pgg.==..TP.
..Pgg.==.f.P.
...gg.==.B.PP
....P.==..PP.
....lP==.P...
......==.....
.............
.............
"""
    flowers = old("flowers")
    legend = {
        ".": (grass_at, None),
        "=": (path_at, None),
        "f": (flowers, None),
        "g": (TALL, None),
        "P": (grass_at, lambda i, j, f: pn(i + j)),
        "T": (grass_at, TREE),
        "B": (grass_at, old("bush")),
        "l": (grass_at, old("log")),
    }
    return run(
        grid, legend, (6, 6), "north-east", "RUTA 2", extra=[(6, 8, old("arch"))]
    )


def bosque(name="5 Bosque"):
    pd = pick(["pine_dark_a", "pine_dark_b", "pine_dark_c"])
    lt = pick(["litter_a", "litter_b", "litter_c", "litter_d"])
    tr = pick(["trail_a", "trail_b"])
    grid = """
PPPPPPPPPPPPP
PPPPPPPPPPPPP
PPPPPP==PPPPP
PPPPmP==PPPPP
PPP.gg==.PPPP
PPPggg==..sPP
PPm.g.==PP.PP
PPPP..==PPPPP
PPPPPs==.mPPP
PPPPPP==PPPPP
PPPPPP==PPPPP
PPPPPP==PPPPP
PPPPPPPPPPPPP
"""
    legend = {
        ".": (lambda i, j, f: lt(i * 5 + j * 3), None),
        "=": (lambda i, j, f: tr(i + j), None),
        "g": (TALL, None),
        "P": (lambda i, j, f: lt(i * 5 + j * 3), lambda i, j, f: pd(i * 7 + j * 5)),
        "s": (lambda i, j, f: lt(0), old("stump")),
        "m": (lambda i, j, f: lt(1), old("mushrooms")),
    }
    return run(grid, legend, (6, 6), "north-east", "BOSQUE UMBRIO", bg=3)


def pack(p):
    b = bytearray(SW * SH // 4)
    for y in range(SH):
        for x in range(SW):
            b[y * (SW // 4) + (x >> 2)] |= (p[y][x] & 3) << ((x & 3) * 2)
    return bytes(b)


def gif(frames, path, scale=4):
    ims = [
        lvlib.lcd_image(f, None, scale=scale, grid=False).convert(
            "P", palette=Image.ADAPTIVE, colors=8
        )
        for f in frames
    ]
    ims[0].save(path, save_all=True, append_images=ims[1:], duration=TICK, loop=0)


def main():
    os.makedirs(OUT, exist_ok=True)
    scenes = [
        ("1 Villa Aliso", villa()),
        ("2 Ruta 1", ruta1()),
        ("3 Ciudad Nodo", nodo()),
        ("4 Ruta 2", ruta2()),
        ("5 Bosque Umbrio", bosque()),
    ]
    with open(os.path.join(OUT, "anim.bin"), "wb") as f:
        f.write(b"AGT1" + bytes([len(scenes)]))
        for name, frs in scenes:
            f.write(name.encode()[:23].ljust(24, b"\0"))
            f.write(struct.pack("<HH", len(frs), TICK))
            for p in frs:
                f.write(pack(p))
    sheet = Image.new("RGB", (SW * 4 * 2 + 30, SH * 4 * 5 + 60), (40, 40, 40))
    for k, (name, frs) in enumerate(scenes):
        gif(frs, os.path.join(OUT, name.split()[0] + ".gif"))
        for c, fi in enumerate((4, NF - 1)):
            sheet.paste(
                lvlib.lcd_image(frs[fi], None, scale=4, grid=False),
                (10 + c * (SW * 4 + 10), 10 + k * (SH * 4 + 10)),
            )
    sheet.save(os.path.join(OUT, "sheet.png"))
    print("anim.bin:", os.path.getsize(os.path.join(OUT, "anim.bin")), "bytes")


if __name__ == "__main__":
    main()
