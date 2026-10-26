"""Shared sprite-level helpers (4-level, masked sprites) for the Nucleo content pipeline.

Representation: `levels` and `mask` are lists of rows (list[list[int]]), H rows of W ints.
levels 0..3 (0 white .. 3 black); mask 1 = opaque (level 0 then means opaque white).
Pixel formats follow contract section 2:
  - 2 bpp, 4 px/byte, pixel x at bits (x&3)*2, stride (w+3)//4
  - mask 1 bpp, LSB first, stride (w+7)//8
Interchange: `<name>.lv.png` (RGBA, grays 255/170/85/0 = levels 0..3, alpha 0/255) +
`<name>.json` {"w","h","ax","ay"}. `<name>.locked` = hand retouched, never regenerate.
"""

import json
import os
from PIL import Image

T_SKIN, T_LIGHT, T_MID = 150, 95, 40
GRAYS = (255, 170, 85, 0)
LCD_BG, LCD_INK = (255, 139, 41), (24, 14, 6)


# ---------------------------------------------------------------- typecast
def levels_from_rgba(img, t_light=T_LIGHT, t_mid=T_MID, skin=True, t_skin=T_SKIN):
    """RGBA image -> (levels, mask).  alpha<128 transparent; skin (r-b>30 and lum>=t_skin)
    -> 0 opaque; else lum>=t_light -> 1, >=t_mid -> 2, else 3; then every opaque pixel
    touching transparency (4-neighbour, or the image border) -> 3."""
    im = img.convert("RGBA")
    W, H = im.size
    p = im.load()
    lv = [[0] * W for _ in range(H)]
    mk = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            r, g, b, a = p[x, y]
            if a < 128:
                continue
            l = 0.299 * r + 0.587 * g + 0.114 * b
            mk[y][x] = 1
            is_skin = skin and r - b > 30 and l >= t_skin
            lv[y][x] = (
                0 if is_skin else (1 if l >= t_light else (2 if l >= t_mid else 3))
            )
    for y in range(H):
        for x in range(W):
            if mk[y][x] and any(
                not (0 <= x + dx < W and 0 <= y + dy < H) or not mk[y + dy][x + dx]
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
            ):
                lv[y][x] = 3
    return lv, mk


ADAPT_DARK_CUT = 70


def _lum_map(im):
    """{(x, y): luminance} of the opaque (alpha >= 128) pixels of an RGBA image."""
    W, H = im.size
    px = im.load()
    lum = {}
    for y in range(H):
        for x in range(W):
            r, g, b, a = px[x, y]
            if a >= 128:
                lum[(x, y)] = 0.299 * r + 0.587 * g + 0.114 * b
    return lum


def adaptive_tones(ims, dark_cut=ADAPT_DARK_CUT):
    """Tone clusters for `adaptive_levels`: 1-D k-means (3 clusters, 20 iterations) of the non-dark luminances
    (> dark_cut) pooled over every image in `ims` (an RGBA image or a list of them).  Returns (centres, level_of)
    or None when nothing is lighter than dark_cut."""
    if isinstance(ims, Image.Image):
        ims = [ims]
    light = [
        v for im in ims for v in _lum_map(im.convert("RGBA")).values() if v > dark_cut
    ]
    if not light:
        return None
    cs = [min(light), (min(light) + max(light)) / 2, max(light)]
    for _ in range(20):
        groups = [[], [], []]
        for v in light:
            groups[min(range(3), key=lambda c: abs(v - cs[c]))].append(v)
        cs = [sum(g) / len(g) if g else cs[i] for i, g in enumerate(groups)]
    order = sorted(range(3), key=lambda c: -cs[c])
    return cs, {order[0]: 0, order[1]: 1, order[2]: 2}


