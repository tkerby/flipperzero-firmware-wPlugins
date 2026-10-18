#!/usr/bin/env python3
"""Spinning Cristal for the title screen: a real 3D render (convex polyhedron, orthographic camera,
flat-shaded facets) rasterized at 8x and reduced to the Flipper's 4 grays at the FINAL pixel size.

A hexagonal crystal (pointed top, short pointed bottom) repeats every 60 degrees around its axis, so
frames over 0..60 degrees loop seamlessly into a continuous spin.

  python3 -I tools/crystal3d.py <out.png> [height ...]   -> preview sheet (LCD look) of every frame
Library: frames(height, n) -> list of (levels, mask) at that height.
"""
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image, ImageDraw  # noqa: E402

SS = 8  # supersampling


def crystal_mesh(sides=6, r=0.62, top=0.85, ring_hi=0.75, ring_lo=-0.75, bottom=-0.85):
    """(vertices, faces) of the crystal; faces are CCW seen from outside."""
    v = []
    for k in range(sides):
        a = 2 * math.pi * k / sides
        v.append((r * math.cos(a), ring_hi, r * math.sin(a)))
    for k in range(sides):
        a = 2 * math.pi * k / sides
        v.append((r * math.cos(a), ring_lo, r * math.sin(a)))
    v.append((0.0, top + ring_hi, 0.0))      # apex
    v.append((0.0, bottom + ring_lo, 0.0))   # bottom point
    A, B = 2 * sides, 2 * sides + 1
    f = []
    for k in range(sides):
        n = (k + 1) % sides
        f.append((k, n, sides + n, sides + k))  # side quad
        f.append((A, n, k))                      # top facet
        f.append((B, sides + k, sides + n))      # bottom facet
    return v, f


def _norm(a):
    l = math.sqrt(sum(c * c for c in a)) or 1.0
    return tuple(c / l for c in a)


def _sub(a, b):
    return tuple(x - y for x, y in zip(a, b))


def _cross(a, b):
    return (a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0])


def _dot(a, b):
    return sum(x * y for x, y in zip(a, b))


