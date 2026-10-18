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


# ================================================================ ground tiles
def rock_floor(seed=0):
    """Senda de Campeones floor: LIGHT worn rock with sparse dark grit and a few hairline cracks."""
    def f(x, y, u, v, r):
        if abs((u - 0.2) - 0.9 * (v - 0.3)) < 0.02 and seed % 2 and 0.2 < v < 0.8:
            return 2
        k = r.random()
        return 3 if k < 0.02 else (1 if k < 0.07 else 0)
    return T(gtile(f, seed))


def marble(pi, pj, seed=0):
    """League marble: big light slabs (2x2) with a soft vein crossing each block and a thin gray joint."""
    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        U, V = u + pi, v + pj
        if abs(math.sin(U * 2.2 + V * 1.3 + seed) * 0.3 + 0.9 - V) < 0.025:
            return 1
        return 0
    return T(gtile(f, seed))


def tatami_ring(pi, pj, seed=0):
    """Fighting room: a big light canvas mat (2x2) with a dark ring line near the block edge (the ring's floor)."""
    def f(x, y, u, v, r):
        U, V = u + pi, v + pj
        if min(U, V, 2 - U, 2 - V) < 0.06:
            return 3
        if min(U, V, 2 - U, 2 - V) < 0.12:
            return 1
        return 0
    return T(gtile(f, seed))


def ghost_floor(pi, pj, seed=0):
    """Ghost room: light old flagstones (2x2) under a drifting mist dither that moves by frame (set by the caller)."""
    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        return 1 if dither(x, y, 0.12) else 0
    return T(gtile(f, seed))


def crystal_floor(seed=0):
    """Cueva Secreta: LIGHT cave rock with tiny bright crystal glints (dark ring + white core)."""
    def f(x, y, u, v, r):
        k = r.random()
        if k < 0.015:
            return 3
        if k < 0.05:
            return 1
        return 0
    return T(gtile(f, seed))


# ================================================================ objects
def badge_gate(n=1):
    """Ruta 23 checkpoint gate: two stone posts with a lintel; a round badge emblem (dark ring, light core) on the lintel."""
    S = Spr()
    for x in (-9, 8):
        S.rect(x, -26, x + 3, 0, 1)
        S.px(x, -26, 3)
    S.rect(-10, -30, 12, -25, 0)
    S.hline(-10, 12, -30, 3)
    S.hline(-10, 12, -25, 3)
    S.disc(1, -28, 3, 0, ring=3)
    S.px(1, -28, 2)
    return done(S)


def pressure_plate(down=False):
    """Boulder switch: a square stone plate with a dark rim; pressed = darker center (flat, on the floor)."""
    def f(x, y, u, v, r):
        if 0.2 < u < 0.8 and 0.2 < v < 0.8:
            if 0.27 < u < 0.73 and 0.27 < v < 0.73:
                return 2 if down else 1
            return 3
        return None
    return T(gtile(f, 1))


def torch(phase=0):
    """Wall torch on a post: a dark iron bowl and a flickering flame (2 frames)."""
    S = Spr()
    S.rect(0, -16, 2, 0, 3)
    S.hline(-3, 6, -16, 3)
    S.poly([(-2, -17), (1, -24 - phase), (4, -17)], 0)
    S.px(1, -19, 1)
    return done(S)