def quantile_tones(ims, splits, dark_cut=ADAPT_DARK_CUT):
    """Alternative to `adaptive_tones` for a direction whose k-means split is poor (e.g. a back view where hair, jacket
    and trousers all land in one cluster).  `splits` = [p_dark, p_mid] (0 < p_dark < p_mid < 1): the non-dark luminances
    (> dark_cut) pooled over every image in `ims` are cut at their quantiles q(p) = sorted[int(len * p)]:
    lum < q(p_dark) -> level 2, lum < q(p_mid) -> level 1, else level 0 (brightest).  Returns a `QuantileTones`
    for `adaptive_levels(..., tones=...)` (which classifies by these exact cuts), or None when nothing is lighter
    than dark_cut.  Silhouette edge and dark pixels -> 3 exactly as in `adaptive_levels`.
    """
    if isinstance(ims, Image.Image):
        ims = [ims]
    if not (len(splits) == 2 and 0 < splits[0] < splits[1] < 1):
        raise ValueError(
            f"quantile_tones: splits must be [p1, p2] with 0 < p1 < p2 < 1, got {splits!r}"
        )
    light = sorted(
        v for im in ims for v in _lum_map(im.convert("RGBA")).values() if v > dark_cut
    )
    if not light:
        return None
    q = [light[min(int(len(light) * p), len(light) - 1)] for p in splits]
    return QuantileTones((q[0], q[1]))


class QuantileTones(tuple):
    """(q_lo, q_hi) from `quantile_tones`; `adaptive_levels` maps lum < q_lo -> 2, lum < q_hi -> 1, else 0."""


def adaptive_levels(im, dark_cut=ADAPT_DARK_CUT, tones=None):
    """RGBA image -> (levels, mask) for art whose colour carries no information (greyscale) or whose tones differ
    a lot between sprites.  alpha<128 transparent; the sprite's own non-dark tones are split into 3 clusters
    (`adaptive_tones`) -> levels 0/1/2 (brightest = opaque white); lum <= dark_cut and the silhouette edge (an opaque
    pixel with a transparent 4-neighbour or on the image border) -> 3.  `tones` = result of adaptive_tones over a
    group of images, to give all of them one consistent mapping (default: this image alone).
    """
    im = im.convert("RGBA")
    W, H = im.size
    lum = _lum_map(im)
    lv = [[0] * W for _ in range(H)]
    mk = [[0] * W for _ in range(H)]
    if tones is None:
        tones = adaptive_tones(im, dark_cut)
    for (x, y), v in lum.items():
        mk[y][x] = 1
        if v <= dark_cut or tones is None:
            lv[y][x] = 3
        else:
            if isinstance(tones, QuantileTones):
                lv[y][x] = 2 if v < tones[0] else 1 if v < tones[1] else 0
            else:
                cs, level_of = tones
                lv[y][x] = level_of[min(range(3), key=lambda c: abs(v - cs[c]))]
    for x, y in lum:
        if any(
            (x + dx, y + dy) not in lum for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
        ):
            lv[y][x] = 3
    return lv, mk


def mask_bbox(mask):
    """Inclusive bbox (x0,y0,x1,y1) of set pixels, or None if empty."""
    x0 = y0 = 10**9
    x1 = y1 = -1
    for y, row in enumerate(mask):
        for x, v in enumerate(row):
            if v:
                x0, x1, y0, y1 = min(x0, x), max(x1, x), min(y0, y), max(y1, y)
    return None if x1 < 0 else (x0, y0, x1, y1)


def shared_bbox(masks):
    """Union (inclusive x0,y0,x1,y1) of the opaque bboxes of every mask in the list."""
    x0 = y0 = 10**9
    x1 = y1 = -1
    for m in masks:
        b = mask_bbox(m)
        if b:
            x0, y0, x1, y1 = min(x0, b[0]), min(y0, b[1]), max(x1, b[2]), max(y1, b[3])
    if x1 < 0:
        raise ValueError("shared_bbox: all masks are empty")
    return x0, y0, x1, y1


def flip_h(grid):
    return [row[::-1] for row in grid]


def crop(grid, x0, y0, x1, y1):
    return [row[x0 : x1 + 1] for row in grid[y0 : y1 + 1]]


# ---------------------------------------------------------------- packing
def pack_2bpp(levels, x0, y0, x1, y1):
    """Pack the inclusive box into 2 bpp bytes (row stride (w+3)//4)."""
    bpr = (x1 - x0 + 1 + 3) // 4
    out = bytearray()
    for y in range(y0, y1 + 1):
        for b in range(bpr):
            byte = 0
            for k in range(4):
                x = x0 + b * 4 + k
                if x <= x1:
                    byte |= (levels[y][x] & 3) << (k * 2)
            out.append(byte)
    return bytes(out)


def pack_mask(mask, x0, y0, x1, y1):
    """Pack the inclusive box into a 1 bpp LSB-first mask (row stride (w+7)//8)."""
    mbpr = (x1 - x0 + 1 + 7) // 8
    out = bytearray()
    for y in range(y0, y1 + 1):
        for b in range(mbpr):
            byte = 0
            for k in range(8):
                x = x0 + b * 8 + k
                if x <= x1 and mask[y][x]:
                    byte |= 1 << k
            out.append(byte)
    return bytes(out)


