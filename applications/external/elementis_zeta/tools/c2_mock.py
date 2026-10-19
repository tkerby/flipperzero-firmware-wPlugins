#!/usr/bin/env python3
"""Chapter 2 zone-identity mockups: Ruta 3, Monte Eco, Ruta 4, Ciudad Marea, Gimnasio de Agua, Rutas 24-25, two variants (A/B) each.
Same composer / geometry as tools/zone_mock.py + tools/town_mock.py (28x14 diamonds, object anchor = bottom vertex of the front tile,
camera on the hero's tile at (64, 32), 4 gray levels: 0 white .. 3 black).

The drawings of the pieces live in tools/gen_tiles.py (`c2_pieces()`, written to assets/art/tiles/ and registered in assets/catalog.toml):
the approved look of every zone is variant A and the game's maps use those very files. This mock only loads them (like zone_mock /
town_mock; run `python3 tools/gen_tiles.py` first). The B variants were the alternative the user did not pick; the few pieces only B used
(mine timber, radio mast, the object ledges) are not part of the game any more: B scenes keep what they can draw with the shipped pieces.
A combination of edges that no map needs (a sea tile with foam on four sides ...) is drawn in memory with the same gen_tiles function.

  python3 -I tools/c2_mock.py            # all scenes
  python3 -I tools/c2_mock.py eco ruta3  # only some zones (R3 ECO R4 MAR GYM R2425)
writes build/c2_mock/<zone>_<A|B>.gif, sheet.png (2 frames per scene) and anim.bin (the anim_gate format of zone_mock / town_mock).
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

OUT = os.path.join(ROOT, "build", "c2_mock")
Sp = zm.Sp
old = zm.old
NF = zm.NF


# ---------------------------------------------------------------- loading the game's pieces
def done(S, kind="object", fw=1, fh=1, tile=False, outline=True):
    """gt.finish() without the file: outline, crop to the bbox (tile=True keeps the 28x15 canvas, anchor (14,7)) and return a Sp."""
    if outline:
        S.outline(3)
    if tile:
        x0, y0, x1, y1 = 0, 0, S.W - 1, S.H - 1
        ax, ay = S.ox, S.oy
    else:
        x0, y0, x1, y1 = S.bbox()
        ax, ay = S.ox - x0, S.oy - y0
    lv = [row[x0:x1 + 1] for row in S.lv[y0:y1 + 1]]
    mk = [row[x0:x1 + 1] for row in S.mk[y0:y1 + 1]]
    return Sp(lv, mk, ax, ay, fw, fh)


def capture(fn, *a, **k):
    """Run one of gen_tiles' drawing functions with finish() redirected to memory (a variant no map needs, so no file exists)."""
    out = []
    orig = gt.finish

    def cap(name, S, kind, fw, fh, tile=False, outline=True):
        out.append(done(S, kind, fw, fh, tile, outline))
    gt.finish = cap
    try:
        fn(*a, **k)
    finally:
        gt.finish = orig
    return out[0]


def piece(name, make=None, *a, **k):
    """The generated file when it exists, else the same drawing made in memory."""
    if os.path.exists(os.path.join(gt.OUT_DIR, name + ".lv.png")):
        return old(name)
    return capture(make, name, *a, **k)


def hv(opts):
    """Variant by a hash of the cell (the game's catalog [variants] does the same; the mock with a fixed rule)."""
    return lambda i, j, f: opts[(i * 7 + j * 3 + (i * j) % 5) % len(opts)]


