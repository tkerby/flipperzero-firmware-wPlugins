#!/usr/bin/env python3
"""Chapter 4 zone-identity mockups (one per zone): Ruta 9, Tunel Oscuro (without / with the Linterna), Pueblo Bruma, Torre de los Recuerdos,
Rutas 7-8, Metropolis Neon, Centro comercial, Casino, Gimnasio Planta, Escondite Vertice. Same composer / geometry as tools/c3_mock.py.
All the pieces load from assets/art/tiles: the C4 ones are drawn in tools/gen_tiles.py `c4_pieces()` (moved there from this mock unchanged; run
`python3 tools/gen_tiles.py` first).

  python3 -I tools/c4_mock.py [KEYS...]     # R9 TUN0 TUN1 BRU TORRE R78 NEON MALL CASINO GYM HIDE
writes build/c4_mock/<zone>.gif, sheet.png and anim.bin (anim_gate format).
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
import c3_mock as c3  # noqa: E402

OUT = os.path.join(ROOT, "build", "c4_mock")
old, done, hv, at_f, actor = zm.old, c2.done, c2.hv, c2.at_f, c2.actor
mapof, P, txt, run, cells_of = c2.mapof, c2.P, c2.txt, c2.run, c2.cells_of
Spr, Box, gtile, dither = gt.Spr, gt.Box, gt.gtile, gt.dither
NF = zm.NF
_M = {}


def memo(key, fn):
    if key not in _M:
        _M[key] = fn()
    return _M[key]


def T(S):
    return done(S, "tile", tile=True, outline=False)


def ground(name, fn, n=3):
    return hv([memo((name, s), lambda s=s: fn(s)) for s in range(n)])


def ob(name, fn):
    return memo(name, fn)


def npc(name, view="south-west"):
    """An NPC from assets/art/npcs; the cut bush is a level-PNG actor (south/f0.lv.png), loaded like a tile piece."""
    d = os.path.join(ROOT, "assets/art/npcs", name)
    if os.path.exists(os.path.join(d, view + ".png")):
        return actor(name, view)
    import json

    im = Image.open(os.path.join(d, "south/f0.lv.png")).convert("RGBA")
    m = json.load(open(os.path.join(d, "south/f0.json")))
    w, h = im.size
    px = im.load()
    lv = [[0] * w for _ in range(h)]
    mk = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b_, a_ = px[x, y]
            if a_ == 255:
                mk[y][x] = True
                lv[y][x] = {255: 0, 170: 1, 85: 2, 0: 3}[g]
    return zm.Sp(lv, mk, m["ax"], m["ay"] + 6, 1, 1)


# ================================================================ the pieces of C4
# Drawn ONCE in tools/gen_tiles.py (`c4_pieces()`, written to assets/art/tiles/ and registered in assets/catalog.toml, like C3's); this mock only loads the
# generated files (run `python3 tools/gen_tiles.py` first). The names below are the call signatures the scenes were written with.
def old_flags(pi, pj, seed=0):
    return old("oflag_%d%d" % (pi, pj))


def asphalt(seed=0, line=False):
    return old("asph_b" if line else "asph_a")


def polished(pi, pj, seed=0):
    return old("polished_%d%d" % (pi, pj))


def old_planks(pi, pj, seed=0):
    return old("oplank_%d%d" % (pi, pj))


def casino_carpet(seed=0):
    return old("ccarpet")


def soil_path(seed=0):
    return old("soil_" + "ab"[seed])


def arrow_tile(d, pi, pj):
    return old(
        "arr_%s_%d%d" % ({"i+": "se", "i-": "nw", "j+": "sw", "j-": "ne"}[d], pi, pj)
    )


def stonewall(axis="i", seed=0):
    return old("stonewall_%s_%s" % (axis, "ab"[seed]))


def tower_far():
    return old("tower_far")


def stele(seed=0):
    return old("stele_" + "ab"[seed])


def candle(phase=0):
    return old("candle_%d" % phase)


def chainlink(axis="i"):
    return old("chain_" + axis)


def road_sign():
    return old("road_sign")


def neon_block(phase=0, sign=0):
    return old("nblock_%d_%d" % (sign, phase))


def holo_screen(phase=0):
    return old("holo_%d" % phase)


def car():
    return old("car")


def vitrina():
    return old("vitrina")


def escalator():
    return old("escalator")


def slot_machine(phase=0):
    return old("slot_%d" % phase)


def stool():
    return old("stool")


def glass_wall(axis="i"):
    return old("glass_" + axis)


def console():
    return old("console")


# ================================================================ overlays
def fog(p, f):
    """Bruma / Torre: drifting bands of mist (white dither) over the scene."""
    for y in range(zm.SH):
        band = math.sin((y + f * 0.6) / 5.0) * 0.5 + 0.5
        d = 0.32 * band
        if d < 0.04:
            continue
        for x in range(zm.SW):
            if dither(x + f, y, d) and p[y][x] in (1, 2):
                p[y][x] = max(0, p[y][x] - 1)


def darkness(radius):
    """Tunel Oscuro: black outside a small diamond around the hero (the screen centre), dithered rim."""

    def post(p, f):
        for y in range(zm.SH):
            for x in range(zm.SW):
                d = abs(x - 64) / 28.0 + abs(y - 30) / 14.0
                if d > radius + 0.35 or (d > radius and dither(x, y, 0.5)):
                    p[y][x] = 3

    return post


def both(*posts):
    def post(p, f):
        for q in posts:
            q(p, f)

    return post


SCENES = {}


def reg(key, title, fn):
    SCENES[key] = (title, fn)


# ================================================================ RUTA 9 (hills, dry-stone walls, cuttable bushes)
def ruta9():
    def f(u, v, a, b):
        if b <= -4:
            return "S"
        if u in (0, 1):
            return "="
        return "."

    g = mapof(f, ".")
    P(
        g,
        {
            (-1, -1): "w",
            (-2, -1): "w",
            (-3, -1): "w",
            (2, 1): "w",
            (3, 1): "w",
            (2, -2): "T",
            (3, -2): "T",
            (-2, 2): "T",
            (-3, 2): "T",
            (1, 3): "c",
            (-1, 2): "r",
            (3, 3): "b",
        },
    )
    ea = hv([old("earth_a"), old("earth_c"), old("earth_d")])
    lg = {
        ".": (c2.GRASS3, None),
        "=": (hv([old("mtrail_a"), old("mtrail_b")]), None),
        "S": (ea, hv([old("slope_a"), old("slope_b")])),
        "w": (
            c2.GRASS3,
            lambda i, j, fr: memo(
                ("sw", (i + j) % 2), lambda: stonewall("i", (i + j) % 2)
            ),
        ),
        "T": (old("tall_grass"), None),
        "r": (c2.GRASS3, old("rock_code")),
        "b": (c2.GRASS3, old("boulder")),
        "c": (c2.GRASS3, None),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "RUTA 9",
        extra=[
            (7, 9, npc("cut_bush")),
            (8, 9, npc("cut_bush")),
            (8, 5, npc("ayudante", "south-west")),
        ],
    )


reg("R9", "Ruta 9", ruta9)


# ================================================================ TUNEL OSCURO (dark cave: without and with the Linterna)
def tunel(lit):
    g = c2.room()
    c2.fillr(g, 4, 4, 9, 9, ".")
    c2.fillr(g, 1, 1, 11, 3, "W")
    c2.fillr(g, 1, 1, 3, 11, "W")
    c2.put(g, {(7, 4): "a", (8, 7): "s", (5, 8): "r", (9, 5): "r", (4, 6): "a"})
    for j in range(13):
        for i in range(13):
            if g[j][i] == "_":
                g[j][i] = "."
    fl = hv([old("cave_a"), old("cave_b"), old("cave_c")])
    wl = [
        at_f([old("cwall_a0"), old("cwall_a1")], 5),
        at_f([old("cwall_b0"), old("cwall_b1")], 5),
    ]
    lg = {
        ".": (fl, None),
        "W": (fl, lambda i, j, fr: wl[(i * 3 + j) % 2](i, j, fr)),
        "a": (fl, old("stalag_a")),
        "s": (fl, old("stalag_s")),
        "r": (fl, hv([old("boulder"), old("rock_code")])),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "TUNEL OSCURO",
        bg=3,
        extra=[(8, 6, npc("montanista", "south-west"))],
        post=None if lit else darkness(1.6),
    )


reg("TUN0", "Tunel sin luz", lambda: tunel(False))
reg("TUN1", "Tunel con Linterna", lambda: tunel(True))


# ================================================================ PUEBLO BRUMA (misty old town, the tower behind)
def bruma():
    g = mapof(lambda u, v, a, b: ".", ".")
    P(g, {(-2, 1): "l", (2, -1): "l", (1, 2): "f", (-3, -1): "f", (3, 1): "b"})
    fl = {
        (a, b): memo(("oflag", a, b), lambda a=a, b=b: old_flags(a, b))
        for a in (0, 1)
        for b in (0, 1)
    }
    cb = lambda i, j, fr: fl[(i % 2, j % 2)]
    lg = {
        ".": (cb, None),
        "l": (cb, at_f([old("lamp_h0"), old("lamp_h1")], 4)),
        "f": (cb, old("flowerbed")),
        "b": (cb, old("bench")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "PUEBLO BRUMA",
        extra=[
            (9, 5, ob("tower", tower_far)),
            (3, 6, old("stone_house")),
            (6, 8, npc("anciano", "north-west")),
        ],
        post=fog,
    )


reg("BRU", "Pueblo Bruma", bruma)


# ================================================================ TORRE DE LOS RECUERDOS (old wood, steles, candles, mist)
def torre():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."

    g = mapof(f, ".")
    P(
        g,
        {
            (-1, -1): "s",
            (1, -1): "s",
            (3, -1): "s",
            (-1, 1): "s",
            (1, 1): "c",
            (3, 1): "s",
            (-1, 3): "c",
            (2, 3): "s",
            (0, -2): "c",
        },
    )
    pl = {
        (a, b): memo(("opl", a, b), lambda a=a, b=b: old_planks(a, b))
        for a in (0, 1)
        for b in (0, 1)
    }
    wd = lambda i, j, fr: pl[(i % 2, j % 2)]
    cnd = at_f([ob("cand0", lambda: candle(0)), ob("cand1", lambda: candle(1))], 3)
    lg = {
        ".": (wd, None),
        "W": (wd, old("wall_i")),
        "s": (
            wd,
            lambda i, j, fr: memo(("stele", (i + j) % 2), lambda: stele((i + j) % 2)),
        ),
        "c": (wd, cnd),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "TORRE RECUERDOS",
        bg=3,
        extra=[(8, 4, npc("medium", "south-west"))],
        post=fog,
    )


reg("TORRE", "Torre", torre)


# ================================================================ RUTAS 7 / 8 (city outskirts)
def r78():
    def f(u, v, a, b):
        if v in (0, 1):
            return "="  # the road along +i
        if v == -1 or v == 2:
            return "-"  # sidewalks
        return "."

    g = mapof(f, ".")
    P(
        g,
        {
            (-2, -2): "F",
            (-1, -2): "F",
            (0, -2): "F",
            (1, -2): "F",
            (2, -2): "F",
            (3, -2): "F",
            (-1, 3): "T",
            (0, 3): "T",
            (1, -1): "s",
            (-3, 2): "l",
            (3, 2): "l",
        },
    )
    lg = {
        ".": (c2.GRASS3, None),
        "=": (ground("asph", lambda s: asphalt(s, s == 1), 2), None),
        "-": (
            ground(
                "sidewalk_",
                lambda s: old("sidewalk_a") if s == 0 else old("sidewalk_b"),
                2,
            ),
            None,
        ),
        "F": (c2.GRASS3, ob("chain_i", lambda: chainlink("i"))),
        "T": (old("tall_grass"), None),
        "s": (
            ground("sidewalk_", lambda s: old("sidewalk_a"), 1),
            ob("rsign", road_sign),
        ),
        "l": (c2.GRASS3, at_f([old("lamp_h0"), old("lamp_h1")], 4)),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "RUTA 7",
        extra=[(5, 8, npc("chica", "north-east"))],
    )


reg("R78", "Rutas 7 y 8", r78)


def bike_scene():
    """Ruta 7 with the hero on the bike (models_dl/pixellab/hero_bike, PixelLab v3 48 px)."""
    import battle_mock as bm

    real = zm.hero

    def bike_hero(view):
        sp = bm.load(os.path.join(ROOT, "models_dl/pixellab/hero_bike", view + ".png"))
        return zm.Sp(sp.lv, sp.mk, sp.w // 2, sp.h - 1)

    zm.hero = bike_hero
    try:
        frs = r78()
    finally:
        zm.hero = real
    return frs


reg("BIKE", "Heroe en bici", bike_scene)


# ================================================================ METROPOLIS NEON (cyberpunk city)
def neon():
    def f(u, v, a, b):
        if v in (1, 2):
            return "="
        return "-"

    g = mapof(f, "-")
    P(g, {(-2, 0): "l", (2, 0): "l", (0, 3): "-", (3, 3): "-"})
    nb = [
        at_f(
            [
                ob(("nb", k, 0), lambda k=k: neon_block(0, k)),
                ob(("nb", k, 1), lambda k=k: neon_block(1, k)),
            ],
            4,
        )
        for k in range(2)
    ]
    lg = {
        "-": (
            ground(
                "sidewalk_",
                lambda s: old("sidewalk_a") if s == 0 else old("sidewalk_b"),
                2,
            ),
            None,
        ),
        "=": (ground("asph", lambda s: asphalt(s, s == 1), 2), None),
        "l": (
            ground("sidewalk_", lambda s: old("sidewalk_a"), 1),
            at_f([old("lamp_h0"), old("lamp_h1")], 4),
        ),
    }
    holo = at_f([ob(("holo", k), lambda k=k: holo_screen(k)) for k in range(3)], 2)
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "METROPOLIS NEON",
        extra=[
            (7, 4, lambda i, j, fr: nb[0](i, j, fr)),
            (3, 5, lambda i, j, fr: nb[1](i, j, fr)),
            (9, 6, holo),
            (6, 8, ob("car", car)),
            (5, 7, npc("explorador", "north-east")),
        ],
        post=c3.cables,
    )


reg("NEON", "Metropolis Neon", neon)


# ================================================================ CENTRO COMERCIAL (big glossy slabs, display cases, escalator)
def mall():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."

    g = mapof(f, ".")
    P(
        g,
        {
            (-1, -1): "v",
            (0, -1): "v",
            (2, 1): "v",
            (2, 2): "v",
            (-2, 2): "p",
            (3, -2): "p",
        },
    )
    slabs = {
        (a, b): memo(("pol", a, b), lambda a=a, b=b: polished(a, b))
        for a in (0, 1)
        for b in (0, 1)
    }
    fl = lambda i, j, fr: slabs[(i % 2, j % 2)]
    lg = {
        ".": (fl, None),
        "W": (fl, old("wall_i")),
        "v": (fl, ob("vit", vitrina)),
        "p": (fl, old("planter")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "CENTRO COMERCIAL",
        bg=0,
        extra=[(9, 6, ob("esc", escalator)), (7, 4, npc("dependienta", "south-west"))],
    )


reg("MALL", "Centro comercial", mall)


# ================================================================ CASINO (carpet, rows of slot machines)
def casino():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."

    g = mapof(f, ".")
    for u in (-2, -1, 1, 2, 3):
        P(g, {(u, -1): "m", (u, 1): "n"})
    P(g, {(-2, 2): "t", (0, 2): "t", (2, 2): "t"})
    cp = ground("ccarp", casino_carpet, 1)
    sm = at_f(
        [
            ob(("slot", 0), lambda: slot_machine(0)),
            ob(("slot", 1), lambda: slot_machine(1)),
        ],
        3,
    )
    sm2 = at_f(
        [
            ob(("slot", 1), lambda: slot_machine(1)),
            ob(("slot", 0), lambda: slot_machine(0)),
        ],
        3,
    )
    lg = {
        ".": (cp, None),
        "W": (cp, old("wall_i")),
        "m": (cp, sm),
        "n": (cp, sm2),
        "t": (cp, ob("stool", stool)),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "CASINO",
        bg=3,
        extra=[
            (5, 8, npc("jugador", "north-east")),
            (8, 4, npc("croupier", "south-west")),
        ],
    )


reg("CASINO", "Casino", casino)


# ================================================================ GIMNASIO PLANTA (greenhouse)
def gym():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        if u == 0 or v == 0:
            return "="
        return "."

    g = mapof(f, ".")
    P(
        g,
        {
            (-2, -1): "f",
            (-1, -2): "f",
            (2, -1): "p",
            (1, -2): "p",
            (-2, 2): "p",
            (2, 2): "f",
            (3, 1): "f",
            (-1, 2): "f",
        },
    )
    lg = {
        ".": (
            (
                old("flowers")
                if os.path.exists(os.path.join(gt.OUT_DIR, "flowers.lv.png"))
                else c2.GRASS3
            ),
            None,
        ),
        "=": (ground("soil", soil_path, 2), None),
        "W": (c2.GRASS3, ob("glass_i", lambda: glass_wall("i"))),
        "f": (c2.GRASS3, old("flowerbed")),
        "p": (c2.GRASS3, old("planter")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "GIMNASIO PLANTA",
        bg=0,
        extra=[(6, 4, npc("cut_bush")), (6, 3, npc("flora", "south-west"))],
    )


reg("GYM", "Gym Planta", gym)


# ================================================================ ESCONDITE VERTICE (steel plates, arrow floors, consoles, crates)
def hideout():
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        if v == 1 and -2 <= u <= 3:
            return ">"
        if u == 2 and -2 <= v <= 0:
            return "^"
        return "."

    g = mapof(f, ".")
    P(g, {(-2, -1): "k", (-1, -2): "x", (3, -2): "k", (-2, 3): "x", (3, 3): "k"})
    plates = {(a, b): old("metal_%d%d" % (a, b)) for a in (0, 1) for b in (0, 1)}
    fl = lambda i, j, fr: plates[(i % 2, j % 2)]
    arr = lambda d: (
        lambda i, j, fr: memo(
            ("arr", d, i % 2, j % 2), lambda: arrow_tile(d, i % 2, j % 2)
        )
    )
    lg = {
        ".": (fl, None),
        ">": (arr("i+"), None),
        "^": (arr("j-"), None),
        "W": (fl, old("cable_wall_i")),
        "k": (fl, ob("console", console)),
        "x": (fl, old("crate_v")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "ESCONDITE VERTICE",
        bg=3,
        extra=[(8, 5, npc("varo", "south-west")), (7, 7, npc("vertice", "north-west"))],
    )


reg("HIDE", "Escondite", hideout)


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
