#!/usr/bin/env python3
"""assets/art/hero_bike/<8 directions>.png (PixelLab v3, 48 px, ONE frame per direction) -> nucleo/files/hero_bike.bin.

The bicycle file has exactly the format of hero.bin (contract 3.4, 24 x 35, 5 frames, the 5 stored facings S SE E NE N, written by tools/pl2h.py --bin): render.c
draws the riding hero through the same `hero_view` with the same constants, so the art has to fit the 24 x 35 box and the anchor (HERO_AX 12, HERO_AY 31).
This script prepares that box and calls pl2h.py:
  1. every direction is scaled by ONE factor (the largest that fits the widest direction, the east view, into 24 px; the rider and the bicycle keep their proportions
     between facings), alpha is cut at 50 %, the picture is placed bottom-centre of the 24 x 35 canvas (the wheels' lowest row on row 33: the walking hero's feet are on
     rows 30..34) with one empty row above it;
  2. frames 1 and 3 are the same picture one pixel higher (the legs' "1 px bob" of the no-art path of render.c, baked into the data: the art has one pose per direction);
     frames 0, 2 and 4 are the picture itself. The walk cycle picks frame 1 + ((walk_px / 7) & 3), so the bicycle bobs while it rolls and stands still when idle;
  3. pl2h.py --adaptive --fixed-box --mirror-west --fold-mirror --bin: 4-level typecast by luminance (the art is coloured but dark-toned), the west views are the mirrors of the east ones
     (the art's own west views are not exact mirrors), only S SE E NE N are stored.
  tools/mkbike.py writes nucleo/files/hero_bike.bin (+ build/hero_bike/ intermediate canvases and preview.png) and, when assets/art/hero_surf/ exists, nucleo/files/hero_surf.bin
  tools/mkbike.py check the file(s) exist and have hero.bin's size / header and the pictures stay inside the box; exit 1 otherwise

C5 (contract 42.1): the SURFING hero is the same pipeline: assets/art/hero_surf/<8 directions>.png (the hero on the board, PixelLab v3, 48 px, ONE frame per direction, the same file names as hero_bike) ->
nucleo/files/hero_surf.bin (hero.bin's format; render.c hero_view reads it while he surfs). Without the art folder nothing is built and the game draws the walking hero with a 1 px bob (the bike's fallback).
  tools/mkbike.py surf [SRC_DIR [OUT_FILE]] build only the surfing file (the host test builds one from a copy of the bicycle art)
  tools/mkbike.py bike [SRC_DIR [OUT_FILE]] build only the bicycle file
"""
import os
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image  # noqa: E402

SETS = {  # name -> (art folder, work folder, output file, header-guard name for pl2h)
    "bike": (
        os.path.join(ROOT, "assets/art/hero_bike"),
        os.path.join(ROOT, "build/hero_bike"),
        os.path.join(ROOT, "nucleo/files/hero_bike.bin"),
        "BIKE",
    ),
    "surf": (
        os.path.join(ROOT, "assets/art/hero_surf"),
        os.path.join(ROOT, "build/hero_surf"),
        os.path.join(ROOT, "nucleo/files/hero_surf.bin"),
        "SURF",
    ),
}
SRC, WORK, OUT, _GUARD = SETS[
    "bike"
]  # (the functions below work on these module globals; select() points them at a set)
HERO = os.path.join(ROOT, "nucleo/files/hero.bin")
DIRS8 = [
    "south",
    "south-east",
    "east",
    "north-east",
    "north",
    "north-west",
    "west",
    "south-west",
]
W, H = 24, 35
FRAMES = 5
BOTTOM_ROW = 33  # the lowest row of the picture (1 row above it is left for the bob)


def bbox(im):
    return im.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()


GAMMA = 0.5  # the art is dark-toned (cap, jacket, tyres ~30..70 of 255): lift the tones before the 4-level typecast, or everything is black
DARK_CUT = 20


def lift(im):
    r, g, b, a = im.split()
    lut = [round(255 * (v / 255) ** GAMMA) for v in range(256)]
    return Image.merge("RGBA", (r.point(lut), g.point(lut), b.point(lut), a))


def prepare():
    """-> {dir: PIL image 24 x 35} for the 8 directions, scaled and placed; also returns the scale."""
    raw = {n: Image.open(os.path.join(SRC, n + ".png")).convert("RGBA") for n in DIRS8}
    boxes = {n: bbox(im) for n, im in raw.items()}
    # the east views set the scale (the widest facings); the same factor for every direction
    wide = max(b[2] - b[0] for n, b in boxes.items() if n in ("east", "west"))
    tall = max(b[3] - b[1] for b in boxes.values())
    scale = min(W / wide, (BOTTOM_ROW) / tall)
    out = {}
    for n, im in raw.items():
        b = boxes[n]
        crop = im.crop(b)
        w = max(1, round(crop.width * scale))
        h = max(1, round(crop.height * scale))
        w, h = min(w, W), min(h, BOTTOM_ROW)
        sm = lift(crop.resize((w, h), Image.LANCZOS))
        a = sm.getchannel("A").point(lambda v: 255 if v >= 128 else 0)
        sm.putalpha(a)
        canvas = Image.new("RGBA", (W, H), (0, 0, 0, 0))
        # centre on the middle of the picture's own box (the bicycle's wheels are the anchor), bottom on BOTTOM_ROW
        x0 = (W - w) // 2
        y0 = BOTTOM_ROW + 1 - h
        canvas.paste(sm, (x0, y0), sm)
        out[n] = canvas
    return out, scale


