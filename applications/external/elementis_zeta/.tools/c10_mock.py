#!/usr/bin/env python3
"""Chapter 10 zone-identity mockups (the design notes §4): the Ruinas del Origen (8 fragment pedestals around the
hero, the Genesa/Arkegen mural stele, fallen columns, big light ancient slabs) and the hidden Gruta del Eclipse (dark rock with
light veins, the altar with the Piedra Eclipse, the light shaft that only shows at the right hours). Same composer as tools/c9_mock.py;
the pieces are drawn ONCE in tools/gen_tiles.py (c10_pieces, contract 68) and this mock loads the generated files (run tools/gen_tiles.py first), like tools/c9_mock.py.

  python3 -I tools/c10_mock.py [KEYS...] # RUINAS_A RUINAS_B MURAL GRUTA_A GRUTA_B
writes build/c10_mock/<zone>.gif, sheet.png and anim.bin (anim_gate format).
"""
import math
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image  # noqa: E402
import gen_tiles as gt  # noqa: E402
import lvlib  # noqa: E402
import zone_mock as zm  # noqa: E402
import c2_mock as c2  # noqa: E402
import c4_mock as c4  # noqa: E402
import c7_mock as c7  # noqa: E402

OUT = os.path.join(ROOT, "build", "c10_mock")
old, done, hv, at_f = zm.old, c2.done, c2.hv, c2.at_f
mapof, P, txt, run = c2.mapof, c2.P, c2.txt, c2.run
Spr, gtile, dither = gt.Spr, gt.gtile, gt.dither
memo, T, ground, ob = c4.memo, c4.T, c4.ground, c4.ob
big = c7.big


# ================================================================ the pieces of C10
# Drawn ONCE in tools/gen_tiles.py (`c10_pieces()`, contract 68: moved there pixel for pixel, `python3 tools/c10_pieces_check.py` compares them with the old in-memory drawing kept in tools/attic/c10_mock_before_pieces.py),
# written to assets/art/tiles/ and registered in assets/catalog.toml; this mock only loads the generated files (run `python3 tools/gen_tiles.py` first). The names below are the call signatures the scenes were written with.
def ancient_slab(pi, pj, seed=0):
    return old("aslab_%d%d" % (pi, pj))


def actor_sp(name):
    """A game ACTOR as a sprite (an actor stands 6 rows above the bottom vertex of its tile, the anchor an object has: gen_tiles `_actor_write` dy)."""
    from lvlib import read_lv_png

    lv, mk, m = read_lv_png(
        os.path.join(ROOT, "assets/art/npcs/%s/south/f0.lv.png" % name)
    )
    return zm.Sp(lv, mk, m["ax"], m["ay"] + 6)


def overlay(base, top):
    """`top` (an actor) drawn over `base` (the object under it) on one canvas, aligned by their anchors, as the game does."""
    x0, y0 = max(base.ax, top.ax), max(base.ay, top.ay)
    x1 = max(len(base.mk[0]) - base.ax, len(top.mk[0]) - top.ax)
    y1 = max(len(base.mk) - base.ay, len(top.mk) - top.ay)
    w, h = x0 + x1, y0 + y1
    lv = [[0] * w for _ in range(h)]
    mk = [[False] * w for _ in range(h)]
    for sp in (base, top):
        for y, row in enumerate(sp.lv):
            for x, v in enumerate(row):
                if sp.mk[y][x]:
                    lv[y0 - sp.ay + y][x0 - sp.ax + x] = v
                    mk[y0 - sp.ay + y][x0 - sp.ax + x] = True
    return zm.Sp(lv, mk, x0, y0, base.fw, base.fh)


def pedestal(lit=False, phase=0):
    """The empty pedestal is the map's object `ped0`; the lit one is the game's actor `ped_lit` (one frame, no sparkle) laid over it by a flag."""
    return old("ped0") if not lit else overlay(old("ped0"), actor_sp("ped_lit"))


def mural_stele():
    return old("ostele")


def fallen_column(axis="i"):
    return old("fcol_" + axis)


def vein_rock(seed=0):
    return old("vrock_%d" % seed)


def altar(stone=True):
    """The empty altar is the map's object `altar`; with the stone (shown by `cave_open`) the actor `estone` is laid over it."""
    return overlay(old("altar"), actor_sp("estone")) if stone else old("altar")


def light_shaft(cx, cy):
    """Screen overlay: a diagonal shaft of light from a ceiling crack down to (cx, cy), dithered, with drifting motes."""

    def post(p, f):
        for y in range(0, cy):
            t = y / max(1, cy)
            xc = cx - 12 + 12 * t
            for x in range(int(xc) - 5, int(xc) + 6):
                if not 0 <= x < zm.SW:
                    continue
                d = abs(x - xc)
                if d < 1.5:
                    p[y][x] = 0
                elif d < 3.5:
                    p[y][x] = 0 if dither(x, y, 0.6) else 1
                elif dither(x, y, 0.35) and p[y][x] >= 2:
                    p[y][x] = 1
        for k in range(4):
            y = (f * 2 + k * 9) % cy
            x = int(cx - 10 + 10 * y / cy) + (k % 3)
            if 0 <= x < zm.SW:
                p[y][x] = 0

    return post


SCENES = {}


def reg(key, title, fn):
    SCENES[key] = (title, fn)


