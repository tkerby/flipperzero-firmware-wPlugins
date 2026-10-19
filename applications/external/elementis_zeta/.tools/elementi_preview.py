#!/usr/bin/env python3
"""Preview PixelLab Elementi sprites as they will look on the Flipper (4 grays).

Adaptive tone mapping: the sprite's own non-dark tones are split into 3 clusters
(1-D k-means) -> levels 0/1/2 (brightest = opaque white); very dark pixels and the
silhouette edge -> level 3. Run with python3 -I (inputs are downloaded data).

  elementi_preview.py <out.png> <dir1> [<dir2> ...]   (each dir has PixelLab direction PNGs)
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from PIL import Image, ImageDraw  # noqa: E402
import lvlib  # noqa: E402

DARK_CUT = lvlib.ADAPT_DARK_CUT
adaptive_levels = (
    lvlib.adaptive_levels
)  # lives in lvlib now (also used by spritec / pl2h)


def main():
    out, dirs = sys.argv[1], sys.argv[2:]
    tiles = []
    for d in dirs:
        for view in ("south-west", "north-east"):
            im = Image.open(os.path.join(d, view + ".png")).convert("RGBA")
            S = 3
            col = Image.new("RGB", (im.width * S, im.height * S), (220, 220, 220))
            big = im.resize((im.width * S, im.height * S), Image.NEAREST)
            col.paste(big, (0, 0), big)
            tiles.append((f"{os.path.basename(d)} {view} color", col))
            lv, mk = adaptive_levels(im)
            x0, y0, x1, y1 = lvlib.mask_bbox(mk)
            tiles.append(
                (
                    f"{os.path.basename(d)} {view} flipper {x1-x0+1}x{y1-y0+1}",
                    lvlib.lcd_image(
                        lvlib.crop(lv, x0, y0, x1, y1),
                        lvlib.crop(mk, x0, y0, x1, y1),
                        scale=4,
                    ),
                )
            )
    W = max(t[1].width for t in tiles)
    H = max(t[1].height for t in tiles)
    cols = 4
    rows = (len(tiles) + cols - 1) // cols
    sheet = Image.new("RGB", (cols * (W + 10) + 10, rows * (H + 24) + 10), (30, 30, 30))
    dr = ImageDraw.Draw(sheet)
    for k, (label, im) in enumerate(tiles):
        x = 10 + (k % cols) * (W + 10)
        y = 10 + (k // cols) * (H + 24)
        dr.text((x, y), label, fill=(255, 255, 255))
        sheet.paste(im, (x, y + 14))
    sheet.save(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
