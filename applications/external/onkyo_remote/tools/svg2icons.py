#!/usr/bin/env python3
"""
Rasterise res/*.svg into the 1-bit icons in images/.

The sources are 24x24 Material Design icons. Each is rendered with an 8x
supersample a little larger than its target, then centre-cropped -- which
scales the 20-unit live area up to fill the cell instead of leaving MDI's
padding in place -- and thresholded. The 35% threshold on the button icons
keeps thin strokes (the power ring) from breaking up on a 1-bit screen; the
10x10 app icon needs a higher one to stop its two driver cutouts filling in.

    python3 tools/svg2icons.py [--preview]

Needs rsvg-convert and Pillow. Re-run after changing anything in res/, then
rebuild: the icons are compiled into onkyo_remote_icons.h by fap_icon_assets.
"""

import io
import os
import subprocess
import sys

from PIL import Image

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SRC = os.path.join(ROOT, "res")
DST = os.path.join(ROOT, "images")

SUPERSAMPLE = 8

# source -> (output name, final size, render size, threshold)
# 16px buttons match ONKYO_ICON_SIZE in onkyo_layout.c; onkyo_10px is fap_icon,
# which the build rejects at any size but 10x10.
ICONS = {
    "power.svg": ("power_16x16", 16, 18, 0.35),
    "volume-off.svg": ("mute_16x16", 16, 18, 0.35),
    "play-pause.svg": ("play_pause_16x16", 16, 18, 0.35),
    "volume-minus.svg": ("vol_down_16x16", 16, 18, 0.35),
    "volume-plus.svg": ("vol_up_16x16", 16, 18, 0.35),
    "skip-previous.svg": ("prev_16x16", 16, 18, 0.35),
    "skip-next.svg": ("next_16x16", 16, 18, 0.35),
    "equalizer.svg": ("flat_16x16", 16, 18, 0.35),
    "speaker.svg": ("onkyo_10px", 10, 12, 0.45),
}


def coverage(svg, size, render):
    """Render one SVG and return per-pixel ink coverage, 0..1."""
    png = subprocess.run(
        [
            "rsvg-convert",
            "-w", str(render * SUPERSAMPLE),
            "-h", str(render * SUPERSAMPLE),
            os.path.join(SRC, svg),
        ],
        capture_output=True,
        check=True,
    ).stdout
    alpha = Image.open(io.BytesIO(png)).convert("RGBA").getchannel("A")
    alpha = alpha.resize((render, render), Image.BOX)
    offset = (render - size) // 2
    return [
        [alpha.getpixel((x, y)) / 255 for x in range(offset, offset + size)]
        for y in range(offset, offset + size)
    ]


def main():
    preview = "--preview" in sys.argv
    for svg, (name, size, render, threshold) in ICONS.items():
        rows = [[v >= threshold for v in row] for row in coverage(svg, size, render)]
        image = Image.new("1", (size, size), 1)  # white background, black ink
        for y, row in enumerate(rows):
            for x, ink in enumerate(row):
                if ink:
                    image.putpixel((x, y), 0)
        image.save(os.path.join(DST, name + ".png"))
        if preview:
            print(f"--- {name} ---")
            for row in rows:
                print("".join("#" if v else "." for v in row))
    print(f"wrote {len(ICONS)} icons to {os.path.relpath(DST, ROOT)}/")


if __name__ == "__main__":
    main()