def render(height, angle_deg, tilt_deg=18.0, light=(-0.55, 0.75, 0.6), tones=None, width_ratio=0.62):
    """One frame: (levels, mask) lists of rows. height = final pixel height of the silhouette
    (outline included). tones: shade thresholds (bright -> level 0 .. dark -> level 2)."""
    tones = tones or (0.80, 0.52)
    v, faces = crystal_mesh()
    a = math.radians(angle_deg)
    t = math.radians(tilt_deg)
    ca, sa, ct, st = math.cos(a), math.sin(a), math.cos(t), math.sin(t)

    def xf(p):
        x, y, z = p
        x, z = x * ca - z * sa, x * sa + z * ca          # spin around the vertical axis
        y, z = y * ct - z * st, y * st + z * ct          # tilt the camera down (we see the top facets)
        return (x * width_ratio / 0.62, y, z)

    w3 = [xf(p) for p in v]
    ys = [p[1] for p in w3]
    xs = [p[0] for p in w3]
    span = max(ys) - min(ys)
    inner = height - 2                                   # 1 px outline above and below
    s = inner * SS / span
    W = int(math.ceil((max(xs) - min(xs)) * s / SS)) + 4
    H = height + 2
    cx = W * SS / 2
    top_y = SS * 1.0
    L = _norm(light)
    view = (0.0, 0.0, 1.0)  # camera looks along -z; faces with normal.z > 0 face us
    img = Image.new("L", (W * SS, H * SS), 255)
    d = ImageDraw.Draw(img)
    vis = []
    for fc in faces:
        p = [w3[i] for i in fc]
        n = _norm(_cross(_sub(p[1], p[0]), _sub(p[2], p[0])))
        c = tuple(sum(q[i] for q in p) / len(p) for i in range(3))
        if _dot(n, c) < 0:  # make it point outwards (the mesh is convex around the origin)
            n = tuple(-x for x in n)
        if n[2] <= 0:       # x right, y up, z towards the viewer: only faces turned to us are drawn
            continue
        lam = max(0.0, _dot(n, L))
        h = _norm((L[0] + view[0], L[1] + view[1], L[2] + view[2]))
        spec = max(0.0, _dot(n, h)) ** 24
        vis.append((0.30 + 0.75 * lam + 0.8 * spec, p))
    # tones by rank in this frame (always white + mid + dark on screen): the brightest facet(s) -> 0, the darker
    # 40 % -> 2, the rest -> 1 (a swap of ranks while it spins reads as the gem catching the light)
    order = sorted(range(len(vis)), key=lambda i: -vis[i][0])
    lvl_of = {}
    for r, i in enumerate(order):
        lvl_of[i] = 0 if (r == 0 or vis[i][0] >= tones[0]) else (2 if r >= len(vis) * 0.6 else 1)
    for i, (_, p) in enumerate(vis):
        poly = [(cx + q[0] * s, top_y + (max(ys) - q[1]) * s) for q in p]
        d.polygon(poly, fill=lvl_of[i] * 60)  # 0, 60, 120 -> levels 0..2; background 255
    # reduce: coverage >= half -> inside; tone = most common facet level among the samples
    px = img.load()
    lv = [[0] * W for _ in range(H)]
    mk = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            cnt = [0, 0, 0]
            for yy in range(y * SS, y * SS + SS):
                for xx in range(x * SS, x * SS + SS):
                    g = px[xx, yy]
                    if g != 255:
                        cnt[g // 60] += 1
            tot = sum(cnt)
            if tot * 2 >= SS * SS:
                mk[y][x] = 1
                lv[y][x] = max(range(3), key=lambda k: cnt[k])
    # 1 px black outline outside the silhouette
    out = [row[:] for row in mk]
    for y in range(H):
        for x in range(W):
            if mk[y][x]:
                continue
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                xx, yy = x + dx, y + dy
                if 0 <= xx < W and 0 <= yy < H and mk[yy][xx]:
                    out[y][x] = 1
                    lv[y][x] = 3
                    break
    # crop to the mask
    rows = [y for y in range(H) if any(out[y])]
    cols = [x for x in range(W) if any(out[y][x] for y in range(H))]
    y0, y1, x0, x1 = rows[0], rows[-1], cols[0], cols[-1]
    return [r[x0:x1 + 1] for r in lv[y0:y1 + 1]], [r[x0:x1 + 1] for r in out[y0:y1 + 1]]


def frames(height, n=12, **kw):
    return [render(height, 60.0 * k / n, **kw) for k in range(n)]


def main():
    import lvlib
    out = sys.argv[1]
    hs = [int(a) for a in sys.argv[2:]] or [16, 24, 32, 40]
    rows = []
    for h in hs:
        fr = frames(h)
        W = max(len(f[0][0]) for f in fr)
        rows.append((fr, W, h))
    scale = 6
    sheet_w = max(len(fr) * (W + 3) for fr, W, h in rows) * scale
    sheet_h = sum((h + 6) for _, _, h in rows) * scale
    sheet = Image.new("RGB", (sheet_w, sheet_h), (40, 40, 40))
    y = 0
    for fr, W, h in rows:
        for k, (lv, mk) in enumerate(fr):
            # on a white field so the outline reads like on the screen
            fw = W + 2
            field = [[0] * fw for _ in range(h + 4)]
            ox = (fw - len(lv[0])) // 2
            for yy in range(len(lv)):
                for xx in range(len(lv[0])):
                    if mk[yy][xx]:
                        field[yy + 2][xx + ox] = lv[yy][xx]
            sheet.paste(lvlib.lcd_image(field, None, scale=scale, grid=False), (k * (W + 3) * scale, y))
        y += (h + 6) * scale
    sheet.save(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
