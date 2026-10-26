#!/usr/bin/env python3
"""Overworld actors of the Empuje puzzle (contract 58): the approved PixelLab pictures of the pushable rock and of the pit
(assets/art/objects_c5/push_boulder.png, floor_pit.png, 96 x 96) scaled down to the two catalog actors `boulder_rock` and `hole_pit`
(assets/art/npcs/<actor>/south.png, converted by the catalog like the legendary birds: `levels = "adaptive"`). Same premultiplied-alpha
Lanczos resample as tools/legend_ow.py (alpha cut at 128: no black bleeding into the edge).

  python3 tools/empuje_ow.py [build|check]

build  (default) writes assets/art/npcs/boulder_rock/south.png (24 x 24, the anchor = the rock's foot: the catalog default, bottom centre) and
       assets/art/npcs/hole_pit/south.png (26 x 15: the hole seen on the isometric floor = the picture squashed to 58 % in height; the catalog's
       `ay` puts the anchor on the pit's centre). The old placeholder `south/f0.lv.png` folders are removed.
check  exits 1 when a picture is missing or older than its source (a step of tools/build_assets.sh)."""
import os
import shutil
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image  # noqa: E402
from legend_ow import bbox, premul_resize  # noqa: E402

SRC = os.path.join(ROOT, "assets", "art", "objects_c5")
MTIME_SLACK = 60  # seconds: a fresh clone / CI checkout writes source and derived files in arbitrary order within a few seconds
DST = os.path.join(ROOT, "assets", "art", "npcs")
JOBS = (
    ("push_boulder.png", "boulder_rock", 24, 24),
    ("floor_pit.png", "hole_pit", 26, 15),
)


def build():
    for src, actor, w, h in JOBS:
        im = Image.open(os.path.join(SRC, src)).convert("RGBA")
        im = im.crop(bbox(im))
        out = premul_resize(im, w, h)
        d = os.path.join(DST, actor)
        old = os.path.join(d, "south")
        if os.path.isdir(old):
            shutil.rmtree(old)
        os.makedirs(d, exist_ok=True)
        out.save(os.path.join(d, "south.png"))
        print(f"wrote {actor}/south.png ({w} x {h})")


def check():
    bad = 0
    for src, actor, w, h in JOBS:
        p = os.path.join(DST, actor, "south.png")
        if not os.path.exists(p) or os.path.getmtime(
            p
        ) + MTIME_SLACK < os.path.getmtime(os.path.join(SRC, src)):
            print(
                f"empuje_ow: {p} missing or older than {src}: run `python3 tools/empuje_ow.py`"
            )
            bad += 1
        elif Image.open(p).size != (w, h):
            print(f"empuje_ow: {p} is {Image.open(p).size}, expected {(w, h)}")
            bad += 1
    if os.path.isdir(os.path.join(DST, "boulder_rock", "south")) or os.path.isdir(
        os.path.join(DST, "hole_pit", "south")
    ):
        print(
            "empuje_ow: an old placeholder folder south/ is still there (the `.lv.png` would win over south.png)"
        )
        bad += 1
    print("empuje_ow: OK" if not bad else "empuje_ow: FAILED")
    return 1 if bad else 0


if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "build"
    if mode == "check":
        sys.exit(check())
    build()