def at_f(opts, per=6):
    """Animated piece: opts[(f // per) % len]."""
    return lambda i, j, f: opts[(f // per) % len(opts)]


def actor(name, view="south-west"):
    """An NPC from assets/art/npcs/<name>/<view>.png standing on the CENTRE of its tile (like the hero: foot 1 px under the centre)."""
    s = zm.npc("assets/art/npcs/%s/%s.png" % (name, view))
    return Sp(s.lv, s.mk, s.ax, s.ay + 6, 1, 1)


def tag(edges):
    return gt.edge_tag(edges)


def edges_of(i, j, W, inside):
    return tuple(k for k, d in (("tl", (i - 1, j)), ("tr", (i, j - 1)), ("br", (i + 1, j)), ("bl", (i, j + 1))) if (d in W) == inside)


_CACHE = {}


def water_at(prefix, W, make, per=6, base=None):
    """Animated water (sea / canal / pool): the tile's edges are the land sides (neighbours not in W); 2 frames."""
    def f(i, j, fr):
        e = edges_of(i, j, W, False)
        k = (fr // per) % 2
        key = (prefix, e, k)
        if key not in _CACHE:
            ph = ((0, 2) if prefix == "sea" else (0, 4))[k]
            kw = {} if base is None else {"base": base}
            _CACHE[key] = piece("%s_%s_%d" % (prefix, tag(e), k), make, ph, e, **kw)
        return _CACHE[key]
    return f


def sea_at(W, per=5):
    return water_at("sea", W, gt.sea_tile, per)


def canal_at(W, per=6, base=2):
    return water_at("canal" if base == 2 else "pool_w", W, gt.canal_tile, per, base=base)


def deck_at(axis_of, water, seeds=2, wet=True):
    """Deck ground: planks along +i; edges follow the neighbouring WATER cells."""
    def f(i, j, fr):
        e = edges_of(i, j, water, True)
        s = (i * 3 + j) % seeds
        key = ("deck", e, s, wet)
        if key not in _CACHE:
            _CACHE[key] = piece(("deck_%s_%d" if wet else "plank_%s_%d") % (tag(e), s) if wet else "plank_%s" % tag(e), gt.deck_tile, "i", e, s, wet=wet)
        return _CACHE[key]
    return f


def walk_at(water):
    def f(i, j, fr):
        e = edges_of(i, j, water, True)
        key = ("walk", e)
        if key not in _CACHE:
            _CACHE[key] = piece("walk_" + tag(e), gt.walk_tile, e)
        return _CACHE[key]
    return f


def room(w=13, h=13, fill="_"):
    return [[fill] * w for _ in range(h)]


def put(g, cells, ch=None):
    """Write chars into a grid made by room(): put(g, {(i, j): 'c'}) or put(g, [(i, j), ...], 'c')."""
    items = cells.items() if isinstance(cells, dict) else [(c, ch) for c in cells]
    for (i, j), c in items:
        g[j][i] = c


def fillr(g, i0, j0, i1, j1, ch):
    for j in range(j0, j1 + 1):
        for i in range(i0, i1 + 1):
            g[j][i] = ch


def txt(g):
    return "\n".join("".join(r) for r in g)


def run(grid, legend, hero_at, view, name, bg=0, extra=(), post=None, banner=True):
    out = []
    for f in range(NF):
        ex = []
        for i, j, o in extra:
            ex.append((i, j, o))
        p = zm.compose(grid, legend, hero_at, zm.hero(view), bg=bg, f=f, extra=ex)
        if post:
            post(p, f)
        if banner:
            zm.banner(p, name, f)
        out.append(p)
    return out


def px_(p, x, y, v):
    if 0 <= x < zm.SW and 0 <= y < zm.SH:
        p[y][x] = v


def gulls(*paths):
    """Screen-space overlay: little 'v' gulls gliding across, flapping every 4 frames. paths = (x0, y0, dx_per_frame)."""
    def post(p, f):
        for x0, y0, dx in paths:
            x = int(x0 + dx * f) % (zm.SW + 20) - 10
            y = y0 + int(2 * math.sin(f / 5.0 + x0))
            up = (f // 4) % 2 == 0
            for k in range(1, 4):
                px_(p, x - k, y - (k if up else 0) + (0 if up else k // 2), 3)
                px_(p, x + k, y - (k if up else 0) + (0 if up else k // 2), 3)
            px_(p, x, y, 3)
    return post


SCENES = {}        # (zone key, variant) -> (anim.bin name, builder)


def reg(key, title, fn):
    for v in "AB":
        SCENES[(key, v)] = ("%s %s" % (title, v), (lambda fn=fn, v=v: fn(v)))


def anim(*names):
    return at_f([old(n) for n in names], 5)


# Maps are written in hero-relative cells (u, v) = (i - 6, j - 6); on screen a cell sits at x = 64 + 14 (u - v), y = 32 + 7 (u + v),
# so  u - v = a  is the horizontal slot and  u + v = b  the vertical one: a band of constant b is a horizontal strip of the screen,
# a line of constant v runs down-right (along +i), a line of constant u runs down-left (along +j).
def mapof(fn, dflt="g"):
    g = room(13, 13, dflt)
    for j in range(13):
        for i in range(13):
            u, v = i - 6, j - 6
            c = fn(u, v, u - v, u + v)
            if c:
                g[j][i] = c
    return g


def P(g, items):
    """Put chars at hero-relative cells: P(g, {(u, v): 'c'})."""
    for (u, v), c in items.items():
        g[6 + v][6 + u] = c


def cells_of(g, chars):
    return {(i, j) for j, row in enumerate(g) for i, ch in enumerate(row) if ch in chars}


# ================================================================ RUTA 3 (mountain trail)
def r3_legend():
    ea = hv([old(n) for n in ("earth_a", "earth_b", "earth_c", "earth_d", "earth_b", "earth_c")])
    tr = hv([old("mtrail_a"), old("mtrail_b")])
    sl = hv([old("slope_a"), old("slope_b"), old("slope_c")])
    spot = old("trainer_spot")
    return {
        ".": (ea, None), "=": (tr, None), "S": (ea, sl),
        "t": (ea, old("tree_code")),
        "i": (ea, old("rfence_i")), "j": (ea, old("rfence_j")),
        "b": (ea, hv([old("dry_bush_a"), old("dry_bush_b")])), "r": (ea, old("rock_code")), "B": (ea, old("boulder")),
        "s": (ea, old("scree")), "c": (ea, old("cairn")), "g": (ea, old("sign")),
        "1": (spot, None), "2": (spot, None), "3": (spot, None),
    }


def ruta3(variant):
    lg = r3_legend()
    if variant == "A":
        # the trail runs along +j (from the upper-right to the lower-left of the screen), mountain slopes close in on both sides
        def f(u, v, a, b):
            if b <= -3 or u <= -3 and v >= -3 and b <= 1:
                return "S"
            if u in (-1, 0):
                return "="
            return "."
        g = mapof(f, ".")
        P(g, {(2, -1): "1", (-2, 1): "2", (1, 3): "3", (1, 2): "c", (3, 0): "j", (3, 1): "j", (3, 2): "j", (3, 3): "j", (-3, 2): "b", (-2, -2): "b",
              (1, -2): "B", (2, 2): "r", (3, -2): "t", (-1, 4): "s", (-2, 3): "b", (4, 0): "S", (4, 1): "S", (2, 4): "r"})
        return run(txt(g), lg, (6, 6), "north-east", "RUTA 3",
                   extra=[(8, 5, actor("explorador", "south-west")), (4, 7, actor("cazabichos", "north-east")),
                          (7, 9, actor("aprendiz", "north-west"))])
    # B: the trail runs along +i (from the upper-left to the lower-right); a wooden fence and the slope above, boulders below
    def f(u, v, a, b):
        if v <= -3 or b <= -3:
            return "S"
        if v in (0, 1):
            return "="
        if v == -1 and u >= -2:
            return "i"
        return "."
    g = mapof(f, ".")
    P(g, {(2, -2): "S", (-3, 0): "S", (-3, -1): "S", (-2, 2): "b", (-1, 3): "r", (3, 3): "B", (2, 2): "b", (1, 3): "r", (-3, 3): "S", (0, 4): "s",
          (3, -1): "b", (-1, 2): "s", (3, 1): "c", (-3, 1): "b", (2, -1): "1", (-2, 2): "2", (1, -1): "i", (1, 3): "3", (-2, 3): "r"})
    return run(txt(g), lg, (6, 6), "north-east", "RUTA 3", extra=[(8, 5, actor("explorador", "south-west")), (4, 7, actor("cazabichos", "north-east")),
                                                                   (7, 9, actor("aprendiz", "north-west"))])


reg("R3", "R3", ruta3)


# ================================================================ MONTE ECO (cave, Monte Eco)
def eco_legend(variant):
    fl = hv([old(n) for n in ("cave_a", "cave_b", "cave_c", "cave_a", "cave_b")])
    wl = [at_f([old("cwall_a0"), old("cwall_a1")], 5), at_f([old("cwall_b0"), old("cwall_b1")], 5)]
    wp = [old("cwall_p")]
    return {
        ".": (fl, None), "_": (None, None),
        "W": (fl, lambda i, j, f: wl[(i * 3 + j) % 2](i, j, f)),
        "w": (fl, lambda i, j, f: wp[0]),
        "a": (fl, old("stalag_a")), "A": (fl, old("stalag_b")), "s": (fl, old("stalag_s")),
        "c": (fl, at_f([old("crystal_0"), old("crystal_1")], 6)),
        "L": (fl, old("rope_ladder")), "k": (fl, old("crate_stack")), "x": (fl, old("crate_v")), "y": (fl, old("crate_plain")),
        "d": (fl, old("dig_tools")), "F": (old("fossil_dig"), None), "v": (fl, at_f([old("vflag_0"), old("vflag_1")], 5)),
        "o": (fl, old("barrel")), "r": (fl, hv([old("boulder"), old("rock_code")])),
        "m": (old("moss"), None),
    }


def monte_eco(variant):
    lg = eco_legend(variant)
    g = room()
    if variant == "A":
        fillr(g, 4, 4, 9, 9, ".")                       # the floor
        fillr(g, 1, 1, 11, 3, "W")                      # back walls (up-right side)
        fillr(g, 1, 1, 3, 11, "W")                      # left walls (up-left side)
        fillr(g, 4, 10, 10, 10, "r")                    # rubble where the cave drops away
        fillr(g, 10, 4, 10, 9, "r")
        put(g, {(4, 4): "W", (5, 4): "W", (9, 4): ".", (10, 3): "W", (4, 5): ".", (9, 9): ".", (10, 10): "_"})
        put(g, {(6, 4): "a", (8, 5): "c", (5, 8): "L", (8, 8): "k", (9, 8): "x", (9, 6): "s", (7, 9): "F", (8, 9): "F", (5, 9): "o", (9, 7): "v",
                (4, 7): "s"})
        return run(txt(g), lg, (6, 6), "north-east", "MONTE ECO", bg=3, extra=[(8, 7, actor("guardia", "south-west"))])
    fillr(g, 4, 4, 9, 9, ".")
    fillr(g, 1, 1, 12, 3, "W")
    fillr(g, 1, 1, 3, 12, "W")
    fillr(g, 4, 10, 10, 10, "r")
    fillr(g, 10, 4, 10, 9, "r")
    put(g, {(7, 3): ".", (8, 3): ".", (6, 3): ".", (9, 4): "W", (9, 5): "W", (4, 9): "W", (4, 8): "w", (3, 4): ".", (3, 5): "."})
    put(g, {(4, 6): "c", (5, 4): "s", (8, 6): "d", (8, 8): "k", (5, 7): "y", (9, 9): "x", (7, 9): "F", (6, 9): "F", (9, 8): "o", (4, 4): "A"})
    return run(txt(g), lg, (6, 6), "north-east", "MONTE ECO", bg=3, extra=[(8, 7, actor("guardia", "north-east"))])


reg("ECO", "Eco", monte_eco)


# ================================================================ coast scenes
GRASS3 = hv([old("grass_a"), old("grass_a"), old("grass_b"), old("grass_c")])


def ruta4(variant):
    A = variant == "A"

    def f(u, v, a, b):
        if A:
            if b <= -3 or (u <= -3 and b <= 0):
                return "C"                                  # the mountain's last cliffs, up-left and across the top
            if v >= 3 or b >= 3:
                return "w"                                  # the sea: the whole bottom-left and a strip along the bottom
            if v == 2 or b == 2:
                return "s"                                  # the beach
            if v == 1 and -4 <= u <= 3:
                return "p" if u == 2 else "L"               # one-way ledges going down-left, a gap in the line
            return "g"
        if v <= -3:
            return "C"
        if u >= 2:
            return "w"                                      # the sea, a big triangle at the bottom-right
        if u == 1:
            return "s"
        if u == 0 and -2 <= v <= 4 and v != 0:
            return "l"                                      # one-way ledges going down-right, the hero stands in the gap
        return "g"
    g = mapof(f)
    if A:
        P(g, {(-1, -1): "r", (1, -2): "B", (3, -1): "T", (2, 0): "r", (-1, 2): "q", (0, 2): "b", (1, 1): "b", (-2, 4): "S", (-1, 4): "Z", (3, 0): "b",
              (-3, 0): "T", (1, 1): "p", (-3, 3): "S", (1, 3): "Z"})
        for u in range(-4, 4):                              # the landing row under the ledges carries the rock face
            if g[6 + 2][6 + u] == "s":
                g[6 + 2][6 + u] = "F"
    else:
        P(g, {(-2, 0): "r", (-1, -2): "B", (-3, 2): "T", (1, 2): "b", (1, -1): "b", (2, 0): "S", (3, 1): "Z", (-1, -1): "G", (1, 0): "p", (1, -2): "q",
              (3, -1): "S"})
    W = cells_of(g, "wSZ")
    seaf = sea_at(W)
    sand_ = hv([old("sand_a"), old("sand_b"), old("sand_c"), old("sand_d")])
    lg = {
        "g": (GRASS3, None), "s": (sand_, None), "w": (seaf, None), "S": (seaf, hv([old("sea_rock_a")])), "Z": (seaf, at_f([old("sailboat_0"), old("sailboat_1")], 8)),
        "C": (GRASS3, hv([old("cliff_grass_a"), old("cliff_grass_b"), old("cliff_grass_c")])),
        "L": (old("ledge_lip_sw"), None), "F": (old("ledge_foot_sw"), None), "l": (old("ledge_lip_se"), None),
        "p": (hv([old("path_a"), old("path_b")]), None),
        "r": (GRASS3, old("rock_code")), "B": (GRASS3, old("boulder")), "b": (sand_, hv([old("beach_grass_a"), old("beach_grass_b")])),
        "T": (GRASS3, old("bush")), "q": (sand_, old("log")), "G": (GRASS3, old("sign")),
    }
    post = gulls((10, 10, 0.9), (70, 17, 0.6)) if A else gulls((20, 12, 0.7))
    return run(txt(g), lg, (6, 6), "south-west" if A else "south-east", "RUTA 4", post=post)


reg("R4", "R4", ruta4)


def marea(variant):
    A = variant == "A"
    if A:
        def f(u, v, a, b):
            if v >= 2:
                return "w"                              # the canal (along +i), down-left of the hero
            if v == 1:
                return "d"                              # the boardwalk
            return "s"
        g = mapof(f, "s")
        P(g, {(0, 3): "B", (2, 0): "m", (-2, 1): "o", (1, 1): "o", (3, 1): "N", (-2, 0): "A", (1, -1): "e", (-1, 1): "x", (-2, -1): "H"})
        hero, view = (6, 6), "south-east"
        extra = [(8, 4, old("fb_shop"))]
    else:
        def f(u, v, a, b):
            if abs(u) <= 1 and v != 0:
                return "w"                              # the canal (along +j) under the bridge
            if v == 0 and abs(u) <= 1:
                return "d"
            return "s"
        g = mapof(f, "s")
        P(g, {(0, 0): "K", (-1, 0): "K", (1, 0): "K", (2, 0): "m", (-2, 0): "m", (0, 2): "V", (0, -2): "V", (-2, 2): "o", (2, 2): "o", (2, -2): "A",
              (-3, 1): "N", (3, 2): "x"})
        hero, view = (6, 6), "north-east"
        extra = [(10, 6, old("fb_center"))]
    W = cells_of(g, "wBV")
    watf = canal_at(W)
    wetf = hv([old("wet_a"), old("wet_b"), old("wet_c")])
    dk = deck_at(lambda i, j: "i", W)
    lamp = at_f([old("lamp_h0"), old("lamp_h1")], 4)
    boat = at_f([old("boat_0"), old("boat_1")], 8)
    boatj = at_f([old("boat_j0"), old("boat_j1")], 8)
    lg = {
        "s": (wetf, None), "w": (watf, None), "d": (dk, None),
        "K": (dk, old("bridge_rails_i")),
        "B": (watf, boat), "V": (watf, boatj),
        "o": (wetf, old("bollard")), "m": (wetf, lamp), "A": (wetf, old("anchor")), "N": (wetf, old("net_rack")),
        "e": (wetf, old("trap")), "x": (wetf, old("barrel")), "H": (wetf, old("crate_plain")),
    }
    return run(txt(g), lg, hero, view, "CIUDAD MAREA", extra=extra, post=gulls((30, 8, 0.7)))


reg("MAR", "Marea", marea)


def gimnasio(variant):
    A = variant == "A"

    def f(u, v, a, b):
        if u <= -3 or v <= -3:
            return "W"
        if A:
            if -2 <= u <= 4 and -2 <= v <= 2:
                return "=" if v == 0 else "w"
        else:
            if -2 <= u <= 4 and -2 <= v <= 3:
                if (v == 0 and 0 <= u <= 2) or (u == 0 and 0 <= v <= 3) or (u == 2 and -1 <= v <= 0):
                    return "="
                return "w"
        return "."
    g = mapof(f, ".")
    for j in range(13):                                  # wall faces: the row along +i shows the emblem
        for i in range(13):
            if g[j][i] == "W" and (i - 6) > -3:
                g[j][i] = "J"
    if A:
        P(g, {(-2, 0): "=", (-1, 0): "=", (-3, 1): "D", (1, -1): "R", (2, -1): "R", (3, -1): "R", (1, 1): "R", (2, 1): "R", (3, 1): "R",
              (4, 0): "=", (0, -2): "u", (2, 2): "u", (-2, 2): "u", (4, 2): "f", (-1, 3): "f", (0, 3): "f", (3, 3): "x", (4, -2): "h"})
        extra = [(10, 6, actor("lin", "south-west"))]
        view = "south-east"
    else:
        P(g, {(1, 1): "u", (-1, 2): "u", (-2, -1): "u", (3, 2): "u", (1, 3): "u", (2, 1): "u", (4, 1): "h", (-1, 4): "x", (-2, 3): "f",
              (3, -1): "R", (4, -1): "R", (3, 0): "w"})
        extra = [(8, 5, actor("lin", "south-west"))]
        view = "east"
    W = cells_of(g, "wuRDx")
    watf = canal_at(W, base=1)
    floor = hv([old("pool_a"), old("pool_b")])
    wk = walk_at(W)
    lg = {
        ".": (floor, None), "w": (watf, None), "=": (wk, None), "W": (floor, old("gym_wall_i")), "J": (floor, old("gym_wall_j")),
        "R": (watf, old("lane_rope_i")), "u": (watf, at_f([old("buoy_0"), old("buoy_1")], 7)),
        "D": (floor, old("diving_board")), "f": (floor, old("bench")), "x": (floor, old("planter")), "h": (floor, old("lifeguard_chair")),
    }
    return run(txt(g), lg, (6, 6), view, "GIMNASIO DE AGUA", bg=0, extra=extra)


reg("GYM", "Gym Agua", gimnasio)


def rutas2425(variant):
    A = variant == "A"
    if A:
        def f(u, v, a, b):
            if u >= 1:
                return "g" if v >= -2 else "w"
            return "=" if v == 0 else "w"                # the long plank bridge along +i; the sea everywhere else
        g = mapof(f, "w")
        P(g, {(1, 2): "s", (2, 2): "s", (3, 2): "s", (1, 3): "s", (2, 3): "s", (0, 2): "S", (-3, -2): "S", (-2, 2): "S", (3, -3): "C", (1, -1): "b",
              (1, 1): "b", (2, 1): "F", (3, 1): "F", (4, 2): "b", (1, 0): "g"})
        cot = (8, 5)
        hero, view = (6, 6), "north-west"
        extra = [(5, 6, actor("explorador", "south-east")), (4, 6, actor("cazabichos", "south-east"))]
    else:
        def f(u, v, a, b):
            if v >= 2 or u <= 0:
                return "g"
            return "=" if v == 0 else "w"                # the bridge goes down-right (+i) over an inlet; the sea around it
        g = mapof(f, "g")
        P(g, {(-2, 1): "b", (-1, 2): "r", (1, 2): "s", (2, 2): "s", (3, 2): "s", (0, 3): "s", (-3, 0): "C", (1, 1): "w", (3, -1): "S", (2, 2): "s",
              (-1, -1): "b", (-3, 2): "b", (-2, 3): "r", (1, 3): "s"})
        cot = (5, 8)
        hero, view = (6, 6), "south-east"
        extra = [(7, 6, actor("explorador", "north-west")), (9, 6, actor("cazabichos", "north-west"))]
    W = cells_of(g, "wS")
    seaf = sea_at(W)
    lg = {
        "g": (GRASS3, None), "w": (seaf, None), "s": (hv([old("sand_a"), old("sand_b"), old("sand_d")]), None),
        "=": (deck_at(lambda i, j: "i", W, wet=False), None),
        "C": (GRASS3, old("tree_code")), "b": (GRASS3, hv([old("beach_grass_a"), old("beach_grass_b")])), "r": (GRASS3, old("rock_code")),
        "F": (GRASS3, old("picket_j")), "M": (GRASS3, old("mailbox")), "S": (seaf, old("sea_rock_b")),
    }
    cottage = at_f([old("cottage_0"), old("cottage_1")], 6)
    return run(txt(g), lg, hero, view, "RUTAS 24-25", extra=[(cot[0], cot[1], cottage)] + extra, post=gulls((12, 10, 0.8)))


reg("R2425", "R24-25", rutas2425)


# ================================================================ output

def write(keys):
    os.makedirs(OUT, exist_ok=True)
    sel = [k for k in SCENES if not keys or k[0] in keys]
    scenes = [(SCENES[k][0], SCENES[k][1](), k) for k in sel]
    with open(os.path.join(OUT, "anim.bin" if not keys else "anim_part.bin"), "wb") as f:
        f.write(b"AGT1" + bytes([len(scenes)]))
        for name, frs, _ in scenes:
            f.write(name.encode()[:23].ljust(24, b"\0"))
            f.write(struct.pack("<HH", len(frs), zm.TICK))
            for p in frs:
                f.write(zm.pack(p))
    rows = (len(scenes) + 1) // 2
    # sheet: one row per zone (A | B), 2 frames each -> 4 columns
    cw, ch = zm.SW * 3, zm.SH * 3
    zones = []
    for _, frs, k in scenes:
        zones.append(k)
    keys_u = []
    for k in zones:
        if k[0] not in keys_u:
            keys_u.append(k[0])
    sheet = Image.new("RGB", (cw * 4 + 50, (ch + 10) * len(keys_u) + 10), (40, 40, 40))
    for name, frs, k in scenes:
        zm.gif(frs, os.path.join(OUT, "%s_%s.gif" % (k[0], k[1])))
        r = keys_u.index(k[0])
        for c, fi in enumerate((6, NF - 1)):
            x = 10 + ((0 if k[1] == "A" else 2) + c) * (cw + 10)
            im = lvlib.lcd_image(frs[fi], None, scale=3, grid=False)
            sheet.paste(im, (x, 10 + r * (ch + 10)))
    sheet.save(os.path.join(OUT, "sheet.png" if not keys else "sheet_part.png"))
    if os.environ.get("C2_PREVIEW"):          # big single frames for eyeballing (a row per scene), not part of the deliverable
        big = Image.new("RGB", (zm.SW * 5 * 2 + 30, (zm.SH * 5 + 10) * len(scenes) + 10), (40, 40, 40))
        for n_, (name, frs, k) in enumerate(scenes):
            for c, fi in enumerate((8, 30)):
                big.paste(lvlib.lcd_image(frs[fi], None, scale=5, grid=False), (10 + c * (zm.SW * 5 + 10), 10 + n_ * (zm.SH * 5 + 10)))
        big.save(os.environ["C2_PREVIEW"])
    print("scenes:", len(scenes), "anim.bin:", os.path.getsize(os.path.join(OUT, "anim.bin" if not keys else "anim_part.bin")), "bytes")


if __name__ == "__main__":
    write([a.upper() for a in sys.argv[1:]])
