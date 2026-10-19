#!/usr/bin/env python3
"""M5 mockups: Pueblo Cantera (the quarry town) and the Rock gym's interior, at the final size in 4 grays,
same engine geometry and composer as tools/zone_mock.py. The pieces are the game's own: tools/gen_tiles.py cantera_pieces draws each
ONCE into assets/art/tiles/*.lv.png (run it first) and this mock loads them (the user picked "M Cantera mezcla" and "G3 big slabs").

  python3 -I tools/town_mock.py
writes build/town_gate/<scene>.gif, sheet.png and anim.bin (the anim_gate viewer: copy to anim_gate/files/anim.bin).
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

OUT = os.path.join(ROOT, "build", "town_gate")
Sp = zm.Sp
old = zm.old


# The pieces are the game's own: tools/gen_tiles.py cantera_pieces() draws each ONCE into assets/art/tiles/*.lv.png (run it first); this mock only
# loads them (like tools/zone_mock.py). Variant lookups below move_copy the catalog's [variants] hash with fixed rules.
def piece(name):
    return old(name)


def gravel(seed=0):
    return old("gravel_" + "abc"[seed % 3])


def cliff(seed=0):
    return old("cliff_" + "abc"[seed % 3])


def rails(seed=0):
    return old("rails_a")


def boulder(seed=0):
    return old("boulder")


def pickaxe_sign():
    return old("pickaxe_sign")


def cart():
    return old("cart")


def stone_house(slate=True):
    return old("stone_house")


def stage():
    return old("stage")


def pillar(seed=0):
    return old("pillar")


def gym_cantera():
    return old("gym_cantera")


def big_slab(i, j, seed=0):
    return old("slab_%d%d" % (i % 2, j % 2))


# ---------------------------------------------------------------- scenes
def char(name, view="south-west"):
    p = os.path.join(ROOT, "models_dl/pixellab", name, view + ".png")
    return zm.npc(os.path.relpath(p, ROOT)) if os.path.exists(p) else None


def cantera_mix():
    """Mix: gravel ground, rock walls ringing the town (D), stone houses, boulders, the sign, rails with the cart (A)."""
    gv = [gravel(s) for s in range(3)]
    cl = [cliff(s) for s in range(3)]
    grid = """
CCCCCCCCCCCCC
CCCCCCCCCCCCC
CCCCCCCCCCCCC
CCCC.==..CCCC
CCC..==....CC
CC...==..b..C
CC...==.....C
CC...==RRRRRR
CC.s.==.....C
CC...==.....C
CC...==.....C
CC...==.....C
CC...==.....C
"""
    lg = {
        ".": (lambda i, j, f: gv[(i * 5 + j * 3) % 3], None),
        "=": (lambda i, j, f: zm.PATH, None),
        "C": (lambda i, j, f: gv[0], lambda i, j, f: cl[(i * 3 + j) % 3]),
        "R": (lambda i, j, f: rails(i), None),
        "b": (lambda i, j, f: gv[1], boulder(5)),
        "s": (lambda i, j, f: gv[2], pickaxe_sign()),
    }
    return zm.run(
        grid,
        lg,
        (5, 5),
        "north-east",
        "PUEBLO CANTERA",
        extra=[(2, 7, stone_house()), (9, 7, cart())],
    )


def gym_interior():
    """G3: one big slab per 2 x 2 tiles (the variant follows the cell slab_<i%2><j%2>), rock pillars, a boulder, Garrick's dais."""
    wj, wi = old("wall_j"), old("wall_i")
    garrick = char("garrick_v3") or zm.hero("south-west")
    grid = """
_____________
_____________
__WWWWWWWWWWW
__w..........
__w..........
__w..........
__w.p....p...
__w..........
__w..........
__w.b........
__w..........
__w.p....p...
__w..........
"""
    lg = {
        "_": (None, None),
        "W": (lambda i, j, f: big_slab(i, j), wj),
        "w": (lambda i, j, f: big_slab(i, j), wi),
        ".": (lambda i, j, f: big_slab(i, j), None),
        "p": (lambda i, j, f: big_slab(i, j), pillar(3)),
        "b": (lambda i, j, f: big_slab(i, j), boulder(4)),
    }
    out = []
    for fr in range(zm.NF):
        p = zm.compose(
            grid,
            lg,
            (5, 9),
            zm.hero("north-east"),
            bg=3,
            f=fr,
            extra=[(6, 6, stage()), (7, 7, garrick)],
        )
        out.append(p)
    return out


def main():
    os.makedirs(OUT, exist_ok=True)
    scenes = [
        ("M Cantera mezcla", cantera_mix()),
        ("G3 Gimnasio losa grande", gym_interior()),
    ]
    with open(os.path.join(OUT, "anim.bin"), "wb") as f:
        f.write(b"AGT1" + bytes([len(scenes)]))
        for name, frs in scenes:
            f.write(name.encode()[:23].ljust(24, b"\0"))
            f.write(struct.pack("<HH", len(frs), zm.TICK))
            for p in frs:
                f.write(zm.pack(p))
    sheet = Image.new(
        "RGB",
        (zm.SW * 4 * 2 + 30, zm.SH * 4 * len(scenes) + 10 * len(scenes) + 10),
        (40, 40, 40),
    )
    for k, (name, frs) in enumerate(scenes):
        zm.gif(frs, os.path.join(OUT, name.split()[0] + ".gif"))
        for c, fi in enumerate((4, zm.NF - 1)):
            sheet.paste(
                lvlib.lcd_image(frs[fi], None, scale=4, grid=False),
                (10 + c * (zm.SW * 4 + 10), 10 + k * (zm.SH * 4 + 10)),
            )
    sheet.save(os.path.join(OUT, "sheet.png"))
    print("anim.bin:", os.path.getsize(os.path.join(OUT, "anim.bin")), "bytes")


if __name__ == "__main__":
    main()
