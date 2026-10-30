#!/usr/bin/env python3
"""Close-up preview of Profesora Lin's three Crystals (LCD look) -> build/preview/crystals.png.

Top: the three crystal sprites (assets/art/npcs/crystal_*/south.lv.png) at 8x, then 1x/3x real-size strips; below: one lab
frame from the host simulation (needs build/host_world/hw_main, `make -C tools/host_world`; skipped when missing).
Usage: python3 tools/crystals_preview.py   (after tools/build_assets.sh)"""
import os
import subprocess
import sys

from PIL import Image

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from lvlib import GRAYS, lcd_image, read_lv_png, LCD_BG  # noqa: E402

NAMES = ("crystal_fire", "crystal_water", "crystal_plant")
SCRIPT = "m2,0 K0,1 F11 w300 p4,2,0 w300 s"  # hero beside the bench: all three crystals in view


def lab_frame():
    hw = os.path.join(ROOT, "build", "host_world", "hw_main")
    if not os.path.exists(hw):
        return None
    out = os.path.join(ROOT, "build", "preview", "_crystal_lab")
    cwd = os.path.join(ROOT, "tools", "host_world")
    subprocess.run(
        [hw, out, "lab", SCRIPT, "1", "1", "1", "frame"],
        cwd=cwd,
        check=True,
        capture_output=True,
    )
    im = (
        Image.open(out + "_frame.png").convert("L").crop((0, 0, 128, 64))
    )  # 128 x 64 without the sheet padding
    os.remove(out + "_frame.png")
    p = im.load()
    inv = {g: i for i, g in enumerate(GRAYS)}
    lv = [
        [inv[min(GRAYS, key=lambda g: abs(g - p[x, y]))] for x in range(128)]
        for y in range(64)
    ]
    return lcd_image(lv, None, scale=4)


def main():
    os.makedirs(os.path.join(ROOT, "build", "preview"), exist_ok=True)
    tiles = []
    for n in NAMES:
        lv, mk, meta = read_lv_png(
            os.path.join(ROOT, "assets", "art", "npcs", n, "south.lv.png")
        )
        tiles.append(lcd_image(lv, mk, scale=8, ground=0))
    lab = lab_frame()
    pad = 16
    W = max(
        sum(t.width for t in tiles) + pad * (len(tiles) + 1),
        (lab.width if lab else 0) + 2 * pad,
    )
    H = pad + max(t.height for t in tiles) + pad + (lab.height + pad if lab else 0)
    out = Image.new("RGB", (W, H), LCD_BG)
    x = pad
    for t in tiles:
        out.paste(t, (x, pad))
        x += t.width + pad
    if lab:
        out.paste(lab, ((W - lab.width) // 2, pad + tiles[0].height + pad))
    path = os.path.join(ROOT, "build", "preview", "crystals.png")
    out.save(path)
    print("wrote", path)


if __name__ == "__main__":
    main()
