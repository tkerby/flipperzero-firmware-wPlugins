#!/usr/bin/env python3
"""Chapter 3 zone-identity mockups (one per zone): Ruta 5 + guarderia, Pasadizo, Ruta 6, Puerto Dinamo, Gimnasio Voltio, Ferry Albatros
(deck + cabins), Ruta 11, Cueva Excavadora. Same composer / geometry as tools/c2_mock.py (28x14 diamonds, object anchor = bottom vertex of the
front tile, camera on the hero's tile at (64, 32), 4 gray levels 0 white .. 3 black).

The C2 / M4 pieces that already exist are loaded from assets/art/tiles (bollards, crates, barrels, sea, planks, fences, lamps...). The NEW
pieces of C3 live in tools/gen_tiles.py (`c3_pieces()`, approved by the user zone by zone and moved there unchanged, like C2's); this mock loads the
generated files from assets/art/tiles (run `python3 tools/gen_tiles.py` first).

  python3 -I tools/c3_mock.py            # all zones
  python3 -I tools/c3_mock.py R5 PORT     # only some (R5 PAS R6 PORT GYM FERRY CAB R11 CAVE)
writes build/c3_mock/<zone>.gif, sheet.png (2 frames per zone) and anim.bin (the anim_gate format).
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

OUT = os.path.join(ROOT, "build", "c3_mock")
old, done, hv, at_f, actor = zm.old, c2.done, c2.hv, c2.at_f, c2.actor
mapof, P, txt, run, cells_of = c2.mapof, c2.P, c2.txt, c2.run, c2.cells_of
Spr, Box, gtile, dither, in_diamond = gt.Spr, gt.Box, gt.gtile, gt.dither, gt.in_diamond
NF = zm.NF
_M = {}


def memo(key, fn):
    if key not in _M:
        _M[key] = fn()
    return _M[key]


def T(S):
    return done(S, "tile", tile=True, outline=False)


# ================================================================ the pieces of C3
# Drawn ONCE in tools/gen_tiles.py (`c3_pieces()`, written to assets/art/tiles/ and registered in assets/catalog.toml, like C2's); this mock only loads the
# generated files (run `python3 tools/gen_tiles.py` first). The names below are the call signatures the scenes were written with.
def sidewalk(seed=0):
    return old("sidewalk_" + "ab"[seed])


def lawn(seed=0):
    return old("lawn_" + "abc"[seed])


def concrete(seed=0, base=1):
    return old("concrete_" + "abc"[seed])


def quay(seed=0, stripe=False):
    return old("quay_s" if stripe else "quay_" + "ab"[seed])


def metal_big(pi, pj, seed=0):
    return old("metal_%d%d" % (pi, pj))


def carpet(seed=0):
    return old("carpet")


def dirt(seed=0):
    return old("dirt_" + "abc"[seed])


def grate(seed=0):
    return old("grate")


def hedge(seed=0):
    return old("hedge_" + "ab"[seed])


def container(stack=True):
    return old("container")


def crane():
    return old("crane")


def ferry():
    return old("ferry_moored")


def fuse_box(lit=False):
    return old("fuse_box_on" if lit else "fuse_box")


def barrier(phase=0):
    return old("barrier_%d" % phase)


def cable_wall(axis="i"):
    return old("cable_wall_" + axis)


def railing(axis="i"):
    return old("rail_" + axis)


def lifebuoy():
    return old("lifebuoy")


def funnel():
    return old("funnel")


def bunk():
    return old("bunk")


def porthole_wall(axis="i"):
    return old("pwall")


def tile_wall(axis="i"):
    return old("twall")


def light_post():
    return old("light_post")


def gatehouse():
    return old("gatehouse")


def root_wall(seed=0):
    return old("root_wall_" + "ab"[seed])


def mound(seed=0):
    return old("mound_" + "ab"[seed])


def hole():
    return old("hole")


def daycare_sign():
    return old("dc_sign")


def pylon():
    return old("pylon")


def transformer():
    return old("transformer")


def kiosk():
    return old("kiosk")


def lily():
    return old("lily")


# ================================================================ legend helpers
def G(name, n=3, fn=None):
    return memo(name, lambda: [fn(s) for s in range(n)])


def ground(name, fn, n=3):
    return hv(G(name, n, fn))


def ob(name, fn):
    return memo(name, fn)


SCENES = {}


def reg(key, title, fn):
    SCENES[key] = (title, fn)


# ================================================================ RUTA 5 (suburbs, the day care)
def ruta5():
    def f(u, v, a, b):
        if v in (0, 1) and u > -4:
            return "="  # sidewalk along +i
        if v == -1 and u >= -3:
            return "H"  # the trimmed hedge row behind it
        return "."

    g = mapof(f, ".")
    P(
        g,
        {
            (-2, 2): "F",
            (-1, 2): "F",
            (0, 2): "F",
            (1, 2): "F",
            (2, 3): "f",
            (3, 3): "f",
            (2, 2): "M",
            (-1, 3): "w",
            (-3, 2): "t",
            (3, 0): "S",
            (-3, 0): "l",
            (0, -1): ".",
        },
    )
    lg = {
        ".": (ground("lawn", lawn), None),
        "=": (ground("sidewalk", sidewalk, 2), None),
        "H": (ground("lawn", lawn), lambda i, j, f: G("hedge", 2, hedge)[(i + j) % 2]),
        "F": (ground("lawn", lawn), old("picket_i")),
        "f": (ground("lawn", lawn), old("picket_j")),
        "M": (ground("lawn", lawn), old("mailbox")),
        "w": (ground("lawn", lawn), old("flowerbed")),
        "t": (ground("lawn", lawn), old("tree_code")),
        "l": (ground("sidewalk", sidewalk, 2), old("lamp")),
        "S": (ground("sidewalk", sidewalk, 2), ob("dcsign", daycare_sign)),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "RUTA 5",
        extra=[(9, 4, old("fb_house")), (5, 7, actor("cuidadora", "north-east"))],
    )


reg("R5", "Ruta 5", ruta5)


# ================================================================ PASADIZO (underground path)
def pasadizo():
    def f(u, v, a, b):
        if u <= -2 or v <= -2:
            return "W"
        if v >= 2:
            return "_"
        return "."

    g = mapof(f, ".")
    for j in range(13):
        for i in range(13):
            if g[j][i] == "W" and (j - 6) > -2:
                g[j][i] = "J"
    P(
        g,
        {
            (0, 0): ".",
            (2, 0): "g",
            (-1, 1): "L",
            (3, -1): "L",
            (1, -1): "p",
            (4, 1): "g",
        },
    )
    lg = {
        ".": (ground("conc", lambda s: concrete(s, 1)), None),
        "_": (None, None),
        "g": (ob("grate", grate), None),
        "W": (
            ground("conc", lambda s: concrete(s, 1)),
            ob("twall_i", lambda: tile_wall("i")),
        ),
        "J": (
            ground("conc", lambda s: concrete(s, 1)),
            ob("twall_j", lambda: tile_wall("j")),
        ),
        "L": (ground("conc", lambda s: concrete(s, 1)), ob("lpost", light_post)),
        "p": (ground("conc", lambda s: concrete(s, 1)), old("sign")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "PASADIZO",
        bg=3,
        extra=[(9, 6, actor("explorador", "south-west"))],
    )


reg("PAS", "Pasadizo", pasadizo)


# ================================================================ RUTA 6 (a park: pond with lily pads, kiosk, flowerbeds; the closed gate to Cenit)
def ruta6():
    def f(u, v, a, b):
        if u >= 0 and 1 <= v <= 3 and u <= 4:
            return "w"  # the pond, down-right
        if u == -1:
            return "="  # the park walk along +j
        return "."

    g = mapof(f, ".")
    P(
        g,
        {
            (1, 2): "y",
            (3, 1): "y",
            (2, 3): "y",
            (1, -1): "K",
            (-2, 1): "f",
            (-3, 0): "f",
            (2, -1): "b",
            (-2, -2): "t",
            (3, -2): "t",
            (-1, 0): "l",
            (0, 0): ".",
            (4, 0): "f",
        },
    )
    W = cells_of(g, "wy")
    lg = {
        ".": (ground("lawn", lawn), None),
        "=": (ground("sidewalk", sidewalk, 2), None),
        "w": (c2.canal_at(W, base=1), None),
        "y": (c2.canal_at(W, base=1), ob("lily", lily)),
        "K": (ground("lawn", lawn), ob("kiosk", kiosk)),
        "f": (ground("lawn", lawn), old("flowerbed")),
        "b": (ground("lawn", lawn), old("bench")),
        "l": (
            ground("sidewalk", sidewalk, 2),
            at_f([old("lamp_h0"), old("lamp_h1")], 4),
        ),
        "t": (ground("lawn", lawn), old("tree_code")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-west",
        "RUTA 6",
        extra=[(4, 9, ob("gate", gatehouse)), (7, 5, actor("pescador", "south-west"))],
    )


reg("R6", "Ruta 6", ruta6)


# ================================================================ PUERTO DINAMO (industrial port)
def puerto():
    def f(u, v, a, b):
        if v >= 3:
            return "w"  # the harbour water, down-left
        if v == 2:
            return "d"  # the quay edge (planks)
        if u == 1:
            return "|"  # painted safety line
        return "."

    g = mapof(f, ".")
    P(
        g,
        {
            (-2, 2): "o",
            (2, 2): "o",
            (-1, -1): "x",
            (-2, -1): "k",
            (2, -2): "b",
            (-3, 1): "x",
            (0, 1): "k",
        },
    )
    W = cells_of(g, "w")
    lg = {
        ".": (ground("quay", quay, 2), None),
        "|": (ob("quay_s", lambda: quay(5, True)), None),
        "w": (c2.sea_at(W), None),
        "d": (c2.deck_at(lambda i, j: "i", W), None),
        "o": (c2.deck_at(lambda i, j: "i", W), old("bollard")),
        "x": (ground("quay", quay, 2), old("crate_stack")),
        "k": (ground("quay", quay, 2), old("barrel")),
        "b": (ground("quay", quay, 2), old("crate_plain")),
        "h": (ground("quay", quay, 2), None),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "PUERTO DINAMO",
        extra=[
            (9, 6, ob("cont", container)),
            (5, 7, ob("crane", crane)),
            (6, 10, ob("ferry", ferry)),
            (7, 5, actor("trueque", "south-west")),
        ],
        post=c2.gulls((40, 6, 0.6)),
    )


reg("PORT", "Puerto Dinamo", puerto)


# ================================================================ GIMNASIO VOLTIO (fuse-box puzzle)
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
    P(
        g,
        {
            (-2, 1): "u",
            (-1, 2): "u",
            (0, 3): "U",
            (2, -1): "u",
            (3, 0): "u",
            (1, -2): "B",
            (2, -2): "B",
            (0, -2): "B",
        },
    )
    plates = {
        (a, b): memo(("plate", a, b), lambda a=a, b=b: metal_big(a, b))
        for a in (0, 1)
        for b in (0, 1)
    }
    met = lambda i, j, f: plates[(i % 2, j % 2)]
    zap = at_f([ob("bar0", lambda: barrier(0)), ob("bar1", lambda: barrier(1))], 2)
    lg = {
        ".": (met, None),
        "W": (met, ob("cwall_i", lambda: cable_wall("i"))),
        "J": (met, ob("cwall_j", lambda: cable_wall("j"))),
        "u": (met, ob("fuse", fuse_box)),
        "U": (met, ob("fuse_on", lambda: fuse_box(True))),
        "B": (met, zap),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "GIMNASIO VOLTIO",
        extra=[(6, 3, actor("ohm", "south-west"))],
    )


reg("GYM", "Gym Voltio", gym)


# ================================================================ FERRY ALBATROS (deck)
def ferry_deck():
    def f(u, v, a, b):
        if v >= 3 or u >= 4:
            return "s"  # the sea beyond the side
        if v == 2:
            return "R"  # railing along the side
        if u == 3:
            return "r"
        return "="

    g = mapof(f, "=")
    P(
        g,
        {
            (-2, 2): "O",
            (1, 2): "R",
            (3, 2): "s",
            (3, -2): "O",
            (-3, 0): "C",
            (-1, 2): "F",
            (2, 1): "C",
        },
    )
    W = cells_of(g, "s")
    pl = c2.deck_at(lambda i, j: "i", set(), wet=False)
    lg = {
        "=": (pl, None),
        "s": (c2.sea_at(W), None),
        "R": (pl, ob("rail_i", lambda: railing("i"))),
        "r": (pl, ob("rail_j", lambda: railing("j"))),
        "O": (pl, ob("buoy", lifebuoy)),
        "C": (pl, old("crate_plain")),
        "F": (pl, ob("funnel", funnel)),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "FERRY ALBATROS",
        extra=[
            (7, 5, actor("rival", "south-west")),
            (8, 7, actor("marinero", "north-west")),
        ],
        post=c2.gulls((20, 8, 0.9)),
    )


reg("FERRY", "Ferry cubierta", ferry_deck)


def ferry_cabins():
    def f(u, v, a, b):
        if u <= -2 or v <= -2:
            return "W"
        if u >= 4 or v >= 4:
            return "_"
        return "."

    g = mapof(f, ".")
    for j in range(13):
        for i in range(13):
            if g[j][i] == "W" and (i - 6) > -2:
                g[j][i] = "J"
    P(g, {(-1, -1): "k", (1, -1): "k", (3, -1): "t", (-1, 2): "k", (2, 1): "m"})
    cp = ground("carpet", carpet, 1)
    lg = {
        ".": (cp, None),
        "_": (None, None),
        "W": (cp, ob("pwall_i", lambda: porthole_wall("i"))),
        "J": (cp, ob("pwall_j", lambda: porthole_wall("j"))),
        "k": (cp, ob("bunk", bunk)),
        "t": (cp, old("fb_table")),
        "m": (cp, old("mat")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "south-east",
        "FERRY CAMAROTES",
        bg=3,
        extra=[(8, 5, actor("capitan", "south-west"))],
    )


reg("CAB", "Ferry camarotes", ferry_cabins)


# ================================================================ RUTA 11 (the high-voltage line out of Dinamo: pylons, cables, a transformer, dry grass)
def cables(p, f):
    """Screen-space overlay: three sagging high-voltage cables crossing the screen between the pylons' arms, a spark running along them."""
    for k, (y0, sag) in enumerate(((4, 6), (9, 7), (14, 8))):
        for x in range(0, zm.SW):
            t = x / (zm.SW - 1.0)
            y = int(y0 + x * 0.18 + sag * 4 * t * (1 - t))
            c2.px_(p, x, y, 3)
        sx = (f * 4 + k * 40) % zm.SW
        sy = int(y0 + sx * 0.18 + sag * 4 * (sx / 127.0) * (1 - sx / 127.0))
        for dx in (-1, 0, 1):
            c2.px_(p, sx + dx, sy - 1, 0)
            c2.px_(p, sx + dx, sy + 1, 0)


