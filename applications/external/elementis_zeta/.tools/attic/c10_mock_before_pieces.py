#!/usr/bin/env python3
"""Chapter 10 zone-identity mockups (the design notes §4): the Ruinas del Origen (8 fragment pedestals around the
hero, the Genesa/Arkegen mural stele, fallen columns, big light ancient slabs) and the hidden Gruta del Eclipse (dark rock with
light veins, the altar with the Piedra Eclipse, the light shaft that only shows at the right hours). Same composer as tools/c9_mock.py;
the new pieces are drawn here in memory (D1 moves them into gen_tiles like c9_pieces when the zone is built).

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
def ancient_slab(pi, pj, seed=0):
    """Ruins floor: big light slabs (2x2) with a thin gray joint, a few hairline cracks and worn corners."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        U, V = u + pi, v + pj
        if (
            abs(U - 0.6 - 0.25 * math.sin(V * 5 + pi * 3)) < 0.02
            and 0.3 < V < 1.2
            and (pi + pj) % 2 == 0
        ):
            return 1
        if min(U, V, 2 - U, 2 - V) < 0.12 and dither(x, y, 0.25):
            return 1
        return 0

    return T(gtile(f, seed))


def pedestal(lit=False, phase=0):
    """Fragment pedestal: a short stone drum; lit, it holds a floating fragment shard with a blinking sparkle."""
    S = Spr()
    S.rect(-5, -9, 5, 0, 1)
    S.rect(-6, -11, 6, -9, 0)
    S.hline(-5, 5, -5, 2)
    if lit:
        S.poly([(0, -22), (3, -17), (0, -13), (-3, -17)], 0)
        S.line(0, -21, 0, -14, 1)
        if phase:
            for dx, dy in ((-6, -19), (6, -16), (0, -25)):
                S.px(dx, dy, 0)
    else:
        S.rect(-1, -12, 1, -11, 2)  # the empty socket
    S.outline(3)
    if (
        lit and phase
    ):  # the sparkle sits outside the outline (white specks on the dark ring)
        for dx, dy in ((-7, -19), (7, -16), (0, -27)):
            S.px(dx, dy, 0)
    return done(S, outline=False)


def mural_stele():
    """The mural: a tall stone slab carved with the myth — a ring (the origin) between two creatures facing each other (Genesa, Arkegen)."""
    S = Spr()
    S.rect(-12, -28, 12, 0, 1)
    S.rect(-13, -30, 13, -28, 0)
    for a in range(0, 360, 12):  # the origin ring
        x, y = round(5 * math.cos(math.radians(a))), round(
            -20 + 5 * math.sin(math.radians(a))
        )
        S.px(x, y, 3)
    S.px(0, -20, 0)
    S.poly(
        [(-10, -10), (-7, -16), (-5, -12), (-6, -6)], 2
    )  # Genesa (left), Arkegen (right): carved silhouettes
    S.poly([(10, -10), (7, -17), (4, -13), (6, -6)], 2)
    S.hline(-11, 11, -3, 2)
    S.outline(3)
    return done(S, outline=False)


def fallen_column(axis="i"):
    """A broken column: a short standing stump and its drum lying beside it."""
    S = Spr()
    S.rect(-3, -14, 3, 0, 0)
    S.line(-3, -14, 0, -16, 0)
    S.line(0, -16, 3, -13, 0)
    S.line(-1, -12, -1, -2, 1)
    d = 1 if axis == "i" else -1
    for k in range(4):
        S.rect(
            d * (5 + k * 2) - 2, -3 + k, d * (5 + k * 2) + 2, 1 + k, 1 if k % 2 else 0
        )
    S.outline(3)
    return done(S, outline=False)


def vein_rock(seed=0):
    """Gruta del Eclipse floor: dark rock (level 2) with light veins (1) and a few white glints."""

    def f(x, y, u, v, r):
        if (
            abs(math.sin(u * 7 + v * 3 + seed * 2) * 0.12 + 0.5 - (u * 0.6 + v * 0.4))
            < 0.03
        ):
            return 1
        if (x * 7 + y * 13 + seed * 5) % 97 == 0:
            return 0
        return 2

    return T(gtile(f, seed))


def altar(stone=True):
    """The Eclipse altar: a stepped stone block; on it the Piedra Eclipse (a black disc with a white corona)."""
    S = Spr()
    S.rect(-9, -6, 9, 0, 1)
    S.rect(-7, -11, 7, -6, 0)
    S.hline(-8, 8, -3, 2)
    if stone:
        for a in range(0, 360, 10):
            S.px(
                round(5 * math.cos(math.radians(a))),
                round(-17 + 5 * math.sin(math.radians(a))),
                0,
            )
        for y in range(-21, -12):
            for x in range(-4, 5):
                if x * x + (y + 17) ** 2 <= 13:
                    S.px(x, y, 3)
    S.outline(3)
    return done(S, outline=False)


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
        lit = at_f([ob(("ped1", k), lambda k=k: pedestal(True, k)) for k in (0, 1)], 4)
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
    lit = at_f([ob(("ped1", k), lambda k=k: pedestal(True, k)) for k in (0, 1)], 4)
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
        P(g, {(1, -1): "A", (-2, 1): "s", (3, 2): "s", (2, -2): "s"})
        fl = ground("vrock", vein_rock, 3)
        lg = {
            ".": (fl, None),
            "W": (fl, hv([old("cwall_a0"), old("cwall_b0")])),
            "J": (fl, hv([old("cwall_a1"), old("cwall_b1")])),
            "A": (fl, ob(("altar", hour), lambda: altar(stone=hour))),
            "s": (fl, old("stalag_a")),
        }
        # the altar cell (1, -1) sits at screen x = 64 + 14 * 2, y = 32 (its base); the shaft ends on the stone above it
        return run(
            txt(g),
            lg,
            (6, 6),
            "south-west",
            "GRUTA DEL ECLIPSE",
            bg=0,
            post=light_shaft(92, 16) if hour else None,
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