def write_frames(dirs):
    for n, im in dirs.items():
        d = os.path.join(WORK, n)
        os.makedirs(d, exist_ok=True)
        up = Image.new("RGBA", im.size, (0, 0, 0, 0))
        up.paste(im, (0, -1))  # one pixel higher
        for f in range(FRAMES):
            (up if f in (1, 3) else im).save(os.path.join(d, f"f{f}.png"))


def select(name, src=None, out=None):
    """Point the module globals at one sprite set ("bike" / "surf"), optionally with another art folder / output file (the tests)."""
    global SRC, WORK, OUT, _GUARD
    SRC, WORK, OUT, _GUARD = SETS[name]
    if src:
        SRC = os.path.abspath(src)
    if out:
        OUT = os.path.abspath(out)
    if src or out:
        WORK = os.path.join(ROOT, "build", "hero_" + name + "_try")


def build():
    dirs, scale = prepare()
    write_frames(dirs)
    hdr = os.path.join(WORK, "hero_" + _GUARD.lower() + ".h")
    prev = os.path.join(WORK, "preview.png")
    r = subprocess.run(
        [
            sys.executable,
            os.path.join(ROOT, "tools/pl2h.py"),
            WORK,
            hdr,
            _GUARD,
            prev,
            "--adaptive",
            "--dark-cut",
            str(DARK_CUT),
            "--fixed-box",
            "--mirror-west",
            "--fold-mirror",
            "--bin",
            OUT,
        ],
        capture_output=True,
        text=True,
    )
    print(r.stdout.strip())
    if r.returncode:
        sys.exit(r.stderr)
    print(f"mkbike: scale {scale:.3f}, {OUT} written")


def check():
    name = os.path.basename(OUT)
    if not os.path.isfile(OUT):
        sys.exit(f"mkbike check: {OUT} is missing (run tools/mkbike.py)")
    b, h = open(OUT, "rb").read(), open(HERO, "rb").read()
    if len(b) != len(h) or b[:16] != h[:16]:
        sys.exit(
            f"mkbike check: {name} is {len(b)} B with header {b[:16].hex()}, hero.bin is {len(h)} B with {h[:16].hex()}"
        )
    nz = [d for d in range(5) if any(b[16 + d * 1575 : 16 + (d + 1) * 1575])]
    if len(nz) != 5:
        sys.exit("mkbike check: an empty facing in hero_bike.bin")
    # every stored facing must have opaque pixels in all 5 frames and none outside the box is possible by construction; mask rows 0 must stay clear for frames 0, 2, 4
    mbpr, hh = 3, 35
    for d in range(5):
        base = 16 + d * 1575 + 5 * 6 * hh  # masks
        for f in range(5):
            m = b[base + f * mbpr * hh : base + (f + 1) * mbpr * hh]
            if not any(m):
                sys.exit(f"mkbike check: facing {d} frame {f} is empty")
    print(
        f"mkbike check: {name} OK ({len(b)} B, same header as hero.bin, 5 facings x 5 frames, all with pixels)"
    )


def run_set(name, args=(), checking=False):
    select(name, *(args[:2]))
    if checking:
        check()
    else:
        build()


if __name__ == "__main__":
    argv = sys.argv[1:]
    if (
        len(argv) > 2 and argv[0] == "try"
    ):  # tools/mkbike.py try GAMMA DARK_CUT: experiment, preview in build/hero_bike/
        GAMMA, DARK_CUT = float(argv[1]), int(argv[2])
        run_set("bike")
    elif (
        argv and argv[0] == "check"
    ):  # every set whose file exists (the bicycle's is required); the surfing set only when its art folder or file exists
        run_set("bike", checking=True)
        if os.path.isdir(SETS["surf"][0]) or os.path.isfile(SETS["surf"][2]):
            run_set("surf", checking=True)
    elif argv and argv[0] in SETS:
        run_set(argv[0], argv[1:])
    else:  # the default: the bicycle, and the surfing hero when its art is there
        run_set("bike")
        if os.path.isdir(SETS["surf"][0]):
            run_set("surf")
        else:
            print(
                f"mkbike: no {os.path.relpath(SETS['surf'][0], ROOT)}/ yet: no hero_surf.bin (the game draws the walking hero with a 1 px bob while surfing)"
            )