def ruta11():
    def f(u, v, a, b):
        if u in (0, 1):
            return "="  # a gravel service track along +j
        return "g"

    g = mapof(f, "g")
    P(
        g,
        {
            (-2, 0): "s",
            (-1, 1): "s",
            (-2, 2): "s",
            (3, -1): "s",
            (2, 2): "s",
            (-3, -1): "s",
            (2, 1): "T",
            (3, 1): "T",
            (2, 0): "T",
            (-1, -2): "x",
            (-3, 2): "r",
        },
    )
    gv = hv([old("gravel_a"), old("gravel_b"), old("gravel_c")])
    lg = {
        "g": (hv([old("earth_a"), old("earth_c"), old("earth_d")]), None),
        "=": (gv, None),
        "s": (
            hv([old("earth_a"), old("earth_c")]),
            hv([old("dry_bush_a"), old("dry_bush_b")]),
        ),
        "T": (hv([old("earth_a"), old("earth_c")]), old("scrub")),
        "x": (hv([old("earth_a")]), ob("transf", transformer)),
        "r": (hv([old("earth_a")]), old("rock_code")),
    }
    return run(
        txt(g),
        lg,
        (6, 6),
        "north-east",
        "RUTA 11",
        extra=[
            (6, 9, ob("pylon", pylon)),
            (9, 7, ob("pylon", pylon)),
            (7, 4, actor("mecanico", "south-west")),
        ],
        post=cables,
    )


