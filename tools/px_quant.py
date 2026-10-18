#!/usr/bin/env python3
"""Native-resolution render -> Flipper levels (no resizing).
Per-pixel tone mapping with contrast stretch inside the silhouette, plus a
1-px black outline on the silhouette edge."""
from PIL import Image


def quantize(path, bands=(0.62, 0.36), outline=True):
    im = Image.open(path).convert("RGBA")
    W, H = im.size
    px = im.load()
    lum = {}
    for y in range(H):
        for x in range(W):
            r, g, b, a = px[x, y]
            if a >= 128:
                lum[(x, y)] = 0.299 * r + 0.587 * g + 0.114 * b
    out = [[0] * W for _ in range(H)]
    if not lum:
        return out
    vals = sorted(lum.values())
    lo, hi = vals[int(len(vals) * 0.03)], vals[int(len(vals) * 0.97) - 1]
    span = max(1.0, hi - lo)
    for (x, y), v in lum.items():
        t = min(1.0, max(0.0, (v - lo) / span))  # 1 = brightest
        out[y][x] = 1 if t > bands[0] else (2 if t > bands[1] else 3)
    if outline:
        for (x, y) in lum:
            if any((x + dx, y + dy) not in lum for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[y][x] = 3
    return out


def crop_to(levels, FW, FH):
    """Bottom-center the opaque content into an FWxFH box (crop only, no scaling)."""
    H, W = len(levels), len(levels[0])
    ys = [y for y in range(H) if any(levels[y])]
    xs = [x for x in range(W) if any(levels[y][x] for y in range(H))]
    if not ys:
        return [[0] * FW for _ in range(FH)]
    x0, x1, y0, y1 = min(xs), max(xs), min(ys), max(ys)
    out = [[0] * FW for _ in range(FH)]
    ox = (FW - (x1 - x0 + 1)) // 2
    oy = FH - (y1 - y0 + 1)
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            tx, ty = ox + x - x0, oy + y - y0
            if 0 <= tx < FW and 0 <= ty < FH:
                out[ty][tx] = levels[y][x]
    return out
