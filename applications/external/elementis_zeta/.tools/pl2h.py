#!/usr/bin/env python3
"""PixelLab direction sprites (native size, no resizing) -> Flipper header.

Per-pixel luminance -> levels 0..3, plus an opacity mask so light pixels stay
opaque white instead of showing the ground through.
All directions are cropped to one shared box so the feet never jump.

  pl2h.py <dir_with_pngs> <out.h> <PFX> [preview.png] [options]
PNG names: south, south-east, east, north-east, north, north-west, west, south-west
(the game's facing order 0..7). Optional frame subfolders f0, f1, ... per direction
are used when present: <dir>/<name>/f<i>.png.

Options:
  --dirs 8|4        4 = south, east, north, west only (dir4 order, array [4][..])
  --frames N        use only the first N frames of every direction (1 for idle NPCs)
  --mirror-west     build west, north-west, south-west by flipping east, north-east, south-east
  --fold-mirror     8 dirs only: store just south, south-east, east, north-east, north (array [5][..], PFX_DIRS 5); the
                    other three are drawn mirrored at run time (Blit.flip, render.c hero_view). Refuses to fold when a
                    west direction is not the exact flip of its east twin (pixels and mask).
  --bin FILE        write the sprite data to FILE (hero.bin format, see tools/hero_h2bin.py) and keep only the #defines in the
                    header: the app loads one facing at a time from the card (render.c hero_view) instead of holding the arrays in RAM.
                    Only for 8 dirs + --fold-mirror (the hero).
  --lv-png OUTDIR   also write OUTDIR/<dirname>/f<i>.lv.png (+.json); ax = W//2, ay = H-1
  --tlight/--tmid   luminance thresholds (default 95 / 40)
  --adaptive        greyscale art (PixelLab v3): lvlib.adaptive_levels instead of the colour/luminance rule - the lighter tones of each
                    direction (all its frames pooled, no flicker in a walk cycle) are k-means clustered into levels 0/1/2 (brightest = opaque white), lum <= --dark-cut
                    and the silhouette edge -> 3 (same as `levels = "adaptive"` in assets/catalog.toml, see tools/spritec.py)
  --fixed-box       no crop: the box is the whole input canvas (the frames are already placed in the final W x H, e.g. tools/mkbike.py -> hero_bike.bin)
  --dark-cut N      dark threshold of --adaptive (default 40)
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lvlib  # noqa: E402
from PIL import Image  # noqa: E402

ORDER8 = [
    "south",
    "south-east",
    "east",
    "north-east",
    "north",
    "north-west",
    "west",
    "south-west",
]
ORDER4 = ["south", "east", "north", "west"]
MIRROR = {"west": "east", "north-west": "north-east", "south-west": "south-east"}


def frames_for(root, name):
    sub = os.path.join(root, name)
    if os.path.isdir(sub):
        fs = sorted(f for f in os.listdir(sub) if f.endswith(".png"))
        return [os.path.join(sub, f) for f in fs]
    return [os.path.join(root, name + ".png")]


def load_grid(
    root, order, mirror_west, nframes, t_light, t_mid, skin=True, adaptive=None
):
    """-> grid[dir][frame] = (levels, mask), all frames of all directions (same canvas size).
    adaptive = dark_cut: lvlib.adaptive_levels with one tone mapping per direction, pooled over its frames (greyscale art).
    """
    cache = {}

    def conv(name):
        if name not in cache:
            fs = frames_for(root, name)
            if (
                adaptive is not None
            ):  # one tone mapping per direction (pooled over its frames: no flicker in a walk cycle)
                ims = [Image.open(f) for f in fs]
                tones = lvlib.adaptive_tones(ims, adaptive)
                cache[name] = [lvlib.adaptive_levels(im, adaptive, tones) for im in ims]
            else:
                cache[name] = [
                    lvlib.levels_from_rgba(Image.open(f), t_light, t_mid, skin)
                    for f in fs
                ]
        return cache[name]

    grid = []
    for n in order:
        if mirror_west and n in MIRROR:
            grid.append(
                [(lvlib.flip_h(lv), lvlib.flip_h(mk)) for lv, mk in conv(MIRROR[n])]
            )
        else:
            grid.append(conv(n))
    nf = min(len(g) for g in grid)
    if nframes:
        nf = min(nf, nframes)
    return [g[:nf] for g in grid]


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    ap.add_argument("root")
    ap.add_argument("out")
    ap.add_argument("pfx")
    ap.add_argument("preview", nargs="?")
    ap.add_argument("--dirs", type=int, choices=(4, 8), default=8)
    ap.add_argument("--frames", type=int, default=0)
    ap.add_argument("--mirror-west", action="store_true")
    ap.add_argument("--fold-mirror", action="store_true")
    ap.add_argument("--bin", metavar="FILE")
    ap.add_argument("--lv-png", metavar="OUTDIR")
    ap.add_argument("--adaptive", action="store_true")
    ap.add_argument("--fixed-box", action="store_true")
    ap.add_argument("--dark-cut", type=int, default=40)
    ap.add_argument("--tlight", type=int, default=lvlib.T_LIGHT)
    ap.add_argument("--tmid", type=int, default=lvlib.T_MID)
    a = ap.parse_args()

    order = ORDER8 if a.dirs == 8 else ORDER4
    grid = load_grid(
        a.root,
        order,
        a.mirror_west,
        a.frames,
        a.tlight,
        a.tmid,
        adaptive=a.dark_cut if a.adaptive else None,
    )
    nf = len(grid[0])
    nd, pfx = len(order), a.pfx
    if a.fixed_box:
        mk0 = grid[0][0][1]
        x0, y0, x1, y1 = 0, 0, len(mk0[0]) - 1, len(mk0) - 1
    else:
        x0, y0, x1, y1 = lvlib.shared_bbox([mk for g in grid for _, mk in g])
    if a.fold_mirror:
        if a.dirs != 8:
            sys.exit("--fold-mirror needs --dirs 8")
        for w_name, e_name in MIRROR.items():
            gw, ge = grid[order.index(w_name)], grid[order.index(e_name)]
            for f, ((lw, mw), (le, me)) in enumerate(zip(gw, ge)):
                if lvlib.flip_h(le) != lw or lvlib.flip_h(me) != mw:
                    sys.exit(
                        f"--fold-mirror: {w_name} frame {f} is not the mirror of {e_name}"
                    )
        keep = [i for i, n in enumerate(order) if n not in MIRROR]
        grid = [grid[i] for i in keep]
        order = [order[i] for i in keep]
        nd = len(order)
    W, H = x1 - x0 + 1, y1 - y0 + 1
    bpr, mbpr = (W + 3) // 4, (W + 7) // 8
    lines = [
        f"// generated by tools/pl2h.py from {a.root}",
        f"#define {pfx}_W {W}",
        f"#define {pfx}_H {H}",
        f"#define {pfx}_BPR {bpr}",
        f"#define {pfx}_MBPR {mbpr}",
        f"#define {pfx}_FRAMES {nf}",
        *(
            [
                f"#define {pfx}_DIRS {nd} // folded: south, south-east, east, north-east, north; west ones = flipped east ones"
            ]
            if a.fold_mirror
            else [f"#define {pfx}_DIRS {nd}"]
        ),
        f"static const unsigned char {pfx.lower()}_frames[{nd}][{nf}][{bpr * H}] = {{",
    ]
    mlines = [
        f"static const unsigned char {pfx.lower()}_mask[{nd}][{nf}][{mbpr * H}] = {{"
    ]
    for g in grid:
        fl = [
            "{" + ",".join(map(str, lvlib.pack_2bpp(lv, x0, y0, x1, y1))) + "}"
            for lv, _ in g
        ]
        ml = [
            "{" + ",".join(map(str, lvlib.pack_mask(mk, x0, y0, x1, y1))) + "}"
            for _, mk in g
        ]
        lines.append("  {" + ",".join(fl) + "},")
        mlines.append("  {" + ",".join(ml) + "},")
    lines.append("};")
    mlines.append("};")
    if a.bin:
        if not a.fold_mirror:
            sys.exit("--bin needs --fold-mirror (hero.bin holds the 5 stored facings)")
        blob = b"NHRO" + bytes([1, W, H, bpr, mbpr, nf, nd, 0, 0, 0, 0, 0])
        for g in grid:
            blob += b"".join(bytes(lvlib.pack_2bpp(lv, x0, y0, x1, y1)) for lv, _ in g)
            blob += b"".join(bytes(lvlib.pack_mask(mk, x0, y0, x1, y1)) for _, mk in g)
        with open(a.bin, "wb") as f:
            f.write(blob)
        lines = lines[:7] + [
            "// the sprite data lives in "
            + os.path.basename(a.bin)
            + " (pl2h.py --bin); render.c hero_view loads one facing at a time"
        ]
        mlines = []
        print(f"wrote {a.bin}: {len(blob)} B")
    with open(a.out, "w") as f:
        f.write("\n".join(lines + mlines) + "\n")
    print(f"wrote {a.out}: {nd} dirs x {nf} frames, {W}x{H}")

    if a.lv_png:
        nw = nl = 0
        for name, g in zip(order, grid):
            for i, (lv, mk) in enumerate(g):
                p = os.path.join(a.lv_png, name, f"f{i}.lv.png")
                if lvlib.write_lv_png(
                    p,
                    lvlib.crop(lv, x0, y0, x1, y1),
                    lvlib.crop(mk, x0, y0, x1, y1),
                    W // 2,
                    H - 1,
                ):
                    nw += 1
                else:
                    nl += 1
        print(
            f"wrote {nw} .lv.png to {a.lv_png}"
            + (f" ({nl} locked, skipped)" if nl else "")
        )

    if a.preview:  # LCD-look preview: frame 0 of each direction
        S = 8
        sheet = Image.new("RGB", (nd * (W * S + 8) + 8, H * S + 16), (30, 30, 30))
        for i, g in enumerate(grid):
            lv, _ = g[0]
            sheet.paste(
                lvlib.lcd_image(lvlib.crop(lv, x0, y0, x1, y1), None, S),
                (8 + i * (W * S + 8), 8),
            )
        sheet.save(a.preview)


if __name__ == "__main__":
    main()
