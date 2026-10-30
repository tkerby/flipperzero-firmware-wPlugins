#!/usr/bin/env python3
"""Chapter 9 zone-identity mockups (Senda de Campeones, Meseta Cumbre and the League four rooms, Cueva Secreta):
Ruta 23 (badge-check gates), Senda de Campeones (cave with boulder switches, torches and banners), Meseta Cumbre (plaza with the league
hall), the five league rooms (Hielo, Lucha, Fantasma, Dragon, Campeon) and the post-game Cueva Secreta. Same composer as tools/c7_mock.py.

  python3 -I tools/c9_mock.py [KEYS...] # R23 SENDA MESETA HIELO LUCHA FANTASMA DRAGON CAMPEON CUEVA
writes build/c9_mock/<zone>.gif, sheet.png and anim.bin (anim_gate format).
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

OUT = os.path.join(ROOT, "build", "c9_mock")
old, done, hv, at_f = zm.old, c2.done, c2.hv, c2.at_f
mapof, P, txt, run = c2.mapof, c2.P, c2.txt, c2.run
Spr, Box, gtile, dither = gt.Spr, gt.Box, gt.gtile, gt.dither
memo, T, ground, ob, npc = c4.memo, c4.T, c4.ground, c4.ob, c4.npc
big, big_old = c7.big, c7.big_old


# ================================================================ the pieces of C9
# Drawn ONCE in tools/gen_tiles.py (`c9_pieces()`, contract 56: moved there pixel for pixel, `build/c9/pieces_check.py`), written to assets/art/tiles/ and registered in assets/catalog.toml; this mock only
# loads the generated files (run `python3 tools/gen_tiles.py` first). The names below are the call signatures the scenes were written with.
def rock_floor(seed=0):
    return old("rockf_%d" % seed)


def marble(pi, pj, seed=0):
    return old("marb_%d%d" % (pi, pj))


def tatami_ring(pi, pj, seed=0):
    return old("ringm_%d%d" % (pi, pj))


def ghost_floor(pi, pj, seed=0):
    return old("ghf_%d%d" % (pi, pj))


def crystal_floor(seed=0):
    return old("crys_%d" % seed)


def badge_gate(n=1):
    return old("bgate")


def torch(phase=0):
    return old("torch_%d" % phase)


def banner(axis="i"):
    return old("ban_" + axis)


def statue():
    return old("lstatue")


def ring_post():
    return old("ringpost")


def candelabra(phase=0):
    return old("cand_%d" % phase)


def glow_crystal(seed=0):
    return old("gcry_%d" % seed)


def pressure_plate(down=False):
    """The mock's boulder switch (a flat plate). The game fills a PIT instead (the engine's boulder + hole, contract 42.2): see the `hole_pit` actor; kept for the SENDA scene."""

    def f(x, y, u, v, r):
        if 0.2 < u < 0.8 and 0.2 < v < 0.8:
            if 0.27 < u < 0.73 and 0.27 < v < 0.73:
                return 2 if down else 1
            return 3
        return None

    return T(gtile(f, 1))


def pl_object(name, max_h, dy=4):
    """The PixelLab decor objects of the ice / dragon / champion rooms (assets/art/objects_c9/<name>.png), generated into catalog objects by gen_tiles c9_pieces()."""
    return old(
        {
            "ice_crystals": "icecr",
            "dragon_statue": "dstat",
            "champion_column": "cpil",
            "champion_trophy": "troph",
        }[name]
    )


SCENES = {}


def reg(key, title, fn):
    SCENES[key] = (title, fn)


def ice_slab(pi, pj, seed=0):
    return old("iceslab_%d%d" % (pi, pj))


def room_walls(g, lim=-3):
    for j in range(13):
        for i in range(13):
            if g[j][i] == "W" and (i - 6) > lim:
                g[j][i] = "J"


def room(floor, wall_i, wall_j, items, title, extra=(), bg=0):
    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        return "."

    g = mapof(f, ".")
    room_walls(g)
    P(g, {k: c for k, (c, _) in items.items()})
    lg = {".": (floor, None), "W": (floor, wall_i), "J": (floor, wall_j)}
    for k, (c, piece) in items.items():
        lg[c] = (floor, piece)
    return run(txt(g), lg, (6, 6), "north-east", title, bg=bg, extra=list(extra))


# ================================================================ RUTA 23 (badge-check gates)
def r23():
    g = mapof(lambda u, v, a, b: "g" if (u + v) % 7 else "p", "g")
    P(g, {(1, -2): "G", (-2, 1): "G", (3, 2): "t", (-3, -1): "t"})
    grass = hv([old("grass_a"), old("grass_b"), old("grass_c")])
    path = hv([old("path_a"), old("path_b")])
    lg = {
        "g": (grass, None),
        "p": (path, None),
        "G": (path, ob("bgate", badge_gate)),
        "t": (grass, old("tree_code")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "RUTA 23",
        extra=[(8, 5, npc("guardia", "south-west"))],
    )


reg("R23", "Ruta 23", r23)


# ================================================================ SENDA DE CAMPEONES (Senda de Campeones)
def senda():
    g = mapof(lambda u, v, a, b: "W" if (u <= -3 or v <= -3) else ".", ".")
    room_walls(g)
    P(
        g,
        {
            (1, -1): "o",
            (2, 1): "s",
            (-2, 2): "S",
            (3, -2): "f",
            (-1, 3): "f",
            (-3, 0): "b",
        },
    )
    fl = ground("rockf", rock_floor, 3)
    lg = {
        ".": (fl, None),
        "W": (fl, hv([old("cwall_a0"), old("cwall_b0")])),
        "J": (fl, hv([old("cwall_a1"), old("cwall_b1")])),
        "o": (fl, old("boulder")),
        "s": (memo("plate0", lambda: pressure_plate(False)), None),
        "S": (memo("plate1", lambda: pressure_plate(True)), old("boulder")),
        "f": (fl, at_f([ob(("torch", k), lambda k=k: torch(k)) for k in (0, 1)], 3)),
        "b": (fl, old("stalag_a")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "SENDA DE CAMPEONES",
        bg=0,
        extra=[(8, 4, npc("elite", "south-west"))],
    )


reg("SENDA", "Senda de Campeones", senda)


# ================================================================ MESETA CUMBRE (plaza + league hall)
def meseta():
    g = mapof(lambda u, v, a, b: ".", ".")
    P(g, {(1, -1): "S", (3, 2): "l", (-2, 2): "l", (-2, -1): "f", (3, -1): "f"})
    fl = big("marb", marble)
    lg = {
        ".": (fl, None),
        "S": (fl, ob("statue", statue)),
        "l": (fl, at_f([old("lamp_h0"), old("lamp_h1")], 4)),
        "f": (fl, at_f([old("vflag_0"), old("vflag_1")], 5)),
    }
    return run(txt(g), lg, (6, 6), "south-east", "MESETA CUMBRE")


reg("MESETA", "Meseta Cumbre", meseta)


# ================================================================ the five league rooms
def hielo():
    return room(
        big("ice", ice_slab),
        old("wall_i"),
        old("wall_j"),
        {
            (-1, -1): ("p", ob("icecr", lambda: pl_object("ice_crystals", 34))),
            (3, 1): ("p", ob("icecr", lambda: pl_object("ice_crystals", 34))),
            (2, 2): ("d", ob("drift", lambda: old("snowdrift"))),
        },
        "SALA HIELO",
        extra=[(8, 4, npc("iver", "south-west"))],
    )


def lucha():
    return room(
        big("ringm", tatami_ring),
        old("wall_i"),
        old("wall_j"),
        {
            (-1, -1): ("r", ob("rpost", ring_post)),
            (2, 2): ("r", ob("rpost", ring_post)),
        },
        "SALA LUCHA",
        extra=[(8, 4, npc("tanko", "south-west"))],
    )


def fantasma():
    def post(p, f):
        for y in range(zm.SH):
            for x in range(zm.SW):
                if (
                    ((x + f * 2 + (y // 6) * 9) % 23) < 3
                    and dither(x, y, 0.5)
                    and p[y][x] == 0
                ):
                    p[y][x] = 1

    def build():
        g = mapof(lambda u, v, a, b: "W" if (u <= -3 or v <= -3) else ".", ".")
        room_walls(g)
        P(g, {(-1, -1): "c", (2, 1): "c", (-2, 2): "c"})
        fl = big("ghf", ghost_floor)
        cd = at_f([ob(("cand", k), lambda k=k: candelabra(k)) for k in (0, 1)], 3)
        lg = {
            ".": (fl, None),
            "W": (fl, old("wall_i")),
            "J": (fl, old("wall_j")),
            "c": (fl, cd),
        }
        return run(
            txt(g),
            lg,
            (6, 6),
            "north-east",
            "SALA FANTASMA",
            bg=0,
            post=post,
            extra=[(8, 4, npc("madame_velo", "south-west"))],
        )

    return build()


def dragon():
    return room(
        big("marb", marble),
        old("wall_i"),
        old("wall_j"),
        {
            (-1, -1): ("d", ob("dstat2", lambda: pl_object("dragon_statue", 44))),
            (3, 1): ("d", ob("dstat2", lambda: pl_object("dragon_statue", 44))),
        },
        "SALA DRAGON",
        extra=[(8, 4, npc("arkan", "south-west"))],
    )


def campeon():
    return room(
        big("marb", marble),
        ob("ban_i", lambda: banner("i")),
        ob("ban_j", lambda: banner("j")),
        {
            (-1, -1): ("p", ob("cpil2", lambda: pl_object("champion_column", 44))),
            (3, 1): ("p", ob("cpil2", lambda: pl_object("champion_column", 44))),
            (-1, 2): ("t", ob("troph", lambda: pl_object("champion_trophy", 30))),
            (2, 2): ("t", ob("troph", lambda: pl_object("champion_trophy", 30))),
        },
        "SALA DEL CAMPEON",
        extra=[(8, 4, npc("rival", "south-west"))],
    )


reg("HIELO", "Sala Hielo (Iver)", hielo)
reg("LUCHA", "Sala Lucha (Tanko)", lucha)
reg("FANTASMA", "Sala Fantasma (Velo)", fantasma)
reg("DRAGON", "Sala Dragon (Arkan)", dragon)
reg("CAMPEON", "Sala del Campeon", campeon)


# ================================================================ CUEVA SECRETA (post-game)
def cueva():
    g = mapof(lambda u, v, a, b: "W" if (u <= -3 or v <= -3) else ".", ".")
    room_walls(g)
    P(g, {(1, -1): "c", (-2, 1): "c", (3, 2): "c", (-1, 3): "w"})
    fl = ground("crys", crystal_floor, 3)
    gc = hv([ob(("gcry", s), lambda s=s: glow_crystal(s)) for s in (0, 1)])
    lg = {
        ".": (fl, None),
        "W": (fl, hv([old("cwall_a0"), old("cwall_b0")])),
        "J": (fl, hv([old("cwall_a1"), old("cwall_b1")])),
        "c": (fl, gc),
        "w": (fl, old("stalag_s")),
    }
    return run(txt(g), lg, (6, 6), "north-east", "CUEVA SECRETA", bg=0)


reg("CUEVA", "Cueva Secreta", cueva)


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
