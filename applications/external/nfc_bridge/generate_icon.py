"""Regenerates icon.png: the 10x10, 1-bit app icon the Flipper menu shows (fap_icon in application.fam).

Flipper icons draw BLACK pixels and leave WHITE pixels transparent, so the picture is black on white.
Run from this folder:  python3 generate_icon.py   (needs Pillow)
"""
from pathlib import Path

from PIL import Image

# An NFC card with radio waves over it. '#' = drawn pixel.
ART = [
    "..######..",
    ".#......#.",
    "...####...",
    "..#....#..",
    "....##....",
    "..........",
    "##########",
    "#........#",
    "#........#",
    "##########",
]

img = Image.new("1", (10, 10), 1)  # mode '1': 1 = white (transparent on the Flipper)
for y, row in enumerate(ART):
    for x, ch in enumerate(row):
        if ch == "#":
            img.putpixel((x, y), 0)  # 0 = black (drawn)

out = Path(__file__).with_name("icon.png")
img.save(out)
print(f"Icon generated at {out}")