def unpack_2bpp(data, w, h):
    bpr = (w + 3) // 4
    return [
        [(data[y * bpr + (x >> 2)] >> ((x & 3) * 2)) & 3 for x in range(w)]
        for y in range(h)
    ]


def unpack_mask(data, w, h):
    mbpr = (w + 7) // 8
    return [
        [(data[y * mbpr + (x >> 3)] >> (x & 7)) & 1 for x in range(w)] for y in range(h)
    ]


# ---------------------------------------------------------------- .lv.png interchange
def lv_json_path(path):
    base = (
        path[: -len(".lv.png")]
        if path.endswith(".lv.png")
        else os.path.splitext(path)[0]
    )
    return base + ".json"


def lv_locked_path(path):
    return lv_json_path(path)[: -len(".json")] + ".locked"


def is_locked(path):
    return os.path.exists(lv_locked_path(path))


def read_lv_png(path):
    """-> (levels, mask, meta).  meta = {"w","h","ax","ay"} (json sidecar, default ax=w//2, ay=h-1)."""
    im = Image.open(path).convert("RGBA")
    W, H = im.size
    p = im.load()
    lv = [[0] * W for _ in range(H)]
    mk = [[0] * W for _ in range(H)]
    for y in range(H):
        for x in range(W):
            r, g, b, a = p[x, y]
            if a >= 128:
                mk[y][x] = 1
                lv[y][x] = min(3, max(0, int(round((255 - g) / 85.0))))
    meta = {"w": W, "h": H, "ax": W // 2, "ay": H - 1}
    jp = lv_json_path(path)
    if os.path.exists(jp):
        with open(jp) as f:
            j = json.load(f)
        meta["ax"], meta["ay"] = int(j.get("ax", meta["ax"])), int(
            j.get("ay", meta["ay"])
        )
        if (j.get("w", W), j.get("h", H)) != (W, H):
            raise ValueError(
                f"{jp}: json size {j.get('w')}x{j.get('h')} != png {W}x{H}"
            )
    return lv, mk, meta


def write_lv_png(path, levels, mask, ax, ay, force=False):
    """Write `.lv.png` + `.json`. Returns False (and writes nothing) if a `.locked` sibling exists
    and force is False."""
    if is_locked(path) and not force:
        return False
    H, W = len(levels), len(levels[0])
    im = Image.new("RGBA", (W, H), (255, 255, 255, 0))
    p = im.load()
    for y in range(H):
        for x in range(W):
            if mask[y][x] or levels[y][x]:
                g = GRAYS[levels[y][x] & 3]
                p[x, y] = (g, g, g, 255)
    d = os.path.dirname(path)
    if d:
        os.makedirs(d, exist_ok=True)
    im.save(path)
    with open(lv_json_path(path), "w") as f:
        json.dump({"w": W, "h": H, "ax": int(ax), "ay": int(ay)}, f)
        f.write("\n")
    return True


# ---------------------------------------------------------------- LCD-look preview
def lcd_image(levels, mask=None, scale=8, grid=True, ground=None):
    """LCD look: orange background, ink = level 3, 1 px gap between dots when grid.
    mask given: transparent pixels are tinted (a pale marker) unless `ground` is a level, then
    shown as that level. mask None: every pixel drawn from its level."""
    H, W = len(levels), len(levels[0])
    tint = (255, 196, 140)
    im = Image.new("RGB", (W * scale, H * scale), LCD_BG if grid else (0, 0, 0))
    p = im.load()
    gap = 1 if grid and scale > 2 else 0
    for y in range(H):
        for x in range(W):
            lvl = levels[y][x]
            if mask is not None and not mask[y][x]:
                if ground is None:
                    col = tint
                else:
                    lvl = ground
                    a = lvl / 3
                    col = tuple(
                        int(LCD_BG[c] * (1 - a) + LCD_INK[c] * a) for c in range(3)
                    )
            else:
                a = lvl / 3
                col = tuple(int(LCD_BG[c] * (1 - a) + LCD_INK[c] * a) for c in range(3))
            for dy in range(scale - gap):
                for dx in range(scale - gap):
                    p[x * scale + dx, y * scale + dy] = col
    return im
