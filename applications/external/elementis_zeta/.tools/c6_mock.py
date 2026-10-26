#!/usr/bin/env python3
"""Chapter 6 (+ the Chapter 8 gym) zone-identity mockups: Ciudad Cenit, Torre Vertice (office floor with teleport pads and a card-key door),
Dojo del Puno, Gimnasio Mente (teleport maze), Gimnasio Tierra (Ciudad Nodo, Director Varo). Same composer as tools/c5_mock.py; every piece loads
from assets/art/tiles: the C6 ones are drawn in tools/gen_tiles.py `c6_pieces` (moved there from this mock unchanged, contract 49; run `python3 tools/gen_tiles.py` first). Exception: the C8 earth gym's sand slab (scene TIERRA) is still drawn here: that piece.

  python3 -I tools/c6_mock.py [KEYS...] # CENIT TORRE DOJO MENTE TIERRA
writes build/c6_mock/<zone>.gif, sheet.png and anim.bin (anim_gate format).
"""
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

OUT = os.path.join(ROOT, "build", "c6_mock")
old, done, hv, at_f = zm.old, c2.done, c2.hv, c2.at_f
mapof, P, txt, run, cells_of = c2.mapof, c2.P, c2.txt, c2.run, c2.cells_of
Spr, Box, gtile, dither = gt.Spr, gt.Box, gt.gtile, gt.dither
memo, T, ground, ob, npc = c4.memo, c4.T, c4.ground, c4.ob, c4.npc
NF = zm.NF


def big(name, fn):
    """2 x 2 slab quarters by cell parity."""
    q = {
        (a, b): memo((name, a, b), lambda a=a, b=b: fn(a, b))
        for a in (0, 1)
        for b in (0, 1)
    }
    return lambda i, j, fr: q[(i % 2, j % 2)]


# ================================================================ the pieces of C6
# Drawn ONCE in tools/gen_tiles.py (`c6_pieces()`, written to assets/art/tiles/ and registered in assets/catalog.toml, like C3 - C5's: contract 49, moved there from this mock UNCHANGED, pixel for pixel);
# this mock only loads the generated files (run `python3 tools/gen_tiles.py` first). The names below are the call signatures the scenes were written with.
def big(name, fn):
    """2 x 2 slab quarters by cell parity."""
    q = {
        (a, b): memo((name, a, b), lambda a=a, b=b: fn(a, b))
        for a in (0, 1)
        for b in (0, 1)
    }
    return lambda i, j, fr: q[(i % 2, j % 2)]


def plaza_light(pi, pj, seed=0):
    return old("ring_%d%d" % (pi, pj))


def office(pi, pj, seed=0):
    return old("carpet_%d%d" % (pi, pj))


def pad(phase=0):
    return old("pad_%d" % phase)


def dojo_floor(pi, pj, seed=0):
    return old("boards_%d%d" % (pi, pj))


def sand_slab(pi, pj, seed=0):
    """Ground gym: big sandstone slabs, light with sparse grit. Drawn ONCE in tools/gen_tiles.py (`c8_pieces()`, contract 51; moved there pixel for pixel), this mock only loads
    the generated file (run `python3 tools/gen_tiles.py` first)."""
    return old("sslab_%d%d" % (pi, pj))


def glass_tower(seed=0):
    return old("glass_tower")


def orb(phase=0):
    return old("orb_%d" % phase)


def desk():
    return old("desk")


def keydoor(locked=True):
    return npc("keydoor", "south")


def dummy():
    return old("dummy")


def scroll_wall(axis="i"):
    return old("scroll_wall")


SCENES = {}


def reg(key, title, fn):
    SCENES[key] = (title, fn)


def room_walls(g, lim=-3):
    for j in range(13):
        for i in range(13):
            if g[j][i] == "W" and (i - 6) > lim:
                g[j][i] = "J"


