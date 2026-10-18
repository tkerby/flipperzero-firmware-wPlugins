#!/usr/bin/env python3
"""Chapter 5 zone-identity mockups (one per zone): Ruta 12 (fishing pier + the sleeping creature), Rutas 13-14 (fence maze), Ruta 15
(windmills), Ciudad Ambar, Gimnasio Veneno, Autopista Mag, Ruta 18 (boardwalk), Reserva Salvaje, el mar. Same composer / geometry as
tools/c4_mock.py; every piece loads from assets/art/tiles: the C5 ones are drawn in tools/gen_tiles.py `c5_pieces()` (moved there from this mock unchanged; run
`python3 tools/gen_tiles.py` first).

  python3 -I tools/c5_mock.py [KEYS...]      # R12 R1314 R15 AMBAR GYM AUTO R18 RESERVA MAR
writes build/c5_mock/<zone>.gif, sheet.png and anim.bin (anim_gate format).
"""
import math
import os
import random
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

OUT = os.path.join(ROOT, "build", "c5_mock")
old, done, hv, at_f = zm.old, c2.done, c2.hv, c2.at_f
mapof, P, txt, run, cells_of = c2.mapof, c2.P, c2.txt, c2.run, c2.cells_of
Spr, Box, gtile, dither = gt.Spr, gt.Box, gt.gtile, gt.dither
memo, T, ground, ob, npc = c4.memo, c4.T, c4.ground, c4.ob, c4.npc
NF = zm.NF


# ================================================================ the pieces of C5
# Drawn ONCE in tools/gen_tiles.py (`c5_pieces()`, written to assets/art/tiles/ and registered in assets/catalog.toml, like C3's and C4's); this mock only loads the
# generated files (run `python3 tools/gen_tiles.py` first). The names below are the call signatures the scenes were written with.
def wildflowers(seed=0):
    return old("wildf_%d" % seed)


def savanna(seed=0):
    return old("sav_%d" % seed)


def dry_grass(seed=0):
    return old("dgrass_%d" % seed)


def tatami(pi, pj, seed=0):
    return old("tatami_%d%d" % (pi, pj))


def boardwalk(seed=0):
    return old("bwalk")


def windmill(phase=0):
    return old("windmill_%d" % phase)


def palm(seed=0):
    return old("palm_%d" % seed)


def reserve_arch():
    return old("res_arch")


def acacia(seed=0):
    return old("acacia_%d" % seed)


def shoji(axis="i"):
    return old("shoji_" + axis)


def graffiti_rail():
    return old("graffiti_rail")


def road_light(seed=0, mark=None):
    return old({"lane": "road_lane", "arrow": "road_arrow"}[mark] if mark else "roadl_%d" % seed)


def guardrail():
    return old("guardrail")


def downhill_sign():
    return old("downhill_sign")


SCENES = {}


def reg(key, title, fn):
    SCENES[key] = (title, fn)


GRASS = c2.GRASS3


# ================================================================ RUTA 12 (pier over the sea, fishermen, the sleeping creature)
def r12():
    def f(u, v, a, b):
        if u >= 1:
            return "w"                                       # the sea, down-right
        if u == 0:
            return "="                                       # the pier along +j
        return "g"
    g = mapof(f, "g")
    P(g, {(1, -1): "=", (2, -1): "=", (3, -1): "=", (3, -2): "o", (1, 0): "o", (-2, 1): "f", (-3, 0): "T", (-2, -2): "T"})
    W = cells_of(g, "w")
    lg = {
        "g": (GRASS, None), "w": (c2.sea_at(W), None), "=": (c2.deck_at(lambda i, j: "i", W, wet=False), None),
        "o": (c2.deck_at(lambda i, j: "i", W, wet=False), old("bollard")), "f": (GRASS, old("flowerbed")), "T": (old("tall_grass"), None),
    }
    return run(txt(g), lg, (6, 6), "south-east", "RUTA 12",
               extra=[(8, 5, npc("pescador", "south-west")), (5, 3, npc("slumberth", "south-west"))], post=c2.gulls((40, 8, 0.6)))


reg("R12", "Ruta 12", r12)


