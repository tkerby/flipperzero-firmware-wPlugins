#!/usr/bin/env python3
"""Any RGBA PNG (e.g. a PixelLab building) -> <out>.lv.png + .json (4 levels + mask).

  img2lv.py in.png out.lv.png [--crop] [--anchor x,y | bottom-center | center | top-left]
            [--tlight 95] [--tmid 40] [--skin] [--force]

Same rules as pl2h (alpha<128 transparent, luminance thresholds, 1-px black silhouette);
the skin rule is OFF unless --skin.  --crop trims to the opaque bounding box (the anchor is
interpreted in the cropped image).  Default anchor: bottom-center.  A `.locked` sibling
makes the tool refuse to overwrite unless --force.
"""
import argparse
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lvlib  # noqa: E402
from PIL import Image  # noqa: E402


def parse_anchor(s, w, h):
    s = s.strip().lower()
    if s == "bottom-center":
        return w // 2, h - 1
    if s == "center":
        return w // 2, h // 2
    if s == "top-left":
        return 0, 0
    x, y = s.split(",")
    return int(x), int(y)


def convert(src, dst, do_crop=False, anchor="bottom-center", t_light=lvlib.T_LIGHT, t_mid=lvlib.T_MID,
            skin=False, force=False):
    lv, mk = lvlib.levels_from_rgba(Image.open(src), t_light, t_mid, skin)
    if do_crop:
        x0, y0, x1, y1 = lvlib.mask_bbox(mk) or (0, 0, 0, 0)
        lv, mk = lvlib.crop(lv, x0, y0, x1, y1), lvlib.crop(mk, x0, y0, x1, y1)
    h, w = len(lv), len(lv[0])
    ax, ay = parse_anchor(anchor, w, h)
    ok = lvlib.write_lv_png(dst, lv, mk, ax, ay, force=force)
    return ok, w, h, ax, ay


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("src")
    ap.add_argument("dst")
    ap.add_argument("--crop", action="store_true")
    ap.add_argument("--anchor", default="bottom-center")
    ap.add_argument("--tlight", type=int, default=lvlib.T_LIGHT)
    ap.add_argument("--tmid", type=int, default=lvlib.T_MID)
    ap.add_argument("--skin", action="store_true")
    ap.add_argument("--force", action="store_true")
    a = ap.parse_args()
    if not a.dst.endswith(".lv.png"):
        ap.error("dst must end with .lv.png")
    ok, w, h, ax, ay = convert(a.src, a.dst, a.crop, a.anchor, a.tlight, a.tmid, a.skin, a.force)
    print(f"wrote {a.dst} {w}x{h} anchor ({ax},{ay})" if ok else f"{a.dst} is locked, skipped")


if __name__ == "__main__":
    main()
