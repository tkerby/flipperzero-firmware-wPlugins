#!/usr/bin/env python3
"""Pixel-art aware sprite fitting for the Flipper (4 levels, 0 = transparent).

1. detect the source's native pixel grid (AI pixel art is drawn in big blocks)
   and sample one value per block -> the clean native sprite, no blur;
2. reduce the native sprite to the target box with feature-preserving rules:
   outline from the alpha mask, dark features win over light fill, fixed tones.
"""
from PIL import Image

LEVEL_RGB_BG = (255, 139, 41)
LEVEL_RGB_INK = (24, 14, 6)


def _grid_step(gray, alpha, axis):
    """Most likely block size along an axis, from where colors change."""
    w, h = gray.size
    g, a = gray.load(), alpha.load()
    edges = []
    if axis == 0:
        for y in range(0, h, 2):
            for x in range(1, w):
                if a[x, y] > 128 and a[x - 1, y] > 128 and abs(g[x, y] - g[x - 1, y]) > 40:
                    edges.append(x)
    else:
        for x in range(0, w, 2):
            for y in range(1, h):
                if a[x, y] > 128 and a[x, y - 1] > 128 and abs(g[x, y] - g[x, y - 1]) > 40:
                    edges.append(y)
    if not edges:
        return 1, 0
    best = []
    for k in range(6, 40):
        hist = [0] * k
        for e in edges:
            hist[e % k] += 1
        ph = max(range(k), key=lambda i: hist[i] + hist[(i + 1) % k] + hist[i - 1])
        score = (hist[ph] + hist[(ph + 1) % k] + hist[ph - 1]) / len(edges)
        best.append((score, k, ph))
    top = max(s for s, _, _ in best)
    # largest step that still explains the edges (smaller divisors also fit)
    s, k, ph = max((b for b in best if b[0] >= top * 0.85), key=lambda b: b[1])
    return k, ph


def native(path, mirror=False):
    """Return (gray, alpha) lists of the clean native-resolution sprite."""
    im = Image.open(path).convert("RGBA")
    im = im.crop(im.split()[3].getbbox())
    if mirror:
        im = im.transpose(Image.FLIP_LEFT_RIGHT)
    alpha = im.split()[3]
    bg = Image.new("RGBA", im.size, (255, 255, 255, 255))
    bg.alpha_composite(im)
    gray = bg.convert("L")
    kx, px = _grid_step(gray, alpha, 0)
    ky, py = _grid_step(gray, alpha, 1)
    k = round((kx + ky) / 2)
    w, h = gray.size
    g, a = gray.load(), alpha.load()
    cols = list(range(px % k + k // 2, w, k))
    rows = list(range(py % k + k // 2, h, k))
    G = [[g[x, y] for x in cols] for y in rows]
    A = [[a[x, y] for x in cols] for y in rows]
    return G, A, k


def tone(v):
    # fixed tones (white shows the light ground through, like sprite color 0)
    return 0 if v > 205 else (1 if v > 150 else (2 if v > 80 else 3))


def fit(path, FW, FH, mirror=False):
    G, A, _ = native(path, mirror)
    nh, nw = len(G), len(G[0])
    sc = min(FW / nw, FH / nh, 1.0)
    tw, th = max(1, round(nw * sc)), max(1, round(nh * sc))
    out = [[0] * FW for _ in range(FH)]
    ox, oy = (FW - tw) // 2, FH - th
    mask = [[False] * FW for _ in range(FH)]
    for ty in range(th):
        y0, y1 = int(ty / sc), max(int(ty / sc) + 1, int((ty + 1) / sc))
        for tx in range(tw):
            x0, x1 = int(tx / sc), max(int(tx / sc) + 1, int((tx + 1) / sc))
            cells = [(G[y][x], A[y][x]) for y in range(y0, min(y1, nh)) for x in range(x0, min(x1, nw))]
            opaque = [v for v, al in cells if al > 128]
            if len(opaque) * 2 < len(cells):
                continue
            mask[oy + ty][ox + tx] = True
            lv = [tone(v) for v in opaque]
            # a dark feature (eye, seam, glove) survives if present in the block
            out[oy + ty][ox + tx] = 3 if lv.count(3) * 3 >= len(lv) else round(sum(lv) / len(lv))
    # 1-px black outline on the silhouette edge (inside the mask)
    for y in range(FH):
        for x in range(FW):
            if mask[y][x] and any(not (0 <= x + dx < FW and 0 <= y + dy < FH) or not mask[y + dy][x + dx]
                                  for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                out[y][x] = 3
    return out


def preview(levels, scale=8):
    """LCD-looking preview of a level grid (0..3) on the light ground."""
    H, W = len(levels), len(levels[0])
    img = Image.new("RGB", (W * scale, H * scale), tuple(int(c * 0.82) for c in LEVEL_RGB_BG))
    p = img.load()
    for y in range(H):
        for x in range(W):
            a = levels[y][x] / 3
            col = tuple(int(LEVEL_RGB_BG[i] * (1 - a) + LEVEL_RGB_INK[i] * a) for i in range(3))
            for dy in range(scale - 1):
                for dx in range(scale - 1):
                    p[x * scale + dx, y * scale + dy] = col
    return img
