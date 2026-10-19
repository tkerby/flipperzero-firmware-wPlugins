#!/usr/bin/env python3
"""Chapter 7 zone-identity mockups (Islas Escarcha, Isla Ceniza, the mansion, the fossil lab, the Power
Plant): Rutas 19-21 (the open sea between islands), Islas Escarcha (ice cave: currents + boulders), Isla Ceniza (volcanic town), Gimnasio
Fuego (quiz machines + braziers), Mansion Calcinada (burnt boards + statue switches), Laboratorio (fossil revival), Central Electrica.
Same composer as tools/c6_mock.py; every piece loads from assets/art/tiles (drawn in tools/gen_tiles.py `c7_pieces`, contract 53: run `python3 tools/gen_tiles.py` first).

  python3 -I tools/c7_mock.py [KEYS...] # MAR ESCARCHA CENIZA FUEGO MANSION LAB CENTRAL
writes build/c7_mock/<zone>.gif, sheet.png and anim.bin (anim_gate format).
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

OUT = os.path.join(ROOT, "build", "c7_mock")
old, done, hv, at_f = zm.old, c2.done, c2.hv, c2.at_f
mapof, P, txt, run, cells_of = c2.mapof, c2.P, c2.txt, c2.run, c2.cells_of
Spr, Box, gtile, dither = gt.Spr, gt.Box, gt.gtile, gt.dither
memo, T, ground, ob, npc = c4.memo, c4.T, c4.ground, c4.ob, c4.npc
NF = zm.NF


def big(name, fn):
    """2 x 2 slab quarters by cell parity."""
    q = {(a, b): memo((name, a, b), lambda a=a, b=b: fn(a, b)) for a in (0, 1) for b in (0, 1)}
    return lambda i, j, fr: q[(i % 2, j % 2)]


def big_old(prefix):
    """An existing 2 x 2 slab set prefix_00 .. prefix_11 by cell parity."""
    return lambda i, j, fr: old("%s_%d%d" % (prefix, i % 2, j % 2))


def surfing(fn):
    """Run a scene with the hero on the Hidrodeslizador (assets/art/hero_surf)."""
    import battle_mock as bm
    real = zm.hero

    def surf_hero(view):
        sp = bm.load(os.path.join(ROOT, "assets/art/hero_surf", view + ".png"))
        return zm.Sp(sp.lv, sp.mk, sp.w // 2, sp.h - 1)
    zm.hero = surf_hero
    try:
        return fn()
    finally:
        zm.hero = real


# ================================================================ the pieces of C7
# Drawn ONCE in tools/gen_tiles.py (`c7_pieces()`, written to assets/art/tiles/ and registered in assets/catalog.toml, like C3 - C8's: contract 53, moved there from this mock UNCHANGED, pixel for pixel);
# this mock only loads the generated files (run `python3 tools/gen_tiles.py` first). The names below are the call signatures the scenes were written with.
def ice_slab(pi, pj, seed=0):
    return old("ice_%d%d" % (pi, pj))


def ash(seed=0):
    return old("ash_%d" % seed)


def burnt_boards(pi, pj, seed=0):
    return old("burnt_%d%d" % (pi, pj))


def current(phase=0):
    return old("current_%d" % phase)


def kelp(phase=0):
    return old("kelp_%d" % phase)


def sandbar():
    return old("sandbar")


def ice_pillar(seed=0):
    return old("ice_pillar_%d" % seed)


def snow_drift():
    return old("snow_drift")


def lava_rock(seed=0):
    return old("lava_rock_%d" % seed)


def steam_vent(phase=0):
    return old("steam_vent_%d" % phase)


def volcano():
    return old("volcano")


def brazier(phase=0):
    return old("brazier_%d" % phase)


def quiz_machine():
    return old("quiz_machine")


def statue_switch(on=False):
    return old("statue_on" if on else "statue_off")


def charred_wall(axis="i"):
    return old("charwall")


def rubble():
    return old("rubble_c7")


def revival_machine():
    return old("reviver")


def generator(phase=0):
    return old("generator_%d" % phase)


SCENES = {}


def reg(key, title, fn):
    SCENES[key] = (title, fn)


def room_walls(g, lim=-3):
    for j in range(13):
        for i in range(13):
            if g[j][i] == "W" and (i - 6) > lim:
                g[j][i] = "J"


# ================================================================ RUTAS 19-21 (the open sea between the islands)
def _mar():
    g = mapof(lambda u, v, a, b: "w", "w")
    P(g, {(1, -2): "s", (-3, 2): "k", (2, 3): "b", (-2, 1): "K", (2, 1): "K", (-2, -2): "r"})
    W = cells_of(g, "wkbKsr")
    sea = c2.sea_at(W)
    kp = at_f([ob(("kelp", k), lambda k=k: kelp(k)) for k in (0, 1)], 6)
    lg = {
        "w": (sea, None), "k": (sea, old("sea_rock_a")), "r": (sea, old("sea_rock_b")), "b": (sea, at_f([old("buoy_0"), old("buoy_1")], 7)),
        "K": (sea, kp), "s": (sea, ob("sandbar", sandbar)),
    }
    return run(txt(g), lg, (6, 6), "south-east", "RUTA 20", post=c2.gulls((20, 6, 0.8), (90, 12, 0.5)),
               extra=[(8, 5, npc("nadadora", "south-west"))])


reg("MAR", "Rutas 19-21", lambda: surfing(_mar))


# ================================================================ ISLAS ESCARCHA (ice cave: currents, boulders, holes)
def escarcha():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        if v == 2 and u >= -2:
            return "c"                                       # a current channel across the cave
        return "."
    g = mapof(f, ".")
    room_walls(g)
    P(g, {(1, -1): "o", (2, 1): "h", (-2, 0): "p", (3, -2): "p", (-1, -2): "d", (0, 4): "p"})
    fl = big("ice", ice_slab)
    cur = at_f([memo(("cur", k), lambda k=k: current(k)) for k in (0, 1)], 5)
    lg = {
        ".": (fl, None), "W": (fl, hv([old("cwall_a0"), old("cwall_b0")])), "J": (fl, hv([old("cwall_a1"), old("cwall_b1")])),
        "c": (cur, None), "o": (fl, old("boulder")), "h": (fl, old("hole")),
        "p": (fl, hv([ob(("ipil", s), lambda s=s: ice_pillar(s)) for s in (0, 1)])), "d": (fl, ob("drift", snow_drift)),
    }
    return run(txt(g), lg, (6, 6), "north-east", "ISLAS ESCARCHA", bg=0)


reg("ESCARCHA", "Islas Escarcha", escarcha)


# ================================================================ ISLA CENIZA (volcanic town)
def ceniza():
    g = mapof(lambda u, v, a, b: ".", ".")
    P(g, {(-2, 1): "l", (2, -2): "s", (3, 2): "l", (-3, -1): "t", (1, 3): "s", (-1, -3): "t"})
    ag = ground("ash", ash, 3)
    vent = at_f([ob(("vent", k), lambda k=k: steam_vent(k)) for k in (0, 1, 2)], 4)
    lg = {
        ".": (ag, None), "l": (ag, hv([ob(("lrock", s), lambda s=s: lava_rock(s)) for s in (0, 1)])), "s": (ag, vent),
        "t": (ag, at_f([old("palm_0"), old("palm_1")], 8)),
    }
    return run(txt(g), lg, (6, 6), "south-east", "ISLA CENIZA",
               extra=[(8, 3, ob("volcano", volcano)), (8, 7, npc("cientifico", "south-west"))])


reg("CENIZA", "Isla Ceniza", ceniza)


# ================================================================ GIMNASIO FUEGO (quiz machines + braziers)
def fuego():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."
    g = mapof(f, ".")
    room_walls(g)
    P(g, {(-1, -1): "q", (2, -1): "q", (1, 2): "f", (-2, 2): "f", (3, 1): "f"})
    fl = big_old("slab")
    br = at_f([ob(("braz", k), lambda k=k: brazier(k)) for k in (0, 1)], 3)
    lg = {
        ".": (fl, None), "W": (fl, old("gym_wall_i")), "J": (fl, old("gym_wall_j")), "q": (fl, ob("quiz", quiz_machine)), "f": (fl, br),
    }
    return run(txt(g), lg, (6, 6), "north-east", "GIMNASIO FUEGO", bg=0, extra=[(8, 4, npc("cientifico", "south-west"))])


reg("FUEGO", "Gym Fuego", fuego)


# ================================================================ MANSION CALCINADA (burnt boards, statue switches)
def mansion():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."
    g = mapof(f, ".")
    room_walls(g)
    P(g, {(2, -1): "S", (-1, 2): "r", (3, 2): "r", (-2, -2): "x"})
    fl = big("burnt", burnt_boards)
    lg = {
        ".": (fl, None), "W": (fl, ob("chw_i", lambda: charred_wall("i"))), "J": (fl, ob("chw_j", lambda: charred_wall("j"))),
        "S": (fl, ob("statue", statue_switch)), "r": (fl, ob("rubble", rubble)), "x": (fl, old("crate_plain")),
    }
    return run(txt(g), lg, (6, 6), "north-east", "MANSION CALCINADA", bg=0, extra=[(8, 5, npc("cientifico", "south-west"))])


reg("MANSION", "Mansion Calcinada", mansion)


# ================================================================ LABORATORIO (fossil revival)
def lab():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."
    g = mapof(f, ".")
    room_walls(g)
    P(g, {(2, -1): "R", (-1, -1): "t", (1, 2): "v", (-2, 2): "t"})
    fl = big_old("polished")
    lg = {
        ".": (fl, None), "W": (fl, old("wall_i")), "J": (fl, old("wall_j")), "R": (fl, ob("reviver", revival_machine)),
        "t": (fl, old("fb_lab_table")), "v": (fl, old("vitrina")),
    }
    return run(txt(g), lg, (6, 6), "north-east", "LABORATORIO", bg=0, extra=[(8, 4, npc("cientifico", "south-west"))])


reg("LAB", "Laboratorio", lab)


# ================================================================ CENTRAL ELECTRICA (Power Plant)
def central():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."
    g = mapof(f, ".")
    room_walls(g)
    P(g, {(-1, -1): "G", (2, -1): "G", (2, 2): "x", (-2, 2): "G", (0, 3): "c", (1, 3): "c"})
    fl = big_old("metal")
    gen = at_f([ob(("gen", k), lambda k=k: generator(k)) for k in (0, 1)], 2)
    lg = {
        ".": (fl, None), "W": (fl, old("cable_wall_i")), "J": (fl, old("cable_wall_j")), "G": (fl, gen), "x": (fl, old("transformer")),
        "c": (fl, old("cable_i")),
    }
    return run(txt(g), lg, (6, 6), "north-east", "CENTRAL ELECTRICA", bg=0)


reg("CENTRAL", "Central Electrica", central)


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