# ================================================================ RUTAS 13-14 (fence maze, wildflowers)
def r1314():
    def f(u, v, a, b):
        return "."
    g = mapof(f, ".")
    for u in range(-3, 4):
        if u not in (0,):
            P(g, {(u, -1): "F"})
    for v in range(-3, 4):
        if v not in (1,):
            P(g, {(2, v): "f"})
    P(g, {(-2, 1): "T", (-1, 2): "T", (-2, 2): "T", (3, 2): "T", (-3, -3): "T"})
    wf = ground("wildf", wildflowers, 3)
    lg = {".": (wf, None), "F": (wf, old("rfence_i")), "f": (wf, old("rfence_j")), "T": (old("tall_grass"), None)}
    return run(txt(g), lg, (6, 6), "south-east", "RUTA 13", extra=[(7, 8, npc("chica", "north-east"))])


reg("R1314", "Rutas 13-14", r1314)


# ================================================================ RUTA 15 (open fields, windmills)
def r15():
    g = mapof(lambda u, v, a, b: "=" if v == 0 else ".", ".")
    P(g, {(-2, 2): "T", (-1, 2): "T", (2, -2): "T", (3, -2): "T"})
    wf = ground("wildf", wildflowers, 3)
    wm = at_f([ob(("wm", k), lambda k=k: windmill(k)) for k in range(3)], 3)
    lg = {".": (wf, None), "=": (hv([old("trail_a"), old("trail_b")]), None), "T": (old("tall_grass"), None)}
    return run(txt(g), lg, (6, 6), "south-east", "RUTA 15", extra=[(9, 7, wm), (5, 9, wm), (8, 4, npc("explorador", "south-west"))])


reg("R15", "Ruta 15", r15)


# ================================================================ CIUDAD AMBAR (warm town by the savanna)
def ambar():
    g = mapof(lambda u, v, a, b: ".", ".")
    P(g, {(-1, 2): "p", (3, 0): "p", (3, 2): "f", (-3, -1): "b", (-2, 0): "l"})
    fl = {(a, b): memo(("aflag", a, b), lambda a=a, b=b: c4.old_flags(a, b)) for a in (0, 1) for b in (0, 1)}
    st = lambda i, j, fr: fl[(i % 2, j % 2)]
    lg = {
        ".": (st, None), "p": (st, lambda i, j, f: memo(("palm", (i + j) % 2), lambda: palm((i + j) % 2))), "f": (st, old("flowerbed")),
        "b": (st, old("bench")), "l": (st, at_f([old("lamp_h0"), old("lamp_h1")], 4)),
    }
    return run(txt(g), lg, (6, 6), "north-east", "CIUDAD AMBAR", extra=[(9, 7, ob("arch", reserve_arch)), (5, 6, npc("guardian", "north-east"))])


reg("AMBAR", "Ciudad Ambar", ambar)


# ================================================================ GIMNASIO VENENO (big light tatami, shoji walls, invisible walls)
def gym():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."
    g = mapof(f, ".")
    for j in range(13):
        for i in range(13):
            if g[j][i] == "W" and (i - 6) > -3:
                g[j][i] = "J"
    tt = {(a, b): memo(("tat", a, b), lambda a=a, b=b: tatami(a, b)) for a in (0, 1) for b in (0, 1)}
    fl = lambda i, j, fr: tt[(i % 2, j % 2)]
    lg = {".": (fl, None), "W": (fl, ob("shoji_i", lambda: shoji("i"))), "J": (fl, ob("shoji_j", lambda: shoji("j")))}
    return run(txt(g), lg, (6, 6), "north-east", "GIMNASIO VENENO", bg=0, extra=[(8, 4, npc("kai", "south-west"))])


reg("GYM", "Gym Veneno", gym)


# ================================================================ AUTOPISTA MAG (elevated downhill road)
def autopista():
    """The elevated downhill road: light pavement, lane dashes, painted downhill chevrons, white guardrails on both edges, the drop dark
    below (bg 3) so it reads as a bridge."""
    def f(u, v, a, b):
        if v == -2 or v == 3:
            return "R"
        if -1 <= v <= 2:
            if v == 0:
                return "-"
            if u in (-2, 1) and v == 1:
                return ">"
            return "="
        return "_"
    g = mapof(f, "_")
    P(g, {(2, -2): "s"})
    rd = ground("roadl", lambda s_: road_light(s_), 2)
    lg = {
        "=": (rd, None), "-": (ob("road_lane", lambda: road_light(0, "lane")), None), ">": (ob("road_arrow", lambda: road_light(0, "arrow")), None),
        "_": (None, None), "R": (rd, ob("grail", guardrail)), "s": (rd, ob("dsign", downhill_sign)),
    }
    return run(txt(g), lg, (6, 6), "south-east", "AUTOPISTA MAG", bg=3, extra=[(8, 6, npc("rider", "north-west")), (5, 7, npc("punk", "north-east"))])