def room_walls(g, lim=-3):
    for j in range(13):
        for i in range(13):
            if g[j][i] == "W" and (i - 6) > lim:
                g[j][i] = "J"


RING = ((1, -2), (2, -1), (2, 1), (1, 2), (-1, 2), (-2, 1), (-2, -1), (-1, -2))


# ================================================================ RUINAS DEL ORIGEN
def ruinas(n_lit):
    def build():
        g = mapof(lambda u, v, a, b: "W" if (u <= -3 or v <= -3) else ".", ".")
        room_walls(g)
        P(g, {c: ("L" if k < n_lit else "p") for k, c in enumerate(RING)})
        fl = big("aslab", ancient_slab)
        lit = ob("ped1", lambda: pedestal(True))
        lg = {
            ".": (fl, None),
            "W": (fl, old("wall_i")),
            "J": (fl, old("wall_j")),
            "p": (fl, ob("ped0", lambda: pedestal(False))),
            "L": (fl, lit),
            "M": (fl, ob("stele", mural_stele)),
            "c": (fl, ob("colI", lambda: fallen_column("i"))),
            "C": (fl, ob("colJ", lambda: fallen_column("j"))),
        }
        return run(txt(g), lg, (6, 6), "north-east", "RUINAS DEL ORIGEN", bg=0)

    return build


def mural():
    g = mapof(lambda u, v, a, b: "W" if (u <= -3 or v <= -3) else ".", ".")
    room_walls(g)
    P(g, {(1, -1): "M", (-2, 1): "c", (2, 2): "C", (-1, -2): "L", (3, 0): "L"})
    fl = big("aslab", ancient_slab)
    lit = ob("ped1", lambda: pedestal(True))
    lg = {
        ".": (fl, None),
        "W": (fl, old("wall_i")),
        "J": (fl, old("wall_j")),
        "L": (fl, lit),
        "M": (fl, ob("stele", mural_stele)),
        "c": (fl, ob("colI", lambda: fallen_column("i"))),
        "C": (fl, ob("colJ", lambda: fallen_column("j"))),
    }
    return run(txt(g), lg, (6, 6), "north-east", "RUINAS DEL ORIGEN", bg=0)


reg("RUINAS_A", "Ruinas (3 de 8)", ruinas(3))
reg("RUINAS_B", "Ruinas (8 de 8)", ruinas(8))
reg("MURAL", "Ruinas (mural)", mural)


# ================================================================ GRUTA DEL ECLIPSE (hidden cave of the Piedra Eclipse)
def gruta(hour):
    def build():
        g = mapof(lambda u, v, a, b: "W" if (u <= -3 or v <= -3) else ".", ".")
        room_walls(g)
        P(g, {(2, 1): "A", (-2, 1): "s", (-1, 3): "s", (2, -2): "s"})
        fl = ground("vrock", vein_rock, 3)
        lg = {
            ".": (fl, None),
            "W": (fl, hv([old("cwall_a0"), old("cwall_b0")])),
            "J": (fl, hv([old("cwall_a1"), old("cwall_b1")])),
            "A": (fl, ob(("altar", hour), lambda: altar(stone=hour))),
            "s": (fl, old("stalag_a")),
        }
        # the altar cell (2, 1) sits at screen x = 64 + 14 * 1, y = 32 + 7 * 3 (its base: the PixelLab altar is 36 px tall, so it stands lower than the old drawn one, clear of the title banner); the shaft ends on the stone above it (~27 px over the base)
        return run(
            txt(g),
            lg,
            (6, 6),
            "south-west",
            "GRUTA DEL ECLIPSE",
            bg=0,
            post=light_shaft(78, 26) if hour else None,
        )

    return build


reg("GRUTA_A", "Gruta (otra hora)", gruta(False))
reg("GRUTA_B", "Gruta (amanecer)", gruta(True))


# ================================================================ output
def write(keys):
    os.makedirs(OUT, exist_ok=True)
    sel = [k for k in SCENES if not keys or k in keys]
    scenes = [(SCENES[k][0], SCENES[k][1](), k) for k in sel]
    with open(os.path.join(OUT, "anim.bin"), "wb") as f:
        f.write(b"AGT1" + bytes([len(scenes)]))
        for name, frs, _ in scenes:
            f.write(name.encode()[:23].ljust(24, b"\0"))
            f.write(struct.pack("<HH", len(frs), zm.TICK))
            for p in frs:
                f.write(zm.pack(p))
    cw, ch = zm.SW * 3, zm.SH * 3
    rows = (len(scenes) + 1) // 2
    sheet = Image.new("RGB", (cw * 2 + 30, (ch + 10) * rows + 10), (40, 40, 40))
    for n, (name, frs, k) in enumerate(scenes):
        zm.gif(frs, os.path.join(OUT, "%s.gif" % k))
        sheet.paste(
            lvlib.lcd_image(frs[6], None, scale=3, grid=False),
            (10 + (n % 2) * (cw + 10), 10 + (n // 2) * (ch + 10)),
        )
    sheet.save(os.path.join(OUT, "sheet.png"))
    print(
        "scenes:",
        len(scenes),
        "anim.bin:",
        os.path.getsize(os.path.join(OUT, "anim.bin")),
        "bytes",
    )


if __name__ == "__main__":
    write([a.upper() for a in sys.argv[1:]])
