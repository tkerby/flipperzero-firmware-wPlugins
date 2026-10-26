# Generates icon.png (10x10): a five-pointed star in a ring, the site's temple line.
from PIL import Image
ICON = """
....##....
...#..#...
.##....##.
#..#..#..#
.#..##..#.
.#..##..#.
#..#..#..#
.##....##.
...#..#...
....##....
""".strip("\n").splitlines()
img = Image.new("1", (10, 10), 1)
for y, row in enumerate(ICON):
    for x, ch in enumerate(row):
        if ch == "#":
            img.putpixel((x, y), 0)
img.save("icon.png")
print("icon written")
