#!/usr/bin/env python3
"""Hand-pixeled sprite frames (ASCII) -> level grids, LCD previews and C headers.

Frame file format:  "== name" starts a frame; rows use . o + #  (levels 0..3).
  pix.py preview <file.txt> <out.png>         LCD-looking sheet of every frame
  pix.py header  <file.txt> <out.h> <PFX> <name0,name1,...>   frames in that order
"""
import sys, os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sprite_fit import preview  # noqa: E402
from PIL import Image, ImageDraw

LEV = {".": 0, "o": 1, "+": 2, "#": 3}


def load(path):
    frames, name, rows = {}, None, []
    for line in open(path):
        line = line.rstrip("\n")
        if line.startswith("#") and not set(line) <= set(LEV):
            continue  # comment
        if line.startswith("=="):
            if name:
                frames[name] = rows
            name, rows = line[2:].strip(), []
        elif name and line:
            rows.append([LEV[c] for c in line])
    if name:
        frames[name] = rows
    for n, r in frames.items():
        w = {len(x) for x in r}
        if len(w) != 1:
            sys.exit(f"frame {n}: rows have different widths {sorted(w)}")
    return frames


def mirror(grid):
    return [row[::-1] for row in grid]


def pack(grid):
    h, w = len(grid), len(grid[0])
    bpr = (w + 3) // 4
    data = []
    for y in range(h):
        for b in range(bpr):
            byte = 0
            for k in range(4):
                x = b * 4 + k
                if x < w:
                    byte |= (grid[y][x] & 3) << (k * 2)
            data.append(byte)
    return data, bpr


def main():
    cmd, path = sys.argv[1], sys.argv[2]
    fr = load(path)
    if cmd == "preview":
        tiles = [(n, preview(g, 10)) for n, g in fr.items()]
        W = sum(t.width + 20 for _, t in tiles) + 20
        H = max(t.height for _, t in tiles) + 40
        sh = Image.new("RGB", (W, H), (30, 30, 30))
        d = ImageDraw.Draw(sh)
        x = 20
        for n, t in tiles:
            d.text((x, 8), n, fill=(255, 255, 255))
            sh.paste(t, (x, 28))
            x += t.width + 20
        sh.save(sys.argv[3])
        print("frames:", ", ".join(f"{n} {len(g[0])}x{len(g)}" for n, g in fr.items()))
    elif cmd == "header":
        # names: one per direction (0=down .. 7=down-left); "~name" = mirrored
        out, pfx, names = sys.argv[3], sys.argv[4], sys.argv[5].split(",")
        grids = [mirror(fr[n[1:]]) if n.startswith("~") else fr[n] for n in names]
        h, w = len(grids[0]), len(grids[0][0])
        bpr = (w + 3) // 4
        lines = [
            f"#define {pfx}_W {w}",
            f"#define {pfx}_H {h}",
            f"#define {pfx}_BPR {bpr}",
            f"#define {pfx}_FRAMES 1",
            f"static const unsigned char {pfx.lower()}_frames[{len(grids)}][1][{bpr * h}] = {{",
        ]
        for g in grids:
            data, _ = pack(g)
            lines.append("  {{" + ",".join(map(str, data)) + "}},")
        lines.append("};")
        open(out, "w").write("\n".join(lines) + "\n")
        print(f"wrote {out}: {len(grids)} dirs, {w}x{h}")


if __name__ == "__main__":
    main()