reg("R11", "Ruta 11", ruta11)


# ================================================================ CUEVA EXCAVADORA (earth burrows)
def cueva():
    g = c2.room()
    c2.fillr(g, 4, 4, 9, 9, ".")
    c2.fillr(g, 1, 1, 11, 3, "W")
    c2.fillr(g, 1, 1, 3, 11, "W")
    c2.put(g, {(4, 4): "W", (9, 4): ".", (4, 9): "."})
    c2.put(
        g,
        {(7, 4): "m", (8, 7): "o", (5, 8): "m", (9, 8): "o", (4, 6): "m", (8, 5): "M"},
    )
    for j in range(13):
        for i in range(13):
            if g[j][i] == "_":
                g[j][i] = "."
    df = ground("dirt", dirt, 3)
    lg = {
        ".": (df, None),
        "_": (None, None),
        "W": (df, lambda i, j, f: G("rootw", 2, root_wall)[(i * 3 + j) % 2]),
        "m": (df, lambda i, j, f: G("mound", 2, mound)[(i + j) % 2]),
        "o": (df, ob("hole", hole)),
        "M": (df, old("dig_tools")),
    }
    return run(txt(g), lg, (6, 6), "south-east", "CUEVA EXCAVADORA", bg=3)


reg("CAVE", "Cueva Excavadora", cueva)


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