# ================================================================ CIUDAD CENIT
def cenit():
    g = mapof(lambda u, v, a, b: ".", ".")
    P(g, {(-2, 1): "o", (2, -1): "o", (1, 2): "l", (-3, -1): "l"})
    pl = big("plz", plaza_light)
    o = at_f([ob(("orb", k), lambda k=k: orb(k)) for k in (0, 1)], 5)
    lg = {
        ".": (pl, None),
        "o": (pl, o),
        "l": (pl, at_f([old("lamp_h0"), old("lamp_h1")], 4)),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "CIUDAD CENIT",
        extra=[
            (9, 5, ob("gtower", glass_tower)),
            (4, 8, ob("gtower", glass_tower)),
            (7, 4, npc("explorador", "south-west")),
        ],
    )


reg("CENIT", "Ciudad Cenit", cenit)


# ================================================================ TORRE VERTICE (office floor)
def torre():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."

    g = mapof(f, ".")
    room_walls(g)
    P(
        g,
        {
            (-1, -1): "d",
            (1, -1): "d",
            (-1, 1): "d",
            (2, 2): "p",
            (3, 0): "K",
            (0, 2): "q",
        },
    )
    fl = big("off", office)
    pd = at_f([ob(("pad", k), lambda k=k: pad(k)) for k in (0, 1)], 4)
    lg = {
        ".": (fl, None),
        "W": (fl, old("wall_i")),
        "J": (fl, old("wall_j")),
        "d": (fl, ob("desk", desk)),
        "p": (pd, None),
        "q": (pd, None),
        "K": (fl, ob("kdoor", keydoor)),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "TORRE VERTICE",
        bg=0,
        extra=[(8, 4, npc("vertice", "south-west"))],
    )


reg("TORRE", "Torre Vertice", torre)


# ================================================================ DOJO DEL PUNO
def dojo():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."

    g = mapof(f, ".")
    room_walls(g)
    P(g, {(-1, 1): "m", (2, -1): "m", (1, 2): "y"})
    fl = big("dojo", dojo_floor)
    lg = {
        ".": (fl, None),
        "W": (fl, ob("scr_i", lambda: scroll_wall("i"))),
        "J": (fl, ob("scr_j", lambda: scroll_wall("j"))),
        "m": (fl, ob("dummy", dummy)),
        "y": (fl, old("mat")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "DOJO DEL PUNO",
        bg=0,
        extra=[(8, 4, npc("montanista", "south-west"))],
    )


reg("DOJO", "Dojo", dojo)


# ================================================================ GIMNASIO MENTE (teleport maze)
def mente():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        if u == 1 and v != 0:
            return "Y"  # low glass partitions between the small rooms (along +j)
        if v == 1 and u != 0:
            return "X"  # (along +i)
        return "."

    g = mapof(f, ".")
    room_walls(g)
    P(g, {(0, 0): ".", (-1, -1): "p", (2, 2): "p", (2, -1): "p", (-1, 2): "p"})
    fl = big("plzm", plaza_light)
    pd = at_f([ob(("pad", k), lambda k=k: pad(k)) for k in (0, 1)], 4)
    lg = {
        ".": (fl, None),
        "W": (fl, old("wall_i")),
        "J": (fl, old("wall_j")),
        "X": (fl, old("rail_i")),
        "Y": (fl, old("rail_j")),
        "p": (pd, None),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "GIMNASIO MENTE",
        bg=0,
        extra=[(8, 3, npc("medium", "south-west"))],
    )


reg("MENTE", "Gym Mente", mente)


# ================================================================ GIMNASIO TIERRA (C8, Ciudad Nodo)
def tierra():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."

    g = mapof(f, ".")
    room_walls(g)
    P(g, {(-1, 1): "r", (2, -1): "r", (3, 2): "b", (-2, -2): "b"})
    fl = big("sand", sand_slab)
    lg = {
        ".": (fl, None),
        "W": (fl, old("wall_i")),
        "J": (fl, old("wall_j")),
        "r": (fl, hv([old("rock_code"), old("boulder")])),
        "b": (fl, old("pillar")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "GIMNASIO TIERRA",
        bg=0,
        extra=[(8, 4, npc("varo", "south-west"))],
    )


reg("TIERRA", "Gym Tierra (C8)", tierra)


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