reg("AUTO", "Autopista", autopista)


# ================================================================ RUTA 18 (coast promenade)
def r18():
    def f(u, v, a, b):
        if v >= 2:
            return "w"
        if v in (0, 1):
            return "="
        return "g"
    g = mapof(f, "g")
    P(g, {(-2, 1): "R", (-1, 1): "R", (1, 1): "R", (2, 1): "R", (3, 1): "R", (-3, -1): "p", (2, -2): "p"})
    W = cells_of(g, "w")
    bw = ground("bwalk", boardwalk, 1)
    lg = {
        "g": (GRASS, None), "w": (c2.sea_at(W), None), "=": (bw, None), "R": (bw, old("rail_i")),
        "p": (GRASS, lambda i, j, f: memo(("palm", (i + j) % 2), lambda: palm((i + j) % 2))),
    }
    return run(txt(g), lg, (6, 6), "south-east", "RUTA 18", extra=[(7, 5, npc("chica", "south-west"))], post=c2.gulls((30, 8, 0.7)))


reg("R18", "Ruta 18", r18)


# ================================================================ RESERVA SALVAJE (savanna)
def reserva():
    def f(u, v, a, b):
        if u >= 2 and v >= 1 and u + v <= 6:
            return "w"
        if (u + v) % 3 == 0 and u < 1:
            return "G"
        return "."
    g = mapof(f, ".")
    P(g, {(-3, -1): "F", (-2, -1): "F", (-1, -1): "F", (2, -1): "a", (-1, 2): "a"})
    W = cells_of(g, "w")
    sv = ground("sav", savanna, 3)
    lg = {
        ".": (sv, None), "G": (ground("dgrass", dry_grass, 2), None), "w": (c2.canal_at(W, base=1), None),
        "F": (sv, old("rfence_i")), "a": (sv, lambda i, j, f: memo(("acacia", (i + j) % 2), lambda: acacia((i + j) % 2))),
    }
    return run(txt(g), lg, (6, 6), "south-east", "RESERVA SALVAJE", extra=[(9, 4, old("cottage_0")), (5, 8, npc("explorador", "north-east"))])


reg("RESERVA", "Reserva Salvaje", reserva)


# ================================================================ EL MAR (surfing)
def mar():
    """The open sea with the hero on the Hidrodeslizador (assets/art/hero_surf, PixelLab v3 48 px)."""
    import battle_mock as bm
    real = zm.hero
    def surf_hero(view):
        sp = bm.load(os.path.join(ROOT, "assets/art/hero_surf", view + ".png"))
        return zm.Sp(sp.lv, sp.mk, sp.w // 2, sp.h - 1)
    zm.hero = surf_hero
    try:
        return _mar()
    finally:
        zm.hero = real


def _mar():
    g = mapof(lambda u, v, a, b: "w", "w")
    P(g, {(3, -2): "k", (-3, 2): "k", (2, 3): "b"})
    W = cells_of(g, "wkb")
    lg = {"w": (c2.sea_at(W), None), "k": (c2.sea_at(W), old("sea_rock_a")), "b": (c2.sea_at(W), at_f([old("buoy_0"), old("buoy_1")], 7))}
    return run(txt(g), lg, (6, 6), "south-east", "EL MAR", post=c2.gulls((20, 6, 0.8), (90, 12, 0.5)))


reg("MAR", "El mar", mar)


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
        sheet.paste(lvlib.lcd_image(frs[6], None, scale=3, grid=False), (10 + (n % 2) * (cw + 10), 10 + (n // 2) * (ch + 10)))
    sheet.save(os.path.join(OUT, "sheet.png"))
    print("scenes:", len(scenes), "anim.bin:", os.path.getsize(os.path.join(OUT, "anim.bin")), "bytes")


if __name__ == "__main__":
    write([a.upper() for a in sys.argv[1:]])
