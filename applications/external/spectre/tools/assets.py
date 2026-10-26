# Generates icon.png (10x10) and the 20x20 ghost XBM bytes used by the boot screen.
import re, sys
from PIL import Image

GHOST = """
......########......
....############....
...##############...
..################..
..################..
.####..######..####.
.###....####....###.
.###....####....###.
.####..######..####.
.##################.
.##################.
.##################.
.##################.
.##################.
.##################.
.##################.
.####.####.####.###.
.###...##...##...##.
.##.....#.....#...#.
....................
""".strip("\n").splitlines()

ICON = """
...####...
..######..
.########.
.##.##.##.
.##.##.##.
.########.
.########.
.########.
.##.##.##.
.#..#..#..
""".strip("\n").splitlines()

def xbm(rows):
    out = []
    for row in rows:
        byte = 0
        for i, ch in enumerate(row):
            if ch == "#":
                byte |= 1 << (i % 8)
            if i % 8 == 7 or i == len(row) - 1:
                out.append(byte)
                byte = 0
    return out

bits = xbm(GHOST)
lines = []
for i in range(0, len(bits), 15):
    lines.append("    " + ", ".join(f"0x{b:02x}" for b in bits[i:i + 15]) + ",")
block = "\n".join(lines)
src = open("spectre.c").read()
src, n = re.subn(
    r"(static const uint8_t ghost_bits\[\] = \{\n    // [^\n]*\n)(?:    0x[^\n]*\n)+(\};)",
    lambda m: m.group(1) + block + "\n" + m.group(2),
    src,
)
assert n == 1, "ghost_bits not found"
open("spectre.c", "w").write(src)

img = Image.new("1", (10, 10), 1)
for y, row in enumerate(ICON):
    for x, ch in enumerate(row):
        if ch == "#":
            img.putpixel((x, y), 0)
img.save("icon.png")
print(f"ghost: {len(bits)} bytes, icon written")