def banner(axis="i"):
    """League banner wall: light plaster wall with a hanging long banner bearing a crown emblem."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, gt.WALL_H, 0, 1, 1, edge=3)
    F = b.J if axis == "i" else b.I
    F.rect(4, F.wid - 4, 6, gt.WALL_H - 2, 2)
    for z in range(6, 10):
        F.px(F.wid // 2 - (10 - z), z, 1)
        F.px(F.wid // 2 + (10 - z), z, 1)
    cx = F.wid // 2
    F.rect(cx - 2, cx + 3, 20, 23, 0)
    F.px(cx - 2, 24, 0)
    F.px(cx, 24, 0)
    F.px(cx + 2, 24, 0)
    return done(S, outline=False)


def statue():
    """Meseta Cumbre statue: a tall pedestal with a stylised winged crown (the league's emblem)."""
    S = Spr()
    Box(S, 1, 1, 1, 1, 6, 6, 0, 14, 0, 1, 2, edge=3)
    S.rect(-1, -28, 2, -14, 1)
    S.poly([(-9, -30), (0, -26), (9, -30), (6, -24), (0, -22), (-6, -24)], 0)
    S.poly([(-4, -38), (-2, -32), (0, -36), (2, -32), (4, -38), (4, -31), (-4, -31)], 1)
    S.outline(3)
    return done(S, outline=False)


def icicle_pillar():
    """Ice room pillar: a light column with icicles hanging from its top ring."""
    S = Spr()
    S.rect(-4, -34, 5, 0, 0)
    S.hline(-6, 7, -34, 3)
    for x in range(-5, 7, 2):
        for t in range((x * 7) % 4 + 2):
            S.px(x, -33 + t, 1)
    S.outline(3)
    return done(S, outline=False)


def ring_post():
    """Fighting room ring corner: a padded post with three ropes leaving it."""
    S = Spr()
    S.rect(-1, -20, 2, 0, 3)
    S.rect(-2, -22, 3, -18, 1)
    for z in (-8, -12, -16):
        S.line(1, z, 14, z + 7, 2)
    return done(S)


def candelabra(phase=0):
    """Ghost room candelabra: three candles on a dark stand, flames flicker (2 frames)."""
    S = Spr()
    S.rect(0, -14, 1, 0, 3)
    S.hline(-6, 7, -14, 3)
    for x in (-6, 0, 6):
        S.rect(x, -20, x + 1, -14, 0)
        S.px(x, -21 - ((phase + x) % 2), 1)
        S.px(x, -22 - ((phase + x) % 2), 0)
    return done(S)


def dragon_statue():
    """Dragon room: a stone dragon head on a plinth (light stone, dark eye and horn lines)."""
    S = Spr()
    Box(S, 1, 1, 1, 1, 6, 6, 0, 10, 1, 2, 2, edge=3)
    S.poly([(-6, -10), (-6, -24), (2, -30), (8, -26), (6, -20), (0, -18), (2, -10)], 0)
    S.line(-4, -26, -9, -34, 3)
    S.line(0, -29, -2, -36, 3)
    S.px(3, -25, 3)
    S.line(1, -20, 6, -21, 2)
    S.outline(3)
    return done(S, outline=False)


def champion_pillar():
    """Champion hall pillar: a fluted light column with a dark capital band and a crown relief."""
    S = Spr()
    S.rect(-5, -40, 6, 0, 0)
    for x in (-3, 0, 3):
        S.line(x, -38, x, -2, 1)
    S.hline(-7, 8, -40, 3)
    S.hline(-7, 8, -1, 3)
    S.outline(3)
    return done(S, outline=False)


def glow_crystal(seed=0):
    """Cueva Secreta crystal cluster: light prisms with dark facet lines (it glints every other frame via the caller's variant)."""
    S = Spr()
    for dx, h in ((-5, 14 + seed * 3), (0, 22), (5, 12)):
        S.poly([(dx - 3, 0), (dx - 2, -h + 4), (dx, -h), (dx + 2, -h + 4), (dx + 3, 0)], 0)
        S.line(dx, -h, dx, 0, 1)
    S.outline(3)
    return done(S, outline=False)


def pl_object(name, max_h, dy=4):
    """A PixelLab object PNG (assets/art/objects_c9/<name>.png), scaled to max_h px tall, anchored at its base centre."""
    import battle_mock as bm
    sp = bm.load(os.path.join(ROOT, "assets/art/objects_c9", name + ".png"), max_h=max_h)
    return c2.Sp(sp.lv, sp.mk, sp.w // 2, sp.h - 1 - dy, 1, 1)


SCENES = {}


def reg(key, title, fn):
    SCENES[key] = (title, fn)


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
    lg = {"g": (grass, None), "p": (path, None), "G": (path, ob("bgate", badge_gate)), "t": (grass, old("tree_code"))}
    return run(txt(g), lg, (6, 6), "south-east", "RUTA 23", extra=[(8, 5, npc("guardia", "south-west"))])


reg("R23", "Ruta 23", r23)


# ================================================================ SENDA DE CAMPEONES (Senda de Campeones)
def senda():
    g = mapof(lambda u, v, a, b: "W" if (u <= -3 or v <= -3) else ".", ".")
    room_walls(g)
    P(g, {(1, -1): "o", (2, 1): "s", (-2, 2): "S", (3, -2): "f", (-1, 3): "f", (-3, 0): "b"})
    fl = ground("rockf", rock_floor, 3)
    lg = {
        ".": (fl, None), "W": (fl, hv([old("cwall_a0"), old("cwall_b0")])), "J": (fl, hv([old("cwall_a1"), old("cwall_b1")])),
        "o": (fl, old("boulder")), "s": (memo("plate0", lambda: pressure_plate(False)), None), "S": (memo("plate1", lambda: pressure_plate(True)), old("boulder")),
        "f": (fl, at_f([ob(("torch", k), lambda k=k: torch(k)) for k in (0, 1)], 3)), "b": (fl, old("stalag_a")),
    }
    return run(txt(g), lg, (6, 6), "north-east", "SENDA DE CAMPEONES", bg=0, extra=[(8, 4, npc("elite", "south-west"))])


reg("SENDA", "Senda de Campeones", senda)


# ================================================================ MESETA CUMBRE (plaza + league hall)
def meseta():
    g = mapof(lambda u, v, a, b: ".", ".")
    P(g, {(1, -1): "S", (3, 2): "l", (-2, 2): "l", (-2, -1): "f", (3, -1): "f"})
    fl = big("marb", marble)
    lg = {".": (fl, None), "S": (fl, ob("statue", statue)), "l": (fl, at_f([old("lamp_h0"), old("lamp_h1")], 4)),
          "f": (fl, at_f([old("vflag_0"), old("vflag_1")], 5))}
    return run(txt(g), lg, (6, 6), "south-east", "MESETA CUMBRE")


reg("MESETA", "Meseta Cumbre", meseta)


# ================================================================ the five league rooms
def hielo():
    return room(big("ice", c7.ice_slab), old("wall_i"), old("wall_j"),
                {(-1, -1): ("p", ob("icecr", lambda: pl_object("ice_crystals", 34))), (3, 1): ("p", ob("icecr", lambda: pl_object("ice_crystals", 34))), (2, 2): ("d", ob("drift", c7.snow_drift))},
                "SALA HIELO", extra=[(8, 4, npc("iver", "south-west"))])


def lucha():
    return room(big("ringm", tatami_ring), old("wall_i"), old("wall_j"),
                {(-1, -1): ("r", ob("rpost", ring_post)), (2, 2): ("r", ob("rpost", ring_post))},
                "SALA LUCHA", extra=[(8, 4, npc("tanko", "south-west"))])


def fantasma():
    def post(p, f):
        for y in range(zm.SH):
            for x in range(zm.SW):
                if ((x + f * 2 + (y // 6) * 9) % 23) < 3 and dither(x, y, 0.5) and p[y][x] == 0:
                    p[y][x] = 1
    def build():
        g = mapof(lambda u, v, a, b: "W" if (u <= -3 or v <= -3) else ".", ".")
        room_walls(g)
        P(g, {(-1, -1): "c", (2, 1): "c", (-2, 2): "c"})
        fl = big("ghf", ghost_floor)
        cd = at_f([ob(("cand", k), lambda k=k: candelabra(k)) for k in (0, 1)], 3)
        lg = {".": (fl, None), "W": (fl, old("wall_i")), "J": (fl, old("wall_j")), "c": (fl, cd)}
        return run(txt(g), lg, (6, 6), "north-east", "SALA FANTASMA", bg=0, post=post, extra=[(8, 4, npc("madame_velo", "south-west"))])
    return build()


def dragon():
    return room(big("marb", marble), old("wall_i"), old("wall_j"),
                {(-1, -1): ("d", ob("dstat2", lambda: pl_object("dragon_statue", 44))), (3, 1): ("d", ob("dstat2", lambda: pl_object("dragon_statue", 44)))},
                "SALA DRAGON", extra=[(8, 4, npc("arkan", "south-west"))])


def campeon():
    return room(big("marb", marble), ob("ban_i", lambda: banner("i")), ob("ban_j", lambda: banner("j")),
                {(-1, -1): ("p", ob("cpil2", lambda: pl_object("champion_column", 44))), (3, 1): ("p", ob("cpil2", lambda: pl_object("champion_column", 44))),
                 (-1, 2): ("t", ob("troph", lambda: pl_object("champion_trophy", 30))), (2, 2): ("t", ob("troph", lambda: pl_object("champion_trophy", 30)))},
                "SALA DEL CAMPEON", extra=[(8, 4, npc("rival", "south-west"))])


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
    lg = {".": (fl, None), "W": (fl, hv([old("cwall_a0"), old("cwall_b0")])), "J": (fl, hv([old("cwall_a1"), old("cwall_b1")])), "c": (fl, gc),
          "w": (fl, old("stalag_s"))}
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
        sheet.paste(lvlib.lcd_image(frs[6], None, scale=3, grid=False), (10 + (n % 2) * (cw + 10), 10 + (n // 2) * (ch + 10)))
    sheet.save(os.path.join(OUT, "sheet.png"))
    print("scenes:", len(scenes), "anim.bin:", os.path.getsize(os.path.join(OUT, "anim.bin")), "bytes")


if __name__ == "__main__":
    write([a.upper() for a in sys.argv[1:]])
