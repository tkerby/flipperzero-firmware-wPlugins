#!/usr/bin/env python3
"""Code-drawn isometric tiles and boxes for the Flipper iso game (M1, contract §1/§2/§6).

Outputs assets/art/tiles/<name>.lv.png (RGBA, grays 255/170/85/0 = levels 0..3,
alpha 255 = opaque/mask) + <name>.json {w,h,ax,ay,fw,fh,kind}, and the preview
build/preview/tiles.png (+ tiles_scene.png).

Geometry
  * 1 tile = 28x14 diamond, drawn in a 28x15 canvas (vertices top (14,0), bottom (14,14),
    left (0,7), right (27,7)); TILE anchor = centre (14,7).
  * OBJECT anchor = bottom vertex of the footprint's front tile.  Sprites are cropped to
    their bounding box and (ax,ay) is the pixel that lands on that vertex.
  * Edges are exact 2:1 staircases (rows of 2 px), mirror-symmetric around the anchor column.
  * Object world units: 1 tile = 7 units, 1 unit along +i = (+2px, +1px) on screen; z in px.
  * "+j face" = lower-left face of a box, "+i face" = lower-right face.
A <name>.locked file next to the output means hand-retouched: never regenerate.
"""
import json
import math
import os
import random
import sys

from PIL import Image, ImageDraw, ImageFont

# ---------------------------------------------------------------- configuration
WALL_H = 36  # wall block height in px
FENCE_H = 12
SIGN_H = 20
SKIRT_H = 7
GRAY = [255, 170, 85, 0]  # level 0..3

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT_DIR = os.path.join(ROOT, "assets", "art", "tiles")
PREV_DIR = os.path.join(ROOT, "build", "preview")
HERO_PNG = os.path.join(ROOT, "models_dl", "pixellab", "hero_anim", "south", "f0.png")


# ---------------------------------------------------------------- canvas
class Spr:
    """Level/mask canvas addressed in anchor-relative coordinates (rx, ry)."""

    def __init__(self, R=100, up=150, down=24):
        self.W = 2 * R
        self.H = up + down + 1
        self.ox = R
        self.oy = up
        self.lv = [[0] * self.W for _ in range(self.H)]
        self.mk = [[False] * self.W for _ in range(self.H)]

    def px(self, rx, ry, lv):
        x, y = rx + self.ox, ry + self.oy
        if 0 <= x < self.W and 0 <= y < self.H:
            self.lv[y][x] = lv
            self.mk[y][x] = True

    def get(self, rx, ry):
        x, y = rx + self.ox, ry + self.oy
        if 0 <= x < self.W and 0 <= y < self.H and self.mk[y][x]:
            return self.lv[y][x]
        return None

    def t(self, x, y, lv):  # tile coordinates (tile canvas: origin = centre (14,7))
        self.px(x - 14, y - 7, lv)

    def hline(self, x0, x1, y, lv):
        for x in range(x0, x1 + 1):
            self.px(x, y, lv)

    def rect(self, x0, y0, x1, y1, lv):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.px(x, y, lv)

    def poly(self, pts, lv):
        xs = [p[0] for p in pts]
        ys = [p[1] for p in pts]
        x0, y0 = min(xs), min(ys)
        w, h = max(xs) - x0 + 1, max(ys) - y0 + 1
        im = Image.new("L", (w, h), 0)
        ImageDraw.Draw(im).polygon([(x - x0, y - y0) for x, y in pts], fill=1)
        p = im.load()
        for y in range(h):
            for x in range(w):
                if p[x, y]:
                    self.px(x + x0, y + y0, lv)

    def line(self, x0, y0, x1, y1, lv):
        """Bresenham line in anchor-relative pixel coordinates."""
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        err = dx + dy
        while True:
            self.px(x0, y0, lv)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def disc(self, cx, cy, r, lv, ring=None):
        """Filled disc (r = radius in px); optional 1-px ring colour drawn first."""
        for y in range(cy - r - 1, cy + r + 2):
            for x in range(cx - r - 1, cx + r + 2):
                d = (x - cx) ** 2 + (y - cy) ** 2
                if ring is not None and d <= r * r + r * 0.6:
                    self.px(x, y, ring)
        for y in range(cy - r - 1, cy + r + 2):
            for x in range(cx - r - 1, cx + r + 2):
                d = (x - cx) ** 2 + (y - cy) ** 2
                if (
                    d <= (r - 1) ** 2 + (r - 1) * 0.6
                    if ring is not None
                    else d <= r * r + r * 0.6
                ):
                    self.px(x, y, lv)

    def outline(self, lv=3):
        """1-px silhouette outline drawn INSIDE the shape (keeps the geometry exact)."""
        edge = []
        for y in range(self.H):
            for x in range(self.W):
                if not self.mk[y][x]:
                    continue
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
                    nx, ny = x + dx, y + dy
                    if (
                        not (0 <= nx < self.W and 0 <= ny < self.H)
                        or not self.mk[ny][nx]
                    ):
                        edge.append((x, y))
                        break
        for x, y in edge:
            self.lv[y][x] = lv

    def flat_white(self, lv=1):
        """Opaque white (level 0) -> `lv`: the sprite then needs no mask (level 0 is only opaque with one), 1/3 fewer bytes in a bank.
        Used by the small city pieces whose map has no room for the mask (Ciudad Nodo's memory budget, contract 5.x).
        """
        for y in range(self.H):
            for x in range(self.W):
                if self.mk[y][x] and self.lv[y][x] == 0:
                    self.lv[y][x] = lv

    def bbox(self):
        xs = [x for y in range(self.H) for x in range(self.W) if self.mk[y][x]]
        ys = [y for y in range(self.H) for x in range(self.W) if self.mk[y][x]]
        return min(xs), min(ys), max(xs), max(ys)


# ---------------------------------------------------------------- iso box primitives
def yDR(x, X, Y):
    """Row of the down-right 2:1 line through vertex (X,Y) at pixel column x."""
    return Y + (x - X + 1) // 2


def yUR(x, X, Y):
    """Row of the up-right 2:1 line through vertex (X,Y) at pixel column x."""
    return Y - (x - X + 1) // 2


class Face:
    """A side face; (sx, z) = pixels along the face from its left end, px above ground."""

    def __init__(self, S, kind, X0, Y0, wid):
        self.S, self.kind, self.X0, self.Y0, self.wid = S, kind, X0, Y0, wid

    def at(self, sx, z):
        y = self.Y0 + (sx + 1) // 2 if self.kind == "j" else self.Y0 - (sx + 1) // 2
        return self.X0 + sx, y - z

    def px(self, sx, z, lv):
        if 0 <= sx < self.wid:
            self.S.px(*self.at(sx, z), lv)

    def rect(self, s0, s1, z0, z1, lv):  # [s0,s1) x [z0,z1)
        for sx in range(s0, s1):
            for z in range(z0, z1):
                self.px(sx, z, lv)

    def hline(self, s0, s1, z, lv):
        self.rect(s0, s1, z, z + 1, lv)

    def vline(self, sx, z0, z1, lv):
        self.rect(sx, sx + 1, z0, z1, lv)

    def window(self, s0, s1, z0, z1, glass=1, frame=3, cross=True, glint=True):
        self.rect(s0, s1, z0, z1, frame)
        self.rect(s0 + 1, s1 - 1, z0 + 1, z1 - 1, glass)
        if cross:
            mid = (s0 + s1) // 2
            self.vline(mid, z0 + 1, z1 - 1, frame)
            if z1 - z0 > 9:
                self.hline(s0 + 1, s1 - 1, (z0 + z1) // 2, frame)
        if glint:
            self.px(s0 + 1, z1 - 2, 0)
            self.px(s0 + 1, z1 - 3, 0)


class Box:
    def __init__(
        self,
        S,
        fw,
        fh,
        a0,
        b0,
        a1,
        b1,
        z0,
        z1,
        top,
        left,
        right,
        edge=3,
        vedge=None,
        bot=None,
    ):
        self.S, self.A, self.B = S, 7 * fw, 7 * fh
        A, B = self.A, self.B

        def sx(a, b):
            return 2 * ((a - A) - (b - B))

        def sy(a, b):
            return (a - A) + (b - B)

        self.XL, self.YL = sx(a0, b1), sy(a0, b1)
        self.XB, self.YB = sx(a1, b1), sy(a1, b1)
        self.XT, self.YT = sx(a0, b0), sy(a0, b0)
        self.XR, self.YR = sx(a1, b0), sy(a1, b0)
        self.z0, self.z1 = z0, z1
        XL, YL, XB, YB, XT, YT, XR = (
            self.XL,
            self.YL,
            self.XB,
            self.YB,
            self.XT,
            self.YT,
            self.XR,
        )
        # top face
        for x in range(XL, XR):
            yt = yUR(x, XL, YL - z1) if x < XT else yDR(x, XT, YT - z1)
            yb = yDR(x, XL, YL - z1) if x < XB else yUR(x, XB, YB - z1)
            for y in range(yt, yb + 1):
                S.px(x, y, top)
        # +j face (lower-left)
        for x in range(XL, XB):
            for y in range(yDR(x, XL, YL - z1) + 1, yDR(x, XL, YL - z0) + 1):
                S.px(x, y, left)
        # +i face (lower-right)
        for x in range(XB, XR):
            for y in range(yUR(x, XB, YB - z1) + 1, yUR(x, XB, YB - z0) + 1):
                S.px(x, y, right)
        self.J = Face(S, "j", XL, YL, XB - XL)
        self.I = Face(S, "i", XB, YB, XR - XB)
        if edge is not None:
            for x in range(XL, XB):
                S.px(x, yDR(x, XL, YL - z1), edge)
            for x in range(XB, XR):
                S.px(x, yUR(x, XB, YB - z1), edge)
            ve = edge if vedge is None else vedge
            for y in range(YB - z1, YB - z0 + 1):
                S.px(XB, y, ve)
        if bot is not None:
            for x in range(XL, XB):
                S.px(x, yDR(x, XL, YL - z0), bot)
            for x in range(XB, XR):
                S.px(x, yUR(x, XB, YB - z0), bot)

    def top_px(self, a2, b2, lv):
        """Pixel on the top face at doubled unit coords (a2, b2)."""
        x = a2 - b2 - 2 * self.A + 2 * self.B
        y = (a2 + b2) // 2 - self.A - self.B - self.z1
        self.S.px(x, y, lv)

    def top_line_a(self, b, a0, a1, lv, dash=None):
        for k, a2 in enumerate(range(2 * a0, 2 * a1 + 1)):
            if dash and (k % dash[0]) >= dash[1]:
                continue
            self.top_px(a2, 2 * b, lv)

    def top_line_b(self, a, b0, b1, lv, dash=None):
        for k, b2 in enumerate(range(2 * b0, 2 * b1 + 1)):
            if dash and (k % dash[0]) >= dash[1]:
                continue
            self.top_px(2 * a, b2, lv)


# ---------------------------------------------------------------- output
RESULTS = []  # (name, kind, w, h, ax, ay, fw, fh)


def finish(name, S, kind, fw, fh, tile=False, outline=True):
    """Outline, crop and write.  tile=True keeps the full 28x15 canvas, anchor (14,7)."""
    if os.path.exists(os.path.join(OUT_DIR, name + ".locked")):
        print("locked, skipped:", name)
        return
    if outline:
        S.outline(3)
    if tile:
        x0, y0, x1, y1 = 0, 0, S.W - 1, S.H - 1
        ax, ay = S.ox, S.oy
    else:
        x0, y0, x1, y1 = S.bbox()
        ax, ay = S.ox - x0, S.oy - y0
    w, h = x1 - x0 + 1, y1 - y0 + 1
    im = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    p = im.load()
    for y in range(h):
        for x in range(w):
            if S.mk[y0 + y][x0 + x]:
                g = GRAY[S.lv[y0 + y][x0 + x]]
                p[x, y] = (g, g, g, 255)
    os.makedirs(OUT_DIR, exist_ok=True)
    im.save(os.path.join(OUT_DIR, name + ".lv.png"))
    meta = {"w": w, "h": h, "ax": ax, "ay": ay, "fw": fw, "fh": fh, "kind": kind}
    with open(os.path.join(OUT_DIR, name + ".json"), "w") as f:
        json.dump(meta, f)
    RESULTS.append((name, kind, w, h, ax, ay, fw, fh))


# ---------------------------------------------------------------- ground tiles
def in_diamond(x, y, hh=7):
    u = x - 14 if x >= 14 else 13 - x
    if u > 2 * hh - 1:
        return False
    top = 7 - hh + (u + 1) // 2
    return top <= y <= 14 - top


def tile_canvas():
    return Spr(14, 7, 7)  # 28 x 15, origin (14,7)


def vtuft(S, bx, by, lv=2):
    for k in range(3):
        S.t(bx - k, by - k, lv)
        S.t(bx + k, by - k, lv)


def grass(name, dots, tuft=None, flowers=()):
    S = tile_canvas()
    for x, y in dots:
        assert in_diamond(x, y, 6), (name, x, y)
        S.t(x, y, 3)
    if tuft:
        vtuft(S, *tuft)
    for fx, fy in flowers:
        for dx in (-1, 0, 1):
            for dy in (-1, 0, 1):
                if dx == 0 and dy == 0:
                    S.t(fx, fy, 3)
                elif dx == 0 or dy == 0:
                    S.t(fx + dx, fy + dy, 0)  # opaque white petal
                else:
                    S.t(fx + dx, fy + dy, 2)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def path_tile(name, seed, phase):
    S = tile_canvas()
    rnd = random.Random(seed)
    for r, y in enumerate(range(1, 14, 2)):
        x = 1 + phase + (2 if r % 2 else 0)
        while x < 27:
            k = rnd.random()
            if k < 0.55:
                cells = [(x, y, 2), (x + 1, y, 2)]
            elif k < 0.85:
                cells = [(x, y, 3)]
            else:
                cells = []
            for cx, cy, lv in cells:
                if in_diamond(cx, cy, 7):
                    S.t(cx, cy, lv)
            x += 4
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def floor_wood(name="floor_wood"):
    S = tile_canvas()
    # seams along the i axis (down-right lines y = c + (x+1)//2): c spacing 4,3,4,3 (planks)
    for c, lv, ph in ((0, 2, 0), (7, 2, 3), (3, 1, 5), (-4, 1, 1)):
        for x in range(28):
            y = c + (x + 1) // 2
            if in_diamond(x, y, 7) and ((x + ph) % 7) < 5:
                S.t(x, y, lv)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def floor_lab(name="floor_lab"):
    S = tile_canvas()
    for x in range(0, 14):
        S.t(x, 7 + (x + 1) // 2, 1)  # L->B edge
    for x in range(14, 28):
        S.t(x, 14 - (x - 14 + 1) // 2, 1)  # B->R edge
    for p in ((13, 14), (14, 14), (0, 7), (27, 7)):  # joint studs where grid lines meet
        S.t(p[0], p[1], 2)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def mat(name="mat"):
    S = tile_canvas()
    hh = 5
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y, hh) and in_diamond(x, y + (7 - 7), 7):
                u = x - 14 if x >= 14 else 13 - x
                top = 7 - hh + (u + 1) // 2
                if u <= 2 * hh - 1 and top <= y <= 14 - top:
                    S.t(x, y, 2)
    finish(name, S, "tile", 1, 1, tile=True, outline=True)


def void_dark(name="void_dark"):
    """M4-D / M4 polish / forest-light: the unseen depth of the Bosque Umbrio, drawn as treetops seen from above: a mid-dark canopy (level 2)
    with level-3 shadow clusters between the crowns and level-1 lit leaf tops, so it is shady but never a black field. No white, so no
    mask. Solid; the maze's walls are two trees deep and nothing behind them is drawn as objects (the draw list and the file stay small).
    """
    S = tile_canvas()
    rnd = random.Random(21)
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y):
                S.t(x, y, 2)
    for _ in range(
        14
    ):  # shadow gaps between the crowns: a short run with a pixel below (level 3)
        x, y = rnd.randrange(2, 24), rnd.randrange(2, 13)
        for dx, dy in ((0, 0), (1, 0), (2, 0), (1, 1)):
            if in_diamond(x + dx, y + dy, 6):
                S.t(x + dx, y + dy, 3)
    for _ in range(
        18
    ):  # lit leaf tops (level 1): a crown highlight with a pixel below-left
        x, y = rnd.randrange(2, 24), rnd.randrange(2, 13)
        for dx, dy in ((0, 0), (1, 0), (-1, 1) if rnd.random() < 0.5 else (0, 1)):
            if in_diamond(x + dx, y + dy, 6) and S.get(x + dx - 14, y + dy - 7) == 2:
                S.t(x + dx, y + dy, 1)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


# ---------------------------------------------------------------- tall grass / ledges (M3-B)
def tall_grass(name="tall_grass"):
    """Dense tufts of blades on a staggered lattice: blade bodies level 1 (light), a level-2 base pixel and level-3 tips,
    so it reads clearly against the village's short grass (three level-3 dots on white) without a dark mass. C_GRASS.
    """
    S = tile_canvas()
    tufts = []
    for r, y in enumerate((4, 8, 12)):
        for x in range(3 + (4 if r % 2 else 0), 26, 8):
            tufts.append((x, y))
    for x, y in tufts:
        pts = []
        for dx, dy, lv in (
            (-1, 0, 2),
            (0, 0, 2),
            (1, 0, 2),
            (-1, -1, 2),
            (-2, -2, 1),
            (-3, -3, 3),
            (1, -1, 2),
            (2, -2, 1),
            (3, -3, 3),
            (0, -1, 2),
            (0, -2, 2),
            (0, -3, 1),
            (0, -4, 3),
        ):
            if in_diamond(x + dx, y + dy, 7) and in_diamond(x + dx, y + dy + 1, 7):
                pts.append((x + dx, y + dy, lv))
        for px_, py_, lv in sorted(pts, key=lambda t: -t[1]):  # back to front
            S.t(px_, py_, lv)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


LEDGE_FACE = 5  # px of cliff face below the diamond edge (SW / SE ledges)
LEDGE_EDGE = {  # direction -> (x range, y of the diamond edge at x)
    "sw": (range(0, 14), lambda x: 7 + (x + 1) // 2),
    "se": (range(14, 28), lambda x: 14 - (x - 14 + 1) // 2),
    "nw": (range(0, 14), lambda x: 7 - (x + 1) // 2),
    "ne": (range(14, 28), lambda x: (x - 14 + 1) // 2),
}


def ledge(name, d):
    """One-way ledge tile (C_LEDGE); the jump direction lives in the map (ledge=SW|SE|NE|NW). The lip is drawn along
    the edge the hero drops over: a dark line, a gray band inside and (south-facing edges) a cliff face under it.
    """
    S = Spr(14, 7, 7 + LEDGE_FACE)
    xs, edge = LEDGE_EDGE[d]
    rnd = random.Random(31 + ord(d[0]) + ord(d[1]))
    for x in xs:
        ye = edge(x)
        down = d in ("sw", "se")
        S.t(x, ye, 3)
        if down:
            for k in range(1, LEDGE_FACE):
                S.t(x, ye + k, 2)
            S.t(x, ye + LEDGE_FACE, 3)
            if rnd.random() < 0.3:
                S.t(x, ye + 2 + rnd.randrange(2), 3)
            S.t(x, ye - 1, 1)
            S.t(x, ye - 2, 1) if x % 2 else None
        else:  # far edge: the face is hidden behind the lip, only a thick lip shows
            S.t(x, ye + 1, 3)
            S.t(x, ye + 2, 2)
            S.t(x, ye + 3, 1) if x % 2 else None
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


# ---------------------------------------------------------------- walls
def wall_deco(b, window_on=None):
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 6, 1)  # baseboard
    J.hline(0, J.wid, WALL_H - 6, 1)  # cornice seam
    J.vline(0, 6, WALL_H - 6, 1)  # panel joint
    I.rect(0, I.wid, 0, 6, 2)
    I.hline(0, I.wid, WALL_H - 6, 2)
    I.vline(I.wid - 1, 6, WALL_H - 6, 2)
    if window_on == "j":
        J.window(3, 11, 14, 29, glass=1)
    elif window_on == "i":
        I.window(3, 11, 14, 29, glass=0)


def wall(name, window_on=None):
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 0, 0, 1, edge=3)
    wall_deco(b, window_on)
    finish(name, S, "object", 1, 1)


# ---------------------------------------------------------------- fences, sign
def fence(name, axis):
    """axis 'i': runs along +i, picket face toward +j (front row).  axis 'j': runs along +j,
    face toward +i (right column).  Set 1 unit in from the tile's outer edge (b/a in 5..6).
    """
    S = Spr()
    P = 5
    if axis == "i":
        Box(S, 1, 1, 3, P, 4, P + 1, 0, FENCE_H, 1, 1, 2, edge=3)  # post
        for z0, z1 in ((2, 5), (7, 10)):
            Box(S, 1, 1, 0, P, 7, P + 1, z0, z1, 1, 1, 2, edge=3, vedge=None)
    else:
        Box(S, 1, 1, P, 3, P + 1, 4, 0, FENCE_H, 1, 1, 2, edge=3)
        for z0, z1 in ((2, 5), (7, 10)):
            Box(S, 1, 1, P, 0, P + 1, 7, z0, z1, 1, 1, 2, edge=3)
    finish(name, S, "object", 1, 1)


def sign(name="sign"):
    S = Spr()
    Box(S, 1, 1, 3, 3, 4, 4, 0, 12, 1, 1, 2, edge=None)  # post
    b = Box(S, 1, 1, 1, 3, 6, 4, 10, SIGN_H, 0, 0, 1, edge=3)  # board
    b.J.hline(2, 8, 15, 2)
    b.J.hline(2, 6, 12, 2)
    finish(name, S, "object", 1, 1)


def skirt(name, axis):
    """Cliff face under a ground tile's front edge: ground plane extruded SKIRT_H px down.
    axis 'i' = under the lower-right edge (cols 0..13 from the anchor), 'j' = lower-left.
    No end verticals (segments repeat along the edge)."""
    S = Spr()
    rnd = random.Random(7 if axis == "i" else 11)
    cols = range(0, 14) if axis == "i" else range(-14, 0)
    for x in cols:
        u = x if axis == "i" else -1 - x
        ye = -((u + 1) // 2)  # ground edge row at this column (anchor-relative)
        for k in range(0, SKIRT_H + 1):
            if k == 0 or k == SKIRT_H:
                S.px(x, ye + k, 3)
            else:
                S.px(x, ye + k, 2)
        if rnd.random() < 0.35:  # rock strata ticks
            S.px(x, ye + 3 + rnd.randrange(3), 3)
    finish(name, S, "object", 1, 1, outline=False)


# ---------------------------------------------------------------- roofs / buildings
def roof(S, fw, fh, z0, thick=3, inset=0):
    A, B = 7 * fw, 7 * fh
    b = Box(S, fw, fh, 0, 0, A, B, z0, z0 + thick, 1, 1, 2, edge=3)
    b.J.rect(0, b.J.wid, z0, z0 + thick, 2)  # darker fascia on the lit side
    b.J.hline(0, b.J.wid, z0 + thick - 1, 3)
    return b


def fb_house(name="fb_house"):
    fw, fh = 2, 2
    S = Spr()
    WH = 28
    b = Box(S, fw, fh, 0, 0, 14, 14, 0, WH, 0, 0, 1, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 3, 1)  # plinth
    I.rect(0, I.wid, 0, 3, 2)
    J.hline(0, J.wid, WH - 4, 1)  # trim
    I.hline(0, I.wid, WH - 4, 2)
    J.window(3, 11, 11, 22, glass=1)
    # door in the right (front) tile: tile i=1 spans s 14..27
    J.rect(17, 25, 0, 18, 3)
    J.rect(18, 24, 0, 17, 2)
    J.vline(21, 0, 17, 3)
    J.px(19, 8, 0)
    J.px(23, 8, 0)
    J.hline(16, 26, 18, 1)  # canopy line
    I.window(4, 12, 11, 22, glass=0)
    I.window(16, 24, 11, 22, glass=0)
    # flat roof with parapet
    r = roof(S, fw, fh, WH, 3)
    rz = WH + 3
    # roof deck + solar panel + vent
    deck = Box(S, fw, fh, 1, 1, 13, 13, rz, rz + 1, 0, 1, 2, edge=2)
    sp = Box(S, fw, fh, 3, 4, 10, 9, rz + 1, rz + 3, 3, 3, 3, edge=None)
    for a in (5, 7, 9):
        sp.top_line_b(a, 4, 9, 2)
    sp.top_line_a(6.5 if False else 6, 3, 10, 2)
    Box(S, fw, fh, 10, 2, 12, 4, rz + 1, rz + 4, 0, 1, 2, edge=3)  # vent
    finish(name, S, "object", fw, fh)


def fb_house_rival(name="fb_house_rival"):
    """Neighbour's house: gable roof (ridge along +i, triangular gable on the right face),
    clapboard walls, shuttered window, porch awning with posts, brick chimney.  2x2, door
    on the +j face of the front tile (i=1)."""
    fw, fh = 2, 2
    S = Spr()
    WH, RH = 26, 9  # wall height, ridge rise
    A = B = 14

    def P(a, b, z):  # unit coords -> anchor-relative pixel
        return 2 * (a - b), a + b - A - B - z

    b = Box(S, fw, fh, 0, 0, 14, 14, 0, WH, 0, 0, 1, edge=3)
    J, I = b.J, b.I
    # clapboard siding
    for z in range(4, WH - 1, 4):
        J.hline(0, J.wid, z, 1)
        I.hline(0, I.wid, z, 2)
    J.rect(0, J.wid, 0, 3, 1)  # stone plinth
    I.rect(0, I.wid, 0, 3, 2)
    J.hline(0, J.wid, 3, 2)
    # left window with shutters
    J.window(4, 12, 8, 19, glass=1, frame=3)
    J.rect(2, 4, 8, 19, 2)
    J.rect(12, 14, 8, 19, 2)
    J.hline(3, 13, 7, 3)  # sill
    # door (front tile i=1 -> sx 14..27), recessed under the porch
    J.rect(17, 25, 0, 15, 3)
    J.rect(18, 24, 0, 14, 2)
    J.vline(21, 0, 14, 3)
    J.px(19, 7, 0)
    J.px(23, 7, 0)
    J.rect(18, 24, 10, 13, 1)  # door glass
    # right face: one wide picture window
    I.window(6, 22, 8, 19, glass=0, frame=3)
    I.hline(5, 23, 7, 3)
    O = 1
    r = lambda a, b_, z: P(a, b_, z)
    # back (-i) slope first, then the lit gable triangle on the +j face, then the front slope
    S.poly([r(-O, -O, WH), r(-O, 14, WH), r(7, 14, WH + RH), r(7, -O, WH + RH)], 1)
    for z in range(WH, WH + RH):
        hw = 14.0 * (1 - (z - WH) / RH)
        for sx in range(0, J.wid):
            if abs(sx + 0.5 - 14) <= hw:
                J.px(sx, z, 1 if (z - WH) % 4 == 3 else 0)
    S.poly(
        [r(14 + O, -O, WH), r(14 + O, 14, WH), r(7, 14, WH + RH), r(7, -O, WH + RH)], 2
    )
    # rake boards: the 1-unit overhang strip in front of the gable triangle
    S.poly([r(-O, 14, WH), r(-O, 15, WH), r(7, 15, WH + RH), r(7, 14, WH + RH)], 1)
    S.poly(
        [r(14 + O, 14, WH), r(14 + O, 15, WH), r(7, 15, WH + RH), r(7, 14, WH + RH)], 2
    )
    # chimney on the +i slope near the ridge
    zc = WH + (RH * 3) // 8
    ch = Box(S, fw, fh, 9, 2, 12, 5, zc, zc + 9, 1, 1, 2, edge=3)
    ch.J.hline(0, ch.J.wid, 3, 3)
    ch.J.hline(0, ch.J.wid, 6, 3)
    ch.J.rect(1, ch.J.wid - 1, 8, 9, 3)
    # shingle courses on the +i slope (lines of constant a)
    for aa in (12, 10, 8):
        z = WH + RH * (14 + O - aa) / (7 + O)
        for k, b2 in enumerate(range(2 * (-O) + 1, 2 * (14 + O))):
            if k % 6 < 4 and not (7 <= aa <= 13 and 1 <= b2 / 2 <= 6):
                x = 2 * aa - b2
                y = (2 * aa + b2) // 2 - A - B - int(round(z))
                S.px(x, y, 1)
    # roof edges
    S.line(*r(14 + O, -O, WH), *r(14 + O, 15, WH), 3)  # front eave
    S.line(*r(14 + O, 15, WH), *r(7, 15, WH + RH), 3)  # front rake
    S.line(*r(-O, 15, WH), *r(7, 15, WH + RH), 3)  # back-half rake
    S.line(*r(-O, 14, WH), *r(-O, 15, WH), 3)
    S.line(*r(7, -O, WH + RH), *r(7, 15, WH + RH), 3)  # ridge
    ex0, ey0 = r(14 + O, -O, WH)
    ex1, ey1 = r(14 + O, 15, WH)
    for x in range(ex0, ex1 + 1):  # fascia board under the eave
        S.px(x, yUR(x, ex1, ey1) + 1, 2)
    # porch: awning over the door + two posts
    aw = Box(S, fw, fh, 7, 14, 14, 17, 16, 20, 1, 2, 2, edge=3)
    for sx in range(2, aw.J.wid - 1, 4):  # awning stripes
        aw.J.rect(sx, sx + 2, 17, 19, 1)
    finish(name, S, "object", fw, fh)


def fb_lab(name="fb_lab"):
    fw, fh = 3, 2
    S = Spr()
    WH = 32
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 0, 0, 1, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 3, 1)
    I.rect(0, I.wid, 0, 3, 2)
    # wide glass windows left and right of the door
    for s0 in (1, 29):
        J.window(s0, s0 + 12, 9, 26, glass=1, cross=False)
        J.vline(s0 + 4, 10, 25, 3)
        J.vline(s0 + 8, 10, 25, 3)
        J.hline(s0 + 1, s0 + 11, 17, 3)
    # glass door in the middle tile (tile i=1 spans s 14..27)
    J.rect(16, 26, 0, 19, 3)
    J.rect(17, 25, 0, 18, 1)
    J.vline(21, 0, 18, 3)
    J.px(19, 9, 0)
    J.px(23, 9, 0)
    # sign plate over the door
    J.rect(14, 29, 20, 28, 3)
    J.rect(15, 28, 21, 27, 0)
    J.hline(17, 26, 25, 3)  # lettering
    J.hline(17, 22, 23, 3)
    J.px(24, 23, 3)
    J.px(26, 23, 3)
    for s0 in (3, 16):
        I.window(s0, s0 + 9, 10, 27, glass=0)
    J.hline(0, J.wid, WH - 3, 1)
    I.hline(0, I.wid, WH - 3, 2)
    r = roof(S, fw, fh, WH, 3)
    rz = WH + 3
    Box(S, fw, fh, 1, 1, 20, 13, rz, rz + 1, 0, 1, 2, edge=2)
    Box(S, fw, fh, 3, 3, 9, 8, rz + 1, rz + 6, 1, 0, 1, edge=3)  # rooftop unit
    Box(S, fw, fh, 14, 8, 17, 11, rz + 1, rz + 3, 0, 0, 1, edge=3)  # dish base
    Box(S, fw, fh, 15, 9, 16, 10, rz + 3, rz + 9, 3, 3, 3, edge=None)  # mast
    dx, dy = 2 * (15 - 21 - (9 - 14)) + 1, (15 - 21) + (9 - 14) - (rz + 9)
    cx, cy = dx - 1, dy - 2
    for y in range(cy - 8, cy + 9):  # dish bowl, opens up-right
        for x in range(cx - 8, cx + 9):
            ddx, ddy = x - cx, y - cy
            d = ddx * ddx + ddy * ddy
            if d <= 40 and ddx - ddy <= 1:
                S.px(x, y, 3)
                if d <= 24 and ddx - ddy <= -2:
                    S.px(x, y, 0 if ddx - ddy > -5 else 1)
    S.line(cx, cy, cx + 6, cy - 6, 3)  # feed arm
    S.px(cx + 7, cy - 7, 3)
    S.px(cx + 6, cy - 7, 3)
    finish(name, S, "object", fw, fh)


def fb_bed(name="fb_bed"):
    fw, fh = 1, 2
    S = Spr()
    Box(S, fw, fh, 0, 0, 7, 14, 0, 5, 1, 1, 2, edge=3)  # frame
    hb = Box(S, fw, fh, 0, 0, 7, 1, 0, 17, 1, 1, 2, edge=3)  # headboard
    m = Box(S, fw, fh, 1, 1, 6, 14, 5, 9, 0, 0, 1, edge=3)  # mattress
    Box(S, fw, fh, 1, 6, 6, 14, 9, 10, 1, 1, 2, edge=3)  # blanket
    Box(S, fw, fh, 2, 2, 5, 5, 9, 11, 0, 0, 1, edge=3)  # pillow
    finish(name, S, "object", fw, fh)


def fb_table(name="fb_table"):
    fw = fh = 1
    S = Spr()
    for a, b_ in ((0, 0), (6, 0), (0, 6)):  # back + side legs
        Box(S, fw, fh, a, b_, a + 1, b_ + 1, 0, 9, 3, 3, 3, edge=None)
    Box(
        S, fw, fh, 1, 1, 6, 6, 3, 9, 1, 1, 2, edge=3
    )  # solid apron under the top (reads as solid)
    Box(
        S, fw, fh, 6, 6, 7, 7, 0, 9, 3, 3, 3, edge=None
    )  # front leg, in front of the apron
    Box(S, fw, fh, 0, 0, 7, 7, 9, 12, 0, 0, 1, edge=3)
    finish(name, S, "object", fw, fh)


def fb_lab_table(name="fb_lab_table"):
    """3x1 lab bench: white top, drawer apron, legs with a lower shelf, devices on top."""
    fw, fh = 3, 1
    S = Spr()
    TZ0, TZ1 = 10, 14
    for a, b_ in sorted(((0, 0), (0, 6), (20, 0), (20, 6)), key=lambda t: t[0] + t[1]):
        Box(S, fw, fh, a, b_, a + 1, b_ + 1, 0, TZ0, 3, 3, 3, edge=None)
    t = Box(S, fw, fh, 0, 0, 21, 7, TZ0, TZ1, 0, 1, 2, edge=3)  # worktop
    J = t.J
    for s0 in (3, 17, 31):  # drawer fronts
        J.rect(s0, s0 + 10, TZ0 + 1, TZ1 - 1, 0)
        J.hline(s0 + 3, s0 + 7, TZ0 + 2, 3)
    # devices on the top (z >= TZ1)
    z = TZ1
    d = Box(S, fw, fh, 1, 1, 6, 6, z, z + 8, 1, 0, 1, edge=3)  # analyser
    d.J.rect(1, d.J.wid - 1, z + 3, z + 7, 3)  # screen
    d.J.hline(2, 6, z + 5, 1)
    d.J.px(2, z + 6, 0)
    d.J.px(3, z + 1, 3)
    d.J.px(6, z + 1, 2)
    bk = Box(S, fw, fh, 8, 3, 10, 5, z, z + 7, 2, 0, 1, edge=3)  # tall beaker
    bk.J.rect(1, bk.J.wid - 1, z + 1, z + 4, 2)  # liquid
    fl = Box(S, fw, fh, 11, 2, 14, 5, z, z + 4, 2, 0, 1, edge=3)  # flask
    fl.J.rect(1, fl.J.wid - 1, z + 1, z + 3, 2)
    Box(S, fw, fh, 12, 3, 13, 4, z + 4, z + 8, 0, 0, 1, edge=3)  # flask neck
    # microscope
    Box(S, fw, fh, 15, 2, 20, 6, z, z + 2, 1, 0, 1, edge=3)  # base
    Box(S, fw, fh, 17, 2, 18, 3, z + 2, z + 11, 1, 1, 2, edge=3)  # arm
    t2 = Box(S, fw, fh, 16, 2, 19, 4, z + 8, z + 11, 3, 3, 3, edge=3)  # head
    Box(S, fw, fh, 17, 4, 18, 5, z + 2, z + 6, 3, 3, 3, edge=None)  # objective
    finish(name, S, "object", fw, fh)


CRYSTAL_TILE_X = (
    -28,
    -14,
    0,
)  # centre pixel of the pedestal above the 3 tiles of a 3x1 table, rel. to the anchor
CRYSTAL_TILE_Y = (
    -35,
    -28,
    -21,
)  # the tip stands on the socket row (the worktop is CRYSTAL_LIFT px above the NPC anchor)
CRYSTAL_LIFT = 14  # the worktop is 14 px above the ground (TZ1)


def fb_lab_table_crystals(name="fb_lab_table_crystals"):
    """3x1 lab bench for Profesora Lin's Crystals: clear white worktop, drawer apron, shelf legs and three small
    flat pedestals (an 11 x 5 px iso plate with a 1 px edge and a dark socket) on the top (the Crystals are NPC
    actors, `crystal_*`, standing on the pedestals)."""
    fw, fh = 3, 1
    S = Spr()
    TZ0, TZ1 = 10, 14
    for a, b_ in sorted(((0, 0), (0, 6), (20, 0), (20, 6)), key=lambda t: t[0] + t[1]):
        Box(S, fw, fh, a, b_, a + 1, b_ + 1, 0, TZ0, 3, 3, 3, edge=None)
    t = Box(S, fw, fh, 0, 0, 21, 7, TZ0, TZ1, 0, 1, 2, edge=3)  # worktop
    J = t.J
    for s0 in (3, 17, 31):  # drawer fronts
        J.rect(s0, s0 + 10, TZ0 + 1, TZ1 - 1, 0)
        J.hline(s0 + 3, s0 + 7, TZ0 + 2, 3)
    for cx, cy in zip(
        CRYSTAL_TILE_X, CRYSTAL_TILE_Y
    ):  # pedestals: iso plate + edge + socket
        for dy, hw in (
            (-2, 1),
            (-1, 3),
            (0, 5),
            (1, 3),
            (2, 1),
        ):  # the 1 px thick edge (plate shifted down 1)
            for x in range(cx - hw, cx + hw + 1):
                S.px(x, cy + dy + 1, 3)
        for dy, hw in (
            (-2, 1),
            (-1, 3),
            (0, 5),
            (1, 3),
            (2, 1),
        ):  # the top face, black rim, light inside
            for x in range(cx - hw, cx + hw + 1):
                S.px(x, cy + dy, 3 if (abs(x - cx) == hw or abs(dy) == 2) else 1)
        S.hline(cx - 1, cx + 1, cy, 3)  # socket under the crystal's tip
    finish(name, S, "object", fw, fh)


# ---------------------------------------------------------------- Crystals (NPC actors, 1 direction, 1 frame)
ACTORS_DIR = os.path.join(ROOT, "assets", "art", "npcs")
# Upright hexagonal crystal, 11 x 16 px, 3/4 isometric light: pointed tip top and bottom (half-width per row), two
# long facets split by a vertical ridge: left facet white (lit), ridge level 1, right facet in shade.
CRYSTAL_HW = (0, 1, 2, 3, 4, 5, 5, 5, 5, 5, 5, 4, 3, 2, 1, 0)
CRYSTAL_H = len(CRYSTAL_HW)
CRYSTAL_RIGHT = {
    "fire": 2,
    "water": 1,
    "plant": 1,
}  # shade of the right facet = the element's hint
# element emblems, 3 x 4 px of level 3 on the lit facet ('#' = ink)
CRYSTAL_EMBLEM = {
    "fire": (".#.", ".##", "###", "###"),  # flame
    "water": (".#.", "###", "###", ".#."),  # drop
    "plant": ("#.#", "###", ".#.", ".#."),  # sprout
}


def crystal_actor(name, mark):
    """Crystal of Profesora Lin (replaces the former round containers): lowest row = CRYSTAL_LIFT px above the
    anchor (= the tile centre + ACTOR_FOOT_DY), i.e. on the pedestal of the worktop of the tile below it.
    mark = 'fire' | 'water' | 'plant': emblem on the lit facet + the right facet's tint (plant also has a shaded tip).
    """
    S = Spr(14, 40, 4)
    top = -CRYSTAL_LIFT - (CRYSTAL_H - 1)  # anchor-relative row of the upper tip
    right = CRYSTAL_RIGHT[mark]
    for r, hw in enumerate(CRYSTAL_HW):
        for x in range(-hw, hw + 1):
            S.px(x, top + r, 0 if x < 0 else (1 if x == 0 else right))
    if mark == "plant":  # shaded lower tip of the right facet
        for r in range(CRYSTAL_H - 5, CRYSTAL_H):
            for x in range(1, CRYSTAL_HW[r]):
                S.px(x, top + r, 2)
    S.outline(3)
    for dy, row in enumerate(
        CRYSTAL_EMBLEM[mark]
    ):  # emblem on the lit facet (x -3..-1, rows 6..9)
        for dx, ch in enumerate(row):
            if ch == "#":
                S.px(-3 + dx, top + 6 + dy, 3)
    S.px(3, top + 5, 0)  # glint on the shaded facet
    S.px(3, top + 6, 0)
    x0, y0, x1, y1 = S.bbox()
    w, h = x1 - x0 + 1, y1 - y0 + 1
    im = Image.new("RGBA", (w, h), (255, 255, 255, 0))
    p = im.load()
    for y in range(h):
        for x in range(w):
            if S.mk[y0 + y][x0 + x]:
                g = GRAY[S.lv[y0 + y][x0 + x]]
                p[x, y] = (g, g, g, 255)
    d = os.path.join(ACTORS_DIR, name)
    if os.path.exists(os.path.join(d, "south.locked")):
        print("locked, skipped:", name)
        return
    os.makedirs(d, exist_ok=True)
    im.save(os.path.join(d, "south.lv.png"))
    with open(os.path.join(d, "south.json"), "w") as f:
        json.dump({"w": w, "h": h, "ax": S.ox - x0, "ay": S.oy - y0}, f)
        f.write("\n")


# ---------------------------------------------------------------- Flitter (the roaming wild creature of Ruta 1)
# Code-drawn overworld WREN (chochin), 13 x 14 px incl. feet, matching the battle art (assets/art/elementis/flitter_c): round plump body,
# short tail cocked UP (the silhouette that makes it a wren), short thin bill, white eyebrow stripe over a dark eye, white belly, darker
# wings / back / cap, a tiny curl on the crown, black outline, two feet. Directions S, SE, E, NE, N are drawn; W / NW / SW are the flipped
# E / NE / SE (catalog mirror_west, the bank folds them). From the back (N) the cocked tail is the dark centre strip. Frame 0 = standing,
# frame 1 = hop pose (body 1 px lower, feet tucked, wings out 1 px): the engine bobs the sprite by
# `hop = true` in the catalog (world.c npcs_pose, contract 13.5). Odd canvas width: the anchor column is the symmetry axis.
# Legend: . clear, w white (level 0), l light (1), d dark gray (2), # black (3).
FLITTER_W, FLITTER_H, FLITTER_AX, FLITTER_AY = 13, 14, 6, 13
FLITTER_ART = {
    "south": """
......##.....
......#......
....#####....
...#ddddd#...
..#ddddddd#..
..#wwdddww#..
.#wd#wdw#dw#.
.#dwwwwwwwd#.
.#ddwwlwwdd#.
.#ddwwwwwdd#.
..#ddlllld#..
...#######...
....d...d....
...##...##...
""",
    "south-east": """
........#....
.......#.....
.......###...
.##...#ddddd#
#dd#..#wwwww#
#dd#..#d#d#d#
#dd#.#wwwwd##
.#dd##dwwwww#
..#dddddwwww#
..#dddddwwww#
...#dddlwww#.
....#######..
......d..d...
.....##..##..
""",
    "east": """
.........#...
........#....
.......####..
.##...#dddd#.
#dd#..#wwww#.
#dd#..#dd#w##
#dd#.#ddwww#.
.#dd##dwwwww#
..#dddddwwww#
..#dddddwwww#
...#dddlwww#.
....#######..
......d..d...
.....##..##..
""",
    "north-east": """
........#....
.......#.....
.......###...
.##..#ddddd#.
#dd#.#wwwdd##
#dd#.#ddddd#.
#dd##dddddd#.
.#dd#ddddddd#
..#ddldddddd#
..#dddldlddw#
...#dddlwwww#
....#######..
......d..d...
.....##..##..
""",
    "north": """
......##.....
......#......
....#####....
...#ddddd#...
..#ddddddd#..
..#dd###dd#..
..#dd#d#dd#..
.#ddd#d#ddd#.
.#dd#ddd#dd#.
.#dd#dld#dd#.
..#d#ddd#d#..
...#lwwwl#...
....d...d....
...##...##...
""",
}


def flitter_frames(txt):
    """ASCII art -> (frame 0, frame 1) as (levels, mask) grids (hop pose = lower body, tucked feet, wings out)."""
    rows = txt.strip("\n").split("\n")
    assert len(rows) == FLITTER_H and all(
        len(r) == FLITTER_W for r in rows
    ), "flitter art size"
    lv = [[0] * FLITTER_W for _ in range(FLITTER_H)]
    mk = [[False] * FLITTER_W for _ in range(FLITTER_H)]
    for y, r in enumerate(rows):
        for x, ch in enumerate(r):
            if ch != ".":
                lv[y][x] = "wld#".index(ch)
                mk[y][x] = True
    f0 = (lv, mk)
    lv1 = [[0] * FLITTER_W for _ in range(FLITTER_H)]
    mk1 = [[False] * FLITTER_W for _ in range(FLITTER_H)]
    for y in range(
        FLITTER_H - 1
    ):  # whole bird one row down: the foot row falls off = tucked feet
        lv1[y + 1] = list(lv[y])
        mk1[y + 1] = list(mk[y])
    for y in range(
        8, 11
    ):  # wings out: an outline pixel moves out, the wing grays fill its place
        row, mrow = lv1[y], mk1[y]
        xs = [x for x in range(FLITTER_W) if mrow[x]]
        for x_end, step in ((xs[0], -1), (xs[-1], 1)):
            xo, xi = x_end + step, x_end - step
            if 0 <= xo < FLITTER_W and row[x_end] == 3 and mrow[xi] and row[xi] == 2:
                mrow[xo], row[xo] = True, 3
                row[x_end] = 2
    return f0, (lv1, mk1)


def flitter_actor(name="flitter_ow"):
    import lvlib

    d = os.path.join(ACTORS_DIR, name)
    if os.path.exists(os.path.join(d, "south.locked")):
        print("locked, skipped:", name)
        return
    for dirname, txt in FLITTER_ART.items():
        for k, (lv, mk) in enumerate(flitter_frames(txt)):
            lvlib.write_lv_png(
                os.path.join(d, dirname, "f%d.lv.png" % k),
                lv,
                mk,
                FLITTER_AX,
                FLITTER_AY,
                force=True,
            )


# ---------------------------------------------------------------- M4-C: the "!" balloon and the item pouch
# Both are drawn from ASCII art (legend: . clear, w white = level 0 opaque, l light = 1, d dark = 2, # black = 3).
# emote_excl: the speech balloon with an exclamation mark that appears above a trainer who just saw the hero (13 x 15, an OBJECT in the
# catalog; mapc puts it LAST in the bank of every map that has a trainer and sets the map flag MAPF_BALLOON; render.c blits it with the tail
# tip (the anchor) one pixel above the trainer sprite's top row).
EXCL_ART = """
...#######...
..#wwwwwww#..
.#wwwwwwwww#.
#wwww###wwww#
#wwww###wwww#
#wwww###wwww#
#wwwww#wwwww#
#wwwww#wwwww#
#wwwwwwwwwww#
#wwww###wwww#
.#www###www#.
..#wwwwwww#..
...###w###...
.....#w#.....
......#......
"""
# item_bag: an item lying on the ground (a cloth pouch with a drawstring, NOT a ball), 13 x 12, an ACTOR with 1 direction and 1 frame
# standing on its tile like an NPC (anchor = the bottom row, 2 px below the tile centre like the feet of the other actors).
BAG_ART = """
....##.##....
.....#w#.....
....#ddd#....
...#bbbbb#...
..#bbbbbbb#..
.#bbbbbbbbb#.
.#bbbbbbbbb#.
#bbbbbbbbbbb#
#bbbbbbbbbbb#
#bbbbbbbbbbb#
.#bbbbbbbbb#.
..#########..
"""
# 'b' = the body: shaded by columns (2 white highlight px on the left, 3 dark px on the right, light gray between) so the pouch reads as a round
# sack on the light grass


def ascii_levels(txt):
    rows = [r for r in txt.strip("\n").split("\n")]
    w = max(len(r) for r in rows)
    lv = [[0] * w for _ in rows]
    mk = [[False] * w for _ in rows]
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            if c != ".":
                mk[y][x] = True
                lv[y][x] = {"w": 0, "l": 1, "d": 2, "#": 3}[c]
    return lv, mk


def emote_excl(name="emote_excl"):
    import lvlib

    lv, mk = ascii_levels(EXCL_ART)
    assert len(lv[0]) == 13 and len(lv) == 15 and all(len(r) == 13 for r in lv)
    path = os.path.join(OUT_DIR, name + ".lv.png")
    if lvlib.write_lv_png(path, lv, mk, 6, 14):
        with open(
            os.path.join(OUT_DIR, name + ".json"), "w"
        ) as f:  # object meta (tile json format + footprint)
            json.dump(
                {
                    "w": 13,
                    "h": 15,
                    "ax": 6,
                    "ay": 14,
                    "fw": 1,
                    "fh": 1,
                    "kind": "object",
                },
                f,
            )
        RESULTS.append((name, "object", 13, 15, 6, 14, 1, 1))


def item_bag(name="item_bag"):
    import lvlib

    rows = [r for r in BAG_ART.strip("\n").split("\n")]
    w = max(len(r) for r in rows)
    shaded = []
    for y, r in enumerate(rows):
        x0, x1 = r.find("b"), r.rfind("b")
        out = []
        for x, c in enumerate(r):
            if c == "b":
                c = "w" if x - x0 < 2 else ("d" if x1 - x < 3 and y >= 4 else "l")
            out.append(c)
        shaded.append("".join(out).ljust(w, "."))
    lv, mk = ascii_levels("\n".join(shaded))
    lvlib.write_lv_png(
        os.path.join(ACTORS_DIR, name, "south", "f0.lv.png"),
        lv,
        mk,
        w // 2,
        len(rows) - 3,
    )


def fb_bookshelf(name="fb_bookshelf"):
    fw = fh = 1
    S = Spr()
    H = 32
    b = Box(S, fw, fh, 0, 1, 7, 7, 0, H, 1, 1, 2, edge=3)
    J, I = b.J, b.I
    rnd = random.Random(3)
    for shelf_z in (1, 9, 17, 25):
        J.rect(1, J.wid - 1, shelf_z + 1, shelf_z + 7, 3)  # dark back
        s = 1
        while s < J.wid - 2:
            w = 2 if rnd.random() < 0.7 else 1
            h = rnd.choice((4, 5, 6))
            lv = rnd.choice((0, 0, 2, 1))
            J.rect(s, s + w, shelf_z + 1, shelf_z + 1 + h, lv)
            s += w
        J.hline(0, J.wid, shelf_z, 3)
    for shelf_z in (1, 9, 17, 25):
        I.hline(0, I.wid, shelf_z, 3)
    I.vline(I.wid - 1, 0, H, 3)
    finish(name, S, "object", fw, fh)


def fb_tv(name="fb_tv"):
    fw = fh = 1
    S = Spr()
    Box(S, fw, fh, 0, 1, 7, 6, 0, 8, 0, 0, 1, edge=3)  # cabinet
    c = Box(S, fw, fh, 0, 1, 7, 6, 0, 8, 0, 0, 1, edge=3)
    c.J.hline(2, c.J.wid - 2, 3, 2)
    p = Box(S, fw, fh, 1, 3, 6, 4, 8, 20, 1, 3, 2, edge=3)  # panel
    p.J.rect(1, p.J.wid - 1, 9, 19, 3)
    for k in range(5):  # glare
        p.J.px(2 + k, 17 - k, 1)
    p.J.hline(3, 9, 11, 2)
    finish(name, S, "object", fw, fh)


def fb_pc(name="fb_pc"):
    fw = fh = 1
    S = Spr()
    d = Box(S, fw, fh, 0, 1, 7, 7, 0, 9, 0, 0, 1, edge=3)  # desk
    d.J.rect(2, d.J.wid - 2, 2, 7, 1)
    d.J.hline(2, d.J.wid - 2, 4, 3)
    Box(S, fw, fh, 3, 3, 4, 4, 9, 11, 2, 2, 2, edge=None)  # monitor neck
    m = Box(S, fw, fh, 1, 2, 6, 3, 10, 21, 1, 3, 2, edge=3)  # monitor
    m.J.rect(1, m.J.wid - 1, 11, 20, 3)
    m.J.hline(2, 8, 17, 1)
    m.J.hline(2, 6, 15, 1)
    m.J.hline(2, 7, 13, 1)
    kb = Box(S, fw, fh, 2, 4, 5, 6, 9, 10, 0, 1, 2, edge=3)  # keyboard
    finish(name, S, "object", fw, fh)


def fb_machine(name="fb_machine"):
    fw = fh = 1
    S = Spr()
    b = Box(S, fw, fh, 1, 1, 6, 6, 0, 24, 0, 0, 1, edge=3)
    J, I = b.J, b.I
    J.rect(1, J.wid - 1, 0, 3, 2)  # base
    J.rect(2, 9, 12, 20, 3)  # screen
    for k in range(6):  # waveform
        J.px(3 + k, 15 + (k % 3 if k < 4 else 5 - k), 1)
    J.px(3, 18, 0)
    J.px(6, 9, 3)  # buttons
    J.px(8, 9, 2)
    J.hline(2, 9, 6, 2)  # vents
    J.hline(2, 9, 4, 2)
    I.vline(3, 3, 22, 2)
    t = Box(S, fw, fh, 2, 2, 5, 5, 24, 29, 0, 1, 2, edge=3)
    t.J.px(2, 26, 3)
    finish(name, S, "object", fw, fh)


# ---------------------------------------------------------------- M4-D: Ciudad Nodo buildings and interior furniture
def crystal_gem(S, cx, cy, hh, hw):
    """Cut crystal (our emblem; NOT a ball): flat table on top, crown widening to the girdle, pavilion narrowing to a point.
    Half-height `hh`, half-width `hw` px around (cx, cy), anchor-relative pixels. Lit left, shaded right, 1-px black ring.
    """
    girdle = cy - hh // 3
    for y in range(cy - hh, cy + hh + 1):
        if y <= girdle:
            w = hw // 2 + (hw - hw // 2) * (y - (cy - hh)) // max(1, girdle - (cy - hh))
        else:
            w = hw * (cy + hh - y) // max(1, cy + hh - girdle)
        for x in range(cx - w, cx + w + 1):
            ring = abs(x - cx) >= w or y == cy - hh or y == cy + hh
            if ring:
                lv = 3
            elif y == girdle:
                lv = 3
            elif y <= cy - hh + 1:
                lv = 0  # the table
            elif y < girdle:
                lv = 0 if x < cx else 1  # crown: lit left, light-grey right
            else:
                lv = 1 if x < cx else 2  # pavilion: grey left, dark right
            S.px(x, y, lv)
    for y in range(girdle + 1, cy + hh):  # centre ridge of the pavilion
        S.px(cx, y, 3)
    S.px(cx - hw // 2, cy - hh // 2 + 1, 2)  # facet mark on the crown


def face_gem(F, s_mid, z_mid, hh, hw, face_lv=0):
    """A crystal drawn on a wall face (F = Face): diamond outline of half-height hh / half-width hw (px along the face)."""
    for z in range(-hh, hh + 1):
        w = hw * (hh - abs(z)) // hh
        for sx in range(s_mid - w, s_mid + w + 1):
            edge = abs(sx - s_mid) == w or abs(z) == hh
            F.px(sx, z_mid + z, 3 if edge else face_lv)
    F.px(s_mid - 1, z_mid + 1, 2)
    F.px(s_mid - 1, z_mid + 2, 2)


def fb_center(name="fb_center"):
    """Centro de curación: a wide light building with a dark frieze, a glass double door under a canopy, a crystal plaque over
    it and a big faceted crystal on the roof (own motif, no red roof / ball logo). 3 x 2, door on the front tile i=1.
    """
    fw, fh = 3, 2
    S = Spr()
    WH = 30
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 0, 0, 1, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 3, 1)
    I.rect(0, I.wid, 0, 3, 2)
    J.rect(0, J.wid, WH - 7, WH - 3, 2)  # dark frieze under the roof
    I.rect(0, I.wid, WH - 7, WH - 3, 2)
    J.hline(0, J.wid, WH - 8, 3)
    I.hline(0, I.wid, WH - 8, 3)
    for s0 in (2, 30):  # windows beside the door
        J.window(s0, s0 + 10, 7, 21, glass=1, cross=True)
    # glass double door in the middle tile (s 14..27)
    J.rect(16, 26, 0, 17, 3)
    J.rect(17, 25, 0, 16, 1)
    J.vline(21, 0, 17, 3)
    J.px(19, 8, 0)
    J.px(23, 8, 0)
    J.hline(15, 27, 18, 3)  # canopy line
    J.rect(15, 28, 19, 21, 2)
    # crystal plaque over the door
    J.rect(14, 29, 22, 27, 3)
    face_gem(J, 21, 24, 2, 6, 0)
    for s0 in (3, 16):
        I.window(s0, s0 + 9, 8, 21, glass=0)
    r = roof(S, fw, fh, WH, 3)
    rz = WH + 3
    Box(S, fw, fh, 1, 1, 20, 13, rz, rz + 1, 1, 1, 2, edge=2)
    Box(
        S, fw, fh, 14, 8, 19, 12, rz + 1, rz + 4, 0, 1, 2, edge=3
    )  # roof plinth under the gem
    Box(S, fw, fh, 3, 2, 7, 5, rz + 1, rz + 3, 1, 1, 2, edge=3)  # rooftop unit
    # the roof gem: centred over the plinth (a = 16.5, b = 10)
    gx, gy = (
        -1,
        -9 - (rz + 4) - 10,
    )  # plinth centre a=16.5 b=10: x -1; (a-A)+(b-B) = -8.5; top z = rz + 4
    crystal_gem(S, gx, gy, 10, 6)
    finish(name, S, "object", fw, fh)


def fb_shop(name="fb_shop"):
    """Tienda: a small shop with a striped awning over the window and door, a display window, a roof board with a crystal
    mark. 2 x 2, door on the front tile i=1."""
    fw, fh = 2, 2
    S = Spr()
    WH = 26
    b = Box(S, fw, fh, 0, 0, 14, 14, 0, WH, 1, 0, 1, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 3, 1)
    I.rect(0, I.wid, 0, 3, 2)
    J.window(
        2, 12, 6, 17, glass=0, cross=False
    )  # display window (goods behind the glass)
    J.rect(4, 6, 7, 11, 2)
    J.rect(7, 10, 7, 9, 1)
    J.hline(3, 11, 6, 3)
    J.rect(17, 25, 0, 16, 3)  # door
    J.rect(18, 24, 0, 15, 2)
    J.vline(21, 0, 15, 3)
    J.px(19, 7, 0)
    J.px(23, 7, 0)
    for z in range(17, 21):  # striped awning across the whole front (z 17..20)
        for sx in range(0, J.wid):
            J.px(sx, z, 3 if (sx // 2) % 2 == 0 else 0)
    J.hline(0, J.wid, 17, 3)
    for sx in range(0, J.wid, 4):  # scalloped fringe
        J.px(sx, 16, 3)
        J.px(sx + 1, 16, 3)
    J.hline(0, J.wid, WH - 4, 1)
    I.hline(0, I.wid, WH - 4, 2)
    I.window(3, 11, 9, 20, glass=0)
    I.window(16, 24, 9, 20, glass=0)
    r = roof(S, fw, fh, WH, 3)
    rz = WH + 3
    Box(S, fw, fh, 1, 1, 13, 13, rz, rz + 1, 1, 1, 2, edge=2)
    # roof board (faces +j) with a crystal mark, on two posts
    for a in (3, 10):
        Box(S, fw, fh, a, 7, a + 1, 8, rz + 1, rz + 6, 3, 3, 3, edge=None)
    bd = Box(S, fw, fh, 1, 6, 12, 8, rz + 5, rz + 15, 0, 0, 1, edge=3)
    bd.J.rect(1, bd.J.wid - 1, 1, 9, 2)
    face_gem(bd.J, bd.J.wid // 2, 5, 3, 4, 0)
    finish(name, S, "object", fw, fh)


def fb_gym(name="fb_gym"):
    """Gimnasio (closed): a heavy 3 x 2 block with pilasters, no windows, a big shut double door barred with planks, a plaque and a
    big crystal on the roof. No walkable door (the door cell carries a text interaction).
    """
    fw, fh = 3, 2
    S = Spr()
    WH = 34
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 1, 1, 2, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 4, 2)
    I.rect(0, I.wid, 0, 4, 3)
    for s in range(0, J.wid, 14):  # pilasters along the front, one per tile
        J.vline(s, 4, WH - 3, 3)
        J.vline(s + 1, 4, WH - 3, 2)
    for s in range(0, I.wid, 14):
        I.vline(s, 4, WH - 3, 3)
    J.hline(0, J.wid, WH - 4, 3)
    I.hline(0, I.wid, WH - 4, 3)
    J.rect(0, J.wid, WH - 3, WH, 2)
    I.rect(0, I.wid, WH - 3, WH, 3)
    # the shut double door (middle tile: s 14..27), barred
    J.rect(16, 26, 0, 22, 3)
    J.rect(17, 25, 0, 21, 1)
    J.vline(21, 0, 21, 3)
    J.vline(22, 0, 21, 3)
    for k in range(0, 8):  # two crossing planks (an X over both leaves)
        J.px(17 + k, 2 + (k * 19) // 8, 3)
        J.px(18 + k, 2 + (k * 19) // 8, 3)
        J.px(24 - k, 2 + (k * 19) // 8, 3)
        J.px(23 - k, 2 + (k * 19) // 8, 3)
    J.rect(14, 29, 23, 29, 3)  # plaque above the door
    J.rect(15, 28, 24, 28, 0)
    face_gem(J, 21, 26, 2, 5, 1)
    r = roof(S, fw, fh, WH, 3)
    rz = WH + 3
    Box(S, fw, fh, 1, 1, 20, 13, rz, rz + 1, 1, 1, 2, edge=2)
    Box(S, fw, fh, 14, 8, 19, 12, rz + 1, rz + 4, 0, 1, 2, edge=3)  # roof plinth
    gx, gy = -1, -9 - (rz + 4) - 13
    crystal_gem(S, gx, gy, 13, 8)
    finish(name, S, "object", fw, fh)


def gym_marea(name="gym_marea"):
    """Gimnasio de Agua (C2): the Nodo gym's heavy 3 x 2 block with an OPEN doorway (the middle tile, walkable) and the sea on it: a wave band round the
    front, ripples inside the doorway, a water-drop emblem on the plaque and the roof crystal.
    """
    fw, fh = 3, 2
    S = Spr()
    WH = 34
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 1, 1, 2, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 4, 2)
    I.rect(0, I.wid, 0, 4, 3)
    for sf, lvb, lvd in (
        (J, 0, 2),
        (I, 1, 3),
    ):  # the wave band (a white crest line over the lower wall)
        for sx in range(sf.wid):
            crest = 10 + int(round(1.8 * math.sin(sx * 0.62)))
            sf.px(sx, crest, lvd)
            sf.px(sx, crest + 1, lvb)
    for s_ in range(0, J.wid, 14):  # pilasters along the front, one per tile
        J.vline(s_, 16, WH - 3, 3)
        J.vline(s_ + 1, 16, WH - 3, 2)
    for s_ in range(0, I.wid, 14):
        I.vline(s_, 16, WH - 3, 3)
    J.hline(0, J.wid, WH - 4, 3)
    I.hline(0, I.wid, WH - 4, 3)
    J.rect(0, J.wid, WH - 3, WH, 2)
    I.rect(0, I.wid, WH - 3, WH, 3)
    J.rect(
        15, 27, 0, 24, 3
    )  # the open doorway (middle tile): dark inside, white ripples at the bottom
    J.rect(17, 25, 0, 22, 3)
    for sx in range(17, 26):
        J.px(sx, 1 + (sx % 3 == 0), 0)
        J.px(sx, 4 + (sx % 4 == 1), 1)
    J.rect(14, 29, 25, 31, 3)  # plaque above the door with the water drop
    J.rect(15, 28, 26, 30, 0)
    for z in range(26, 31):
        w = (z - 26) // 2
        for sx in range(21 - w, 22 + w):
            J.px(sx, z, 3 if z == 30 or sx in (21 - w, 21 + w) else 2)
    r = roof(S, fw, fh, WH, 3)
    rz = WH + 3
    Box(S, fw, fh, 1, 1, 20, 13, rz, rz + 1, 1, 1, 2, edge=2)
    Box(S, fw, fh, 14, 8, 19, 12, rz + 1, rz + 4, 0, 1, 2, edge=3)  # roof plinth
    gx, gy = -1, -9 - (rz + 4) - 13
    crystal_gem(S, gx, gy, 13, 8)
    finish(name, S, "object", fw, fh)


def fb_counter(name="fb_counter"):
    """Service counter segment (solid + C_COUNTER, 1x1): a white top, a front panel with a crystal inlay."""
    fw = fh = 1
    S = Spr()
    t = Box(S, fw, fh, 0, 0, 7, 7, 0, 12, 0, 1, 2, edge=3)
    t.J.rect(1, t.J.wid - 1, 1, 8, 2)
    t.J.rect(3, t.J.wid - 3, 3, 6, 0)
    t.J.px(t.J.wid // 2, 4, 3)
    t.J.px(t.J.wid // 2 - 1, 4, 2)
    t.J.px(t.J.wid // 2 + 1, 4, 2)
    t.I.rect(1, t.I.wid - 1, 1, 8, 3)
    finish(name, S, "object", fw, fh)


def fb_shelf(name="fb_shelf"):
    """Shop shelf (1x1, solid): three shelves of boxes and bottles."""
    fw = fh = 1
    S = Spr()
    H = 26
    b = Box(S, fw, fh, 0, 1, 7, 7, 0, H, 1, 1, 2, edge=3)
    J, I = b.J, b.I
    rnd = random.Random(11)
    for shelf_z in (1, 9, 17):
        J.rect(1, J.wid - 1, shelf_z + 1, shelf_z + 7, 3)
        s = 1
        while s < J.wid - 2:
            w = rnd.choice((2, 3))
            h = rnd.choice((3, 4, 5))
            lv = rnd.choice((0, 0, 1, 2))
            J.rect(s, s + w, shelf_z + 1, shelf_z + 1 + h, lv)
            if w == 3:
                J.px(s + 1, shelf_z + h, 3)  # a bottle neck / label mark
            s += w + (1 if rnd.random() < 0.4 else 0)
        J.hline(0, J.wid, shelf_z, 3)
    for shelf_z in (1, 9, 17):
        I.hline(0, I.wid, shelf_z, 3)
    I.vline(I.wid - 1, 0, H, 3)
    finish(name, S, "object", fw, fh)


def fb_heal(name="fb_heal"):
    """Healing machine of the Centro (1x1, solid): a cabinet with a tray of six crystal sockets lit, a glowing dome."""
    fw = fh = 1
    S = Spr()
    b = Box(S, fw, fh, 0, 1, 7, 7, 0, 14, 1, 0, 1, edge=3)
    J = b.J
    J.rect(1, J.wid - 1, 1, 5, 2)
    J.rect(2, J.wid - 2, 7, 12, 3)  # dark tray window
    for k in range(3):  # crystal sockets lit
        J.rect(3 + k * 3, 5 + k * 3, 8, 11, 0)
        J.px(3 + k * 3, 10, 1)
    top = Box(S, fw, fh, 1, 2, 6, 6, 14, 22, 0, 0, 1, edge=3)
    top.J.rect(1, top.J.wid - 1, 2, 7, 3)
    top.J.rect(2, top.J.wid - 2, 3, 6, 1)
    top.J.px(3, 5, 0)
    Box(S, fw, fh, 2, 3, 5, 5, 22, 25, 0, 1, 2, edge=3)  # cap
    crystal_gem(S, 1, -33, 4, 3)
    finish(name, S, "object", fw, fh)


# ---------------------------------------------------------------- tree / rock
def tree_code(name="tree_code"):
    S = Spr()
    # trunk (anchor row 0 = bottom vertex of the tile; foot sits 4 px above)
    for y in range(-13, -3):
        for x in range(-2, 2):
            S.px(x, y, 2 if x < 0 else 1)
    S.hline(-3, 2, -4, 2)
    S.px(-1, -9, 3)
    S.px(0, -7, 3)
    blobs = [
        (-5, -17, 7),
        (5, -17, 7),
        (0, -20, 8),
        (-4, -24, 6),
        (4, -24, 6),
        (0, -14, 6),
    ]
    # back to front: draw ring+fill per blob
    for cx, cy, r in blobs:
        S.disc(cx, cy, r, 2, ring=3)
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                d = (x - cx) ** 2 + (y - cy) ** 2
                if d <= (r - 2) ** 2 and (x - cx) * 0.6 + (y - cy) * 0.8 < -r * 0.25:
                    if S.get(x, y) == 2:
                        S.px(x, y, 1)  # lit upper-left
    rnd = random.Random(5)
    for _ in range(9):  # leaf specks in the shade
        x, y = rnd.randrange(-8, 9), rnd.randrange(-24, -12)
        if S.get(x, y) == 2 and S.get(x + 1, y) == 2:
            S.px(x, y, 3)
    finish(name, S, "object", 1, 1)


def rock_code(name="rock_code"):
    S = Spr()
    body = [(-8, -1), (-9, -6), (-5, -11), (1, -12), (7, -9), (9, -4), (6, -1)]
    S.poly(body, 2)
    S.poly([(-5, -11), (1, -12), (7, -9), (2, -7), (-6, -7)], 1)  # lit top
    S.poly([(-9, -6), (-6, -7), (-4, -3), (-5, -1), (-8, -1)], 3)  # shadow lobe
    S.px(1, -6, 3)
    S.px(2, -5, 3)
    S.px(2, -4, 3)
    finish(name, S, "object", 1, 1)


# ---------------------------------------------------------------- zone identity (M4 polish)
# One drawing per piece: tools/zone_mock.py (the approved mockups) imports these through load_lv(), it has no copy of its own.
# Ground tiles (28 x 15) leave level 0 TRANSPARENT where they can (no mask: 105 B in a bank), objects follow the usual rules.


def tile_uv(x, y):
    """Pixel (x, y) of the 28x15 tile canvas -> its position inside the tile (u along +i, v along +j, 0..1)."""
    dx, dy = x - 13.5, y - 7.0
    return dy / 14 + dx / 28 + 0.5, dy / 14 - dx / 28 + 0.5


def pave(name, seed=0):
    """City pavers: 2x2 stones per tile in a running bond, light-gray joints, a speck per stone."""
    S = tile_canvas()
    rnd = random.Random(seed)
    for y in range(15):
        for x in range(28):
            if not in_diamond(x, y, 7):
                continue
            u, v = tile_uv(x, y)
            row = int(v * 2) % 2
            uu = (u * 2 + 0.5 * row) % 1.0
            vv = (v * 2) % 1.0
            if vv < 0.09 or uu < 0.06:
                S.t(x, y, 2)
            elif rnd.random() < 0.035:
                S.t(x, y, 1)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def plaza(name, seed=0):
    """The plaza round the fountain: bigger flagstones (one per tile), darker joints."""
    S = tile_canvas()
    rnd = random.Random(seed)
    for y in range(15):
        for x in range(28):
            if not in_diamond(x, y, 7):
                continue
            u, v = tile_uv(x, y)
            if u < 0.05 or v < 0.08:
                S.t(x, y, 3 if rnd.random() < 0.5 else 2)
            elif abs(u - 0.5) < 0.04 and v > 0.5 or abs(v - 0.5) < 0.06 and u < 0.5:
                S.t(x, y, 2)
            elif rnd.random() < 0.03:
                S.t(x, y, 1)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def litter(name, seed=0, base=1):
    """Forest floor: leaf litter on a gray ground (base 1) with dark leaves and a few light flecks."""
    S = tile_canvas()
    rnd = random.Random(seed)
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y, 7):
                S.t(x, y, base)
    for _ in range(9):
        x, y = rnd.randrange(3, 25), rnd.randrange(2, 13)
        shape = rnd.choice(((0, 0, 1, 0), (0, 0, 0, 1, 1, 1), (0, 0, 1, 0, 2, 1)))
        for k in range(0, len(shape), 2):
            if in_diamond(x + shape[k], y + shape[k + 1], 6):
                S.t(x + shape[k], y + shape[k + 1], 3 if k == 0 else 2)
    for _ in range(4):
        x, y = rnd.randrange(3, 25), rnd.randrange(2, 13)
        if in_diamond(x, y, 6):
            S.t(x, y, 0)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def trail(name, seed=0):
    """Forest trail: packed earth (white) with dark pebbles, so it reads against the gray floor."""
    S = tile_canvas()
    rnd = random.Random(seed)
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y, 7):
                S.t(x, y, 0)
                if rnd.random() < 0.05:
                    S.t(x, y, 2)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def water(name, phase, edges):
    """Pond: level-2 water with white ripples (they drift with `phase`); `edges` = sides facing land:
    'tl', 'tr' get a bank (dark lip + shadow band), 'bl', 'br' a dark rim."""
    S = tile_canvas()
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y, 7):
                S.t(x, y, 2)
    for r, y in enumerate((4, 8, 12)):
        for x0 in range(-8, 30, 9):
            x = x0 + (4 if r % 2 else 0) + phase
            for k, dy in ((0, 0), (1, -1), (2, -1), (3, 0)):
                if in_diamond(x + k, y + dy, 6):
                    S.t(x + k, y + dy, 0)
    for y in range(15):
        for x in range(28):
            if not in_diamond(x, y, 7):
                continue
            u, v = tile_uv(x, y)
            if "tl" in edges and u < 0.16:
                S.t(x, y, 3 if u < 0.07 else 1)
            if "tr" in edges and v < 0.16:
                S.t(x, y, 3 if v < 0.07 else 1)
            if "br" in edges and u > 0.95:
                S.t(x, y, 3)
            if "bl" in edges and v > 0.95:
                S.t(x, y, 3)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def pine(name, dark=False, tiers=4, seed=1):
    S = Spr()
    for y in range(-7, -1):
        for x in range(-1, 2):
            S.px(x, y, 3 if x == 1 or dark else 2)
    rnd = random.Random(seed)
    for k in range(tiers):
        base = -6 - 7 * k
        hw = 10 - 2 * k
        hgt = 11
        for y in range(base - hgt, base + 1):
            t = (base - y) / hgt
            w = int(round(hw * (1 - t))) + (1 if k == tiers - 1 and t > 0.8 else 0)
            for x in range(-w, w + 1):
                lv = 2
                if x < -w * 0.15 and y < base - 1 and (x + y) % 2 == 0:
                    lv = 1  # lit left side, dithered
                if (
                    dark
                ):  # forest-light: a lighter body than the first cut, still denser than pine_a/b
                    if x < -w * 0.55 and y < base - 1:
                        lv = 1  # solid lit left edge
                    elif x > w * 0.55 or (x > w * 0.25 and (x + y) % 2 == 0):
                        lv = 3  # shadow side: solid on the edge, dithered inward
                S.px(x, y, lv)
        for x in range(-hw, hw + 1):  # the shadowed hem of the tier
            if (
                abs(x) <= hw - 1
                and (x + k) % 3
                and not (dark and x < -hw * 0.3 and x % 2)
            ):
                S.px(x, base, 3)
        for _ in range(3):
            x, y = rnd.randrange(-hw + 2, hw - 1), base - rnd.randrange(2, 6)
            if S.get(x, y) in (1, 2):
                S.px(x, y, 3)
    finish(name, S, "object", 1, 1)


def bush(name="bush", seed=2):
    S = Spr()
    for cx, cy, r in ((-5, -6, 5), (5, -6, 5), (0, -9, 6)):
        S.disc(cx, cy, r, 2, ring=3)
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                d = (x - cx) ** 2 + (y - cy) ** 2
                if (
                    d <= (r - 2) ** 2
                    and (x - cx) * 0.6 + (y - cy) * 0.8 < -r * 0.2
                    and S.get(x, y) == 2
                ):
                    S.px(x, y, 1)
    rnd = random.Random(seed)
    for _ in range(6):
        x, y = rnd.randrange(-8, 9), rnd.randrange(-13, -3)
        if S.get(x, y) == 2:
            S.px(x, y, 3)
    for x, y in ((-6, -9), (3, -12)):  # two white blossoms: the countryside
        S.px(x, y, 0)
    finish(name, S, "object", 1, 1)


def lamp(name="lamp"):
    S = Spr()
    for y in range(-24, 0):
        S.px(0, y, 3)
        S.px(1, y, 2)
    Box(S, 1, 1, 2, 2, 5, 5, 0, 3, 2, 2, 3, edge=3)
    for y in range(-31, -24):
        for x in range(-3, 5):
            S.px(x, y, 3 if x in (-3, 4) or y in (-31, -25) else 0)
    S.px(0, -28, 1)
    S.px(1, -28, 1)
    S.hline(-4, 5, -32, 3)
    S.hline(-2, 3, -33, 3)
    S.px(0, -34, 3)
    S.px(1, -34, 3)
    S.outline(3)
    S.flat_white()  # no mask (see Spr.flat_white)
    finish(name, S, "object", 1, 1, outline=False)


def bench(name="bench"):
    S = Spr()
    Box(S, 1, 1, 1, 2, 6, 5, 4, 6, 0, 1, 2, edge=3)  # seat
    Box(S, 1, 1, 1, 1, 6, 2, 6, 11, 0, 1, 2, edge=3)  # back rest
    for a in (1, 5):
        Box(S, 1, 1, a, 4, a + 1, 5, 0, 4, 2, 2, 3, edge=None)
    S.outline(3)
    S.flat_white()  # no mask (see Spr.flat_white)
    finish(name, S, "object", 1, 1, outline=False)


def fountain(name="fountain", f=0):
    """2x2 fountain: round stone basin, water, a spout whose jet and drops move with frame f."""
    S = Spr()
    cx, cy = 0, -14

    def ell(rx, ry, y0, lv, fill=True, ring=None):
        for y in range(-ry - 1, ry + 2):
            for x in range(-rx - 1, rx + 2):
                d = (x / rx) ** 2 + (y / ry) ** 2
                if fill and d <= 1.0:
                    S.px(cx + x, y0 + y, lv)
                elif ring is not None and 1.0 < d <= 1.0 + 2.2 / ry:
                    S.px(cx + x, y0 + y, ring)

    H = 6
    for z in range(H, -1, -1):  # basin wall: the front half shows
        ell(23, 11, cy - z, 2 if z < H else 1)
    for x in range(-23, 24, 4):  # stone joints on the wall
        y = cy + int(11 * math.sqrt(max(0.0, 1 - (x / 23) ** 2)))
        for z in (2, 3, 4):
            S.px(x, y - z, 3)
    ell(23, 11, cy - H, 0)  # rim
    ell(19, 9, cy - H, 2)  # water
    for k, (wx, wy) in enumerate(((-12, -2), (8, 3), (-3, 5), (12, -4), (-8, 3))):
        x = wx + (f // 2 + k) % 3 - 1
        S.hline(cx + x, cx + x + 2, cy - H + wy, 0)
    for y in range(cy - H - 12, cy - H + 1):  # pedestal
        S.px(-1, y, 1)
        S.px(0, y, 1)
        S.px(1, y, 2)
        S.px(2, y, 2)
    ell(7, 3, cy - H - 12, 1)  # upper bowl
    S.hline(-6, 7, cy - H - 10, 2)
    top = cy - H - 22
    for y in range(top, cy - H - 12):  # the jet
        S.px(0, y, 0)
        S.px(1, y, 0)
    for k in range(4):  # drops arching out, both sides
        t = ((f + k * 3) % 12) / 12.0
        dx = int(2 + t * 7)
        dy = int(-4 * t * (1 - t) * 14 + t * 10)
        for sgn in (-1, 1):
            S.px(sgn * dx + (1 if sgn > 0 else 0), top + 2 + dy, 0)
    S.outline(3)
    for y in range(top, cy - H - 12):  # keep the jet white over the outline
        S.px(0, y, 0)
        S.px(1, y, 0)
    S.flat_white()  # no mask (see Spr.flat_white)
    finish(name, S, "object", 2, 2, outline=False)


def planter(name="planter"):
    """A city tree in a square stone box."""
    S = Spr()
    Box(S, 1, 1, 0, 0, 7, 7, 0, 6, 2, 1, 2, edge=3)
    for y in range(-16, -8):
        S.px(0, y, 2)
        S.px(1, y, 3)
    for cx, cy, r in ((-4, -19, 5), (4, -19, 5), (0, -23, 6)):
        S.disc(cx, cy, r, 2, ring=3)
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                if (
                    (x - cx) ** 2 + (y - cy) ** 2 <= (r - 2) ** 2
                    and (x - cx) + (y - cy) < -r * 0.4
                    and S.get(x, y) == 2
                ):
                    S.px(x, y, 1)
    finish(name, S, "object", 1, 1)


def picket(name, axis="i"):
    """White picket fence (Villa Aliso): pointed pales on two rails."""
    S = Spr()
    P = 5
    if axis == "i":
        Box(S, 1, 1, 0, P, 7, P + 1, 3, 5, 0, 0, 1, edge=3)
        for a in (0, 2, 4, 6):
            Box(S, 1, 1, a, P, a + 1, P + 1, 0, 11, 0, 0, 1, edge=3)
    else:
        Box(S, 1, 1, P, 0, P + 1, 7, 3, 5, 0, 0, 1, edge=3)
        for b in (0, 2, 4, 6):
            Box(S, 1, 1, P, b, P + 1, b + 1, 0, 11, 0, 0, 1, edge=3)
    finish(name, S, "object", 1, 1)


def mailbox(name="mailbox"):
    S = Spr()
    for y in range(-11, 0):
        S.px(0, y, 2)
        S.px(1, y, 3)
    Box(S, 1, 1, 2, 3, 6, 5, 11, 16, 0, 1, 2, edge=3)
    for y in range(-24, -18):  # the red flag (dark here)
        S.px(5, y, 3)
    S.hline(5, 7, -24, 3)
    S.hline(5, 7, -23, 3)
    finish(name, S, "object", 1, 1)


def flowerbed(name="flowerbed"):
    S = Spr()
    Box(S, 1, 1, 0, 0, 7, 7, 0, 3, 2, 1, 2, edge=3)
    rnd = random.Random(9)
    for k in range(7):
        a2, b2 = 2 + (k * 5) % 11, 2 + (k * 7) % 11
        x = a2 - b2
        y = (a2 + b2) // 2 - 14 - 3
        for dx, dy, lv in ((0, 0, 3), (-1, 0, 0), (1, 0, 0), (0, -1, 0), (0, 1, 0)):
            S.px(x + dx, y + dy, lv)
    finish(name, S, "object", 1, 1)


def log(name="log"):
    S = Spr()
    Box(S, 1, 1, 0, 2, 7, 5, 0, 5, 1, 2, 2, edge=3)
    for sx in range(0, 14, 3):
        S.px(-12 + sx, -8 + sx // 2, 3)
    for dx, dy in ((10, -5), (11, -5), (10, -6), (12, -4)):
        S.px(dx, dy, 1)
    finish(name, S, "object", 1, 1)


def stump(name="stump"):
    S = Spr()
    for z in range(0, 7):
        for x in range(-6, 7):
            yy = int(3 * math.sqrt(max(0.0, 1 - (x / 6.5) ** 2)))
            S.px(x, -4 + yy - z, 2 if x > 1 else 1)
    for y in range(-4, 4):
        for x in range(-7, 8):
            if (x / 6.5) ** 2 + (y / 3.2) ** 2 <= 1:
                S.px(x, y - 10, 0)
    for y in range(-2, 3):
        for x in range(-4, 5):
            d = (x / 4) ** 2 + (y / 2) ** 2
            if 0.55 < d <= 1:
                S.px(x, y - 10, 2)
    S.px(0, -10, 3)
    for x, y in ((-8, -4), (8, -4), (-9, -3), (9, -3)):  # roots
        S.px(x, y, 2)
    finish(name, S, "object", 1, 1)


def mushrooms(name="mushrooms"):
    S = Spr()
    for cx, cy, r in ((-4, -2, 3), (3, -1, 2), (0, -5, 2)):
        for y in range(cy - 1, cy + 2):
            S.px(cx, y, 0)
        for y in range(-r, 1):
            w = int(round(r * math.sqrt(max(0.0, 1 - (y / (r + 0.5)) ** 2))))
            for x in range(-w, w + 1):
                S.px(cx + x, cy - 2 + y, 2)
        S.px(cx - 1, cy - 2 - r + 1, 0)
    finish(name, S, "object", 1, 1)


def arch(name="arch"):
    """Wooden gateway at the forest edge (Ruta 2), across a 2-wide path (2 x 1 along i): posts, a beam, a board."""
    S = Spr()
    for a in (0, 13):
        Box(S, 2, 1, a, 3, a + 1, 4, 0, 30, 1, 2, 3, edge=3)
    Box(S, 2, 1, -1, 3, 15, 4, 28, 32, 1, 2, 2, edge=3)
    b = Box(S, 2, 1, 3, 3, 11, 4, 18, 26, 0, 0, 1, edge=3)
    b.J.hline(3, 14, 23, 2)
    b.J.hline(3, 10, 20, 2)
    for a in (2, 12):
        Box(S, 2, 1, a, 3, a + 1, 4, 26, 28, 2, 2, 2, edge=None)
    finish(name, S, "object", 2, 1)


# the pond's edge sets: tuples of the sides that face land ('tl' = toward -i, 'tr' = toward -j, 'br' = +i, 'bl' = +j); the file / catalog
# name is water_<sides joined by _> (water_c = open water). Only the sets a map uses are drawn (each is a 2-frame tile: _0 and _1).
WATER_SETS = [
    ("tl", "tr"),
    ("tr", "br"),
    ("tl", "bl"),
    ("br", "bl"),
]  # Villa Aliso's 2 x 2 pond


def water_name(edges):
    return "water_" + (
        "_".join(e for e in ("tl", "tr", "br", "bl") if e in edges) or "c"
    )


def zone_pieces():
    for k, nm in enumerate("ab"):
        pave("pave_" + nm, 1 + k)
        plaza("plaza_" + nm, 3 + k)
        trail("trail_" + nm, k)
    for k, nm in enumerate("abcd"):
        litter("litter_" + nm, k, 1)
    for edges in WATER_SETS:
        for fr, phase in enumerate((0, 4)):
            water("%s_%d" % (water_name(edges), fr), phase, edges)
    pine("pine_a", seed=3)
    pine("pine_b", seed=7)
    for k, seed in enumerate((4, 5, 6)):
        pine("pine_dark_" + "abc"[k], dark=True, seed=seed)
    bush()
    lamp()
    bench()
    planter()
    fountain()
    picket("picket_j", "i")  # same convention as fence_j: runs along +i, face +j
    picket("picket_i", "j")  # runs along +j, face +i
    mailbox()
    flowerbed()
    log()
    stump()
    mushrooms()
    arch()


# ---------------------------------------------------------------- Pueblo Cantera and the Rock gym (M5-D, contract 26)
# The user's pick "M Cantera mezcla" (town) and "G3" (gym floor) of tools/town_mock.py; tools/town_mock.py loads these files (one copy of each drawing).
def gravel_canvas(seed=0, stones=7, specks=14):
    """Quarry gravel: white dust with small lit stones (level 1 body, dark underside) and dark specks."""
    S = tile_canvas()
    rnd = random.Random(seed)
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y, 7):
                S.t(x, y, 0)
    for _ in range(stones):
        x, y = rnd.randrange(4, 24), rnd.randrange(3, 12)
        w = rnd.choice((2, 3))
        if all(in_diamond(x + k, y + d, 6) for k in range(-1, w + 1) for d in (-1, 1)):
            for k in range(w):
                S.t(x + k, y - 1, 1)
                S.t(x + k, y, 1)
            S.t(x, y - 1, 0)
            for k in range(-1, w + 1):
                S.t(x + k, y + 1, 3 if 0 <= k < w else 2)
    for _ in range(specks):
        x, y = rnd.randrange(2, 26), rnd.randrange(1, 14)
        if in_diamond(x, y, 6):
            S.t(x, y, rnd.choice((2, 2, 3)))
    return S


def gravel(name, seed=0, stones=7, specks=14):
    finish(
        name,
        gravel_canvas(seed, stones, specks),
        "tile",
        1,
        1,
        tile=True,
        outline=False,
    )


def rails(name="rails_a", seed=0):
    """Mine-cart track along +i on gravel: two dark rails, light sleepers."""
    S = tile_canvas()
    base = gravel_canvas(seed, stones=3, specks=8)
    for y in range(15):
        for x in range(28):
            if base.mk[y][x]:
                S.t(x, y, base.lv[y][x])
    for y in range(15):
        for x in range(28):
            if not in_diamond(x, y, 7):
                continue
            u, v = tile_uv(x, y)
            if abs((u * 4) % 1.0 - 0.5) < 0.16 and 0.22 < v < 0.78:
                S.t(x, y, 2)  # sleepers
            if abs(v - 0.32) < 0.035 or abs(v - 0.68) < 0.035:
                S.t(x, y, 3)  # rails
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def big_slab(name, pi, pj, seed=0):
    """Gym floor (G3): one big slab per 2 x 2 tiles. The joint lines run along the tile edges with an EVEN i (u side) / EVEN j (v side), so the
    tile variant depends on the cell parity (pi = i % 2, pj = j % 2): slab_00 has both joints, slab_10 only the v one, slab_01 only the u one,
    slab_11 none. White with a few cracks."""
    S = tile_canvas()
    rnd = random.Random(seed + pi * 31 + pj * 17)
    for y in range(15):
        for x in range(28):
            if not in_diamond(x, y, 7):
                continue
            u, v = tile_uv(x, y)
            lv = 0
            if (pi == 0 and u < 0.05) or (pj == 0 and v < 0.08):
                lv = 3
            elif rnd.random() < 0.025:
                lv = 1
            S.t(x, y, lv)
    if rnd.random() < 0.4:
        x, y = rnd.randrange(8, 20), rnd.randrange(4, 10)
        for k in range(5):
            if in_diamond(x + k, y + k // 2, 6):
                S.t(x + k, y + k // 2, 2)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def cliff(name, seed=0, h=30):
    """Rock wall block (the town's border instead of trees): faceted strata, lit top."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, h, 1, 1, 2, edge=3)
    rnd = random.Random(seed)
    for face, lv_line in ((b.J, 2), (b.I, 3)):
        z = 3
        while z < h - 2:  # strata: wavy dark lines
            off = rnd.randrange(3)
            for s in range(face.wid):
                face.px(s, z + ((s + off) // 4) % 2, lv_line)
            z += rnd.choice((5, 6, 7))
        for _ in range(4):  # cracks
            s, z0 = rnd.randrange(1, face.wid - 1), rnd.randrange(2, h - 6)
            for k in range(4):
                face.px(s, z0 + k, 3)
    for _ in range(5):  # pebbles on the lit top
        a2, b2 = rnd.randrange(2, 12), rnd.randrange(2, 12)
        b.top_px(a2, b2, 2)
    finish(name, S, "object", 1, 1)


def boulder(name, seed=0):
    S = Spr()
    rnd = random.Random(seed)
    for cx, cy, rx, ry in ((-4, -6, 7, 6), (5, -5, 6, 5), (0, -11, 6, 5)):
        for y in range(-ry, ry + 1):
            for x in range(-rx, rx + 1):
                if (x / rx) ** 2 + (y / ry) ** 2 <= 1:
                    lv = 1 if x * 0.6 + y * 0.8 < -2 else 2
                    if x * 0.5 + y * 0.9 > ry * 0.6:
                        lv = 3
                    S.px(cx + x, cy + y, lv)
    for _ in range(4):
        x, y = rnd.randrange(-8, 9), rnd.randrange(-14, -2)
        if S.get(x, y) in (1, 2):
            S.px(x, y, 3)
    finish(name, S, "object", 1, 1)


def cart(name="cart"):
    """Mine cart full of ore, on the rails (along +i)."""
    S = Spr()
    b = Box(S, 1, 1, 1, 2, 6, 5, 3, 11, 1, 2, 3, edge=3)
    b.J.hline(0, b.J.wid, 7, 3)
    b.I.hline(0, b.I.wid, 7, 3)
    for x, y, r in ((-3, -15, 3), (2, -16, 3), (-1, -18, 2)):  # ore heap
        S.disc(x, y, r, 3, ring=3)
        S.px(x - 1, y - 1, 1)
    for x, y in ((-7, -2), (-1, 1), (4, -1), (9, -4)):  # wheels
        S.disc(x, y, 2, 3)
        S.px(x, y, 1)
    finish(name, S, "object", 1, 1)


def pickaxe_sign(name="pickaxe_sign"):
    """Town sign with a crossed pick and shovel (the quarry emblem)."""
    S = Spr()
    Box(S, 1, 1, 3, 3, 4, 4, 0, 12, 1, 1, 2, edge=None)
    b = Box(S, 1, 1, 0, 3, 7, 4, 10, 22, 0, 0, 1, edge=3)
    for k in range(7):
        b.J.px(3 + k, 12 + k, 3)
        b.J.px(9 - k, 12 + k, 3)
    b.J.hline(1, 5, 19, 3)
    b.J.hline(7, 11, 19, 3)
    finish(name, S, "object", 1, 1)


def stone_house(name="stone_house"):
    """2 x 2 house of dressed stone (block courses, dark mortar), slate roof with a chimney."""
    fw, fh = 2, 2
    S = Spr()
    WH = 26
    b = Box(S, fw, fh, 0, 0, 14, 14, 0, WH, 0, 0, 1, edge=3)
    J, I = b.J, b.I
    for face, lv in ((J, 2), (I, 2)):
        for z in range(4, WH - 2, 5):  # courses
            face.hline(0, face.wid, z, lv)
            sh = 0 if (z // 5) % 2 else 3
            for s in range(sh, face.wid, 6):  # staggered joints
                for k in range(1, 5):
                    face.px(s, z + k, lv)
    J.rect(0, J.wid, 0, 3, 2)
    I.rect(0, I.wid, 0, 3, 3)
    J.window(3, 11, 11, 21, glass=1)
    J.rect(17, 25, 0, 18, 3)  # door
    J.rect(18, 24, 0, 17, 1)
    J.px(23, 8, 3)
    for z in (5, 11):
        J.hline(18, 24, z, 2)
    I.window(6, 14, 11, 21, glass=0)
    I.window(18, 26, 11, 21, glass=0)
    r = Box(
        S, fw, fh, -1, -1, 15, 15, WH, WH + 3, 2, 3, 3, edge=3
    )  # slate roof: a low hip with rows of tiles
    for a in range(0, 16, 2):
        r.top_line_b(a, -1, 15, 3, dash=(4, 3))
    Box(S, fw, fh, 9, 2, 12, 5, WH + 3, WH + 11, 1, 2, 3, edge=3)  # chimney
    finish(name, S, "object", fw, fh)


def stage(name="stage"):
    """Garrick's dais: a 2 x 2 low platform of big stones with steps on the front."""
    S = Spr()
    b = Box(S, 2, 2, 0, 0, 14, 14, 0, 6, 1, 2, 3, edge=3)
    b.top_line_a(7, 0, 14, 2)
    b.top_line_b(7, 0, 14, 2)
    Box(S, 2, 2, 4, 14, 10, 16, 0, 3, 1, 2, 3, edge=3)
    finish(name, S, "object", 2, 2)


def pillar(name, seed=0):
    """Rough rock column in the gym."""
    S = Spr()
    rnd = random.Random(seed)
    b = Box(S, 1, 1, 1, 1, 6, 6, 0, 34, 1, 1, 2, edge=3)
    for face, lv in ((b.J, 2), (b.I, 3)):
        for _ in range(6):
            s, z = rnd.randrange(0, face.wid), rnd.randrange(2, 32)
            for k in range(3):
                face.px(s, z + k, lv)
    finish(name, S, "object", 1, 1)


def gym_cantera(name="gym_cantera"):
    """The Rock gym (3 x 2): stone courses, a stone-hexagon emblem above the open door (the middle tile of the front row), rocks on the roof."""
    fw, fh = 3, 2
    S = Spr()
    WH = 34
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 1, 1, 2, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 4, 2)
    I.rect(0, I.wid, 0, 4, 3)
    for face, lv in ((J, 2), (I, 3)):
        for z in range(6, WH - 4, 6):
            face.hline(0, face.wid, z, lv)
    J.rect(16, 26, 0, 20, 3)  # open door
    J.rect(17, 25, 0, 19, 2)
    J.vline(21, 0, 19, 3)
    for z in range(22, 32):  # emblem: a hexagonal stone above the door
        w = 6 - abs(z - 27)
        for s in range(21 - w - 2, 21 + w + 3):
            J.px(s, z, 1 if s < 21 else 2)
    J.hline(15, 28, 21, 3)
    roof(S, fw, fh, WH, 3)
    rz = WH + 3
    Box(S, fw, fh, 1, 1, 20, 13, rz, rz + 1, 1, 1, 2, edge=2)
    for x, y, rr in (
        (-6, -54, 5),
        (3, -55, 6),
        (-1, -60, 4),
    ):  # rocks piled on the roof
        S.disc(x, y, rr, 2, ring=3)
        S.px(x - 2, y - 2, 1)
        S.px(x - 1, y - 2, 1)
    finish(name, S, "object", fw, fh)


def cantera_pieces():
    for k in range(3):
        gravel("gravel_" + "abc"[k], k)
        cliff("cliff_" + "abc"[k], k)
    rails("rails_a", 0)
    for pi in (0, 1):
        for pj in (0, 1):
            big_slab("slab_%d%d" % (pi, pj), pi, pj, 1 + pi + 2 * pj)
    boulder("boulder", 5)
    cart()
    pickaxe_sign()
    stone_house()
    stage()
    pillar("pillar", 3)
    gym_cantera()


# ---------------------------------------------------------------- Chapter 2 pieces (C2: Ruta 3, Monte Eco, Ruta 4, Ciudad Marea, Gimnasio de Agua, Rutas 24-25)
# The approved look of every zone is variant A of tools/c2_mock.py; the drawings moved here as they were (same pixels), tools/c2_mock.py now
# loads these files like zone_mock / town_mock do. Sizes are bytes in a map's bank (pixels + mask if a level-0 pixel is opaque).
def flip_spr(S):
    """Horizontal mirror of a canvas around its anchor column (the iso grid is symmetric: a piece along +i becomes the one along +j)."""
    T = Spr((S.W) // 2, S.oy, S.H - 1 - S.oy)
    for y in range(S.H):
        for x in range(S.W):
            if S.mk[y][x]:
                x2 = 2 * S.ox - x
                if 0 <= x2 < S.W:
                    T.lv[y][x2] = S.lv[y][x]
                    T.mk[y][x2] = True
    return T


def spr_of_tile(name):
    """A generated ground tile (assets/art/tiles/<name>.lv.png) as a 28x15 canvas, to draw on top of it."""
    lv, mk, _m = load_lv(name)
    S = tile_canvas()
    for y in range(15):
        for x in range(28):
            if mk[y][x]:
                S.t(x, y, lv[y][x])
    return S


def dither(x, y, d=0.5):
    """Ordered dither: True where the pixel should take the second tone for a coverage d (0..1)."""
    m = (x & 3) * 4 + (y & 3)
    thr = (0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5)[m]
    return thr < d * 16


def blob_field(rnd, n, rx, ry):
    return [
        (
            rnd.uniform(4, 24),
            rnd.uniform(3, 12),
            rnd.uniform(rx * 0.6, rx),
            rnd.uniform(ry * 0.6, ry),
        )
        for _ in range(n)
    ]


def in_blobs(x, y, bl):
    return any(
        ((x - cx) / rx) ** 2 + ((y - cy) / ry) ** 2 <= 1 for cx, cy, rx, ry in bl
    )


def marks(S, rnd, n, lv, shapes=((0, 0),), hh=5, base=None):
    """n small marks (each a tuple-list of (dx, dy) pixels) on a tile canvas, kept inside the diamond."""
    k = 0
    tries = 0
    while k < n and tries < 200:
        tries += 1
        x, y = rnd.randrange(3, 25), rnd.randrange(2, 13)
        sh = rnd.choice(shapes)
        if all(in_diamond(x + dx, y + dy, hh) for dx, dy in sh):
            for dx, dy in sh:
                S.t(x + dx, y + dy, lv)
            k += 1


def gtile(fn, seed=0, hh=7):
    """Ground tile from a per-pixel function fn(x, y, u, v, rnd) -> level (None = leave transparent)."""
    S = tile_canvas()
    rnd = random.Random(seed)
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y, hh):
                u, v = tile_uv(x, y)
                lv = fn(x, y, u, v, rnd)
                if lv is not None:
                    S.t(x, y, lv)
    return S


def strata(face, rnd, h, lv, gap=(5, 7), z0=3):
    z = z0
    while z < h - 2:
        off = rnd.randrange(4)
        for s in range(face.wid):
            if (s + off) % 7 != 0:
                face.px(s, z + ((s + off) // 4) % 2, lv)
        z += rnd.randrange(gap[0], gap[1] + 1)


def speck_face(face, rnd, n, h, lv, z0=1):
    for _ in range(n):
        face.px(rnd.randrange(0, face.wid), rnd.randrange(z0, h), lv)


def rock_dither_face(face, rnd, h, lo, hi, cover=0.35):
    """Fill a face with `lo` (already) and scatter `hi` patches (dithered) for a rough rock look."""
    for _ in range(5):
        cs, cz = rnd.randrange(0, face.wid), rnd.randrange(2, h)
        rs, rz = rnd.randrange(3, 7), rnd.randrange(2, 5)
        for s in range(max(0, cs - rs), min(face.wid, cs + rs)):
            for z in range(max(0, cz - rz), min(h, cz + rz)):
                if ((s - cs) / rs) ** 2 + ((z - cz) / rz) ** 2 <= 1 and dither(
                    s, z, cover
                ):
                    face.px(s, z, hi)


def lump(S, cx, cy, rx, ry, rnd, tone=1, cracks=2):
    """One rounded rock lump (ellipse): outline ring, lit upper-left (0/1 dither), shaded lower-right (2, dithered 3 at the rim)."""
    for y in range(-ry - 1, ry + 2):
        for x in range(-rx - 1, rx + 2):
            d = (x / (rx + 0.5)) ** 2 + (y / (ry + 0.5)) ** 2
            if d <= 1.0:
                S.px(cx + x, cy + y, 3)
    for y in range(-ry, ry + 1):
        for x in range(-rx, rx + 1):
            d = (x / (rx - 0.2)) ** 2 + (y / (ry - 0.2)) ** 2
            if d <= 1.0:
                t = (x / rx) * 0.55 + (y / ry) * 0.85
                if t < -0.5:
                    lv = 0 if dither(x + cx, y + cy, 0.7) else tone
                elif t < 0.15:
                    lv = tone
                elif t < 0.65:
                    lv = 2
                else:
                    lv = 3 if dither(x + cx, y + cy, 0.6) else 2
                S.px(cx + x, cy + y, lv)
    for _ in range(cracks):
        x, y = cx + rnd.randrange(-rx // 2, rx // 2 + 1), cy + rnd.randrange(
            -ry // 2, ry // 2 + 1
        )
        for k in range(rnd.randrange(3, 6)):
            S.px(x, y, 3)
            x += rnd.choice((0, 1))
            y += 1


def tpx(S, x, y, lv):
    """Tile-canvas coordinates (28x15, centre (14,7)) on an object canvas whose anchor is the tile's bottom vertex."""
    S.px(x - 14, y - 14, lv)


def vein(face, rnd, phase, h, s0, period=6):
    """A glowing crystal vein climbing a wall face: a 2-px white line (the dashes crawl with `phase`) with lit fringe and big crystal studs."""
    s = s0
    pts = []
    for z in range(1, h - 1):
        pts.append((s, z))
        if z % 3 == 0:
            s = max(2, min(face.wid - 4, s + rnd.choice((-2, -1, 0, 1, 2))))
    for k, (s, z) in enumerate(pts):
        ph = (k + phase * 3) % period
        if ph < period - 2:
            face.px(s, z, 0)
            face.px(s + 1, z, 0)
            face.px(s - 1, z, 1)
            face.px(s + 2, z, 1)
        elif ph == period - 2:
            face.px(s, z, 1)
            face.px(s + 1, z, 1)
    for n, (s, z) in enumerate(
        pts[3::9]
    ):  # crystal studs: 3 x 5 white gems with a gray facet
        for dz, hw in ((-2, 0), (-1, 1), (0, 1), (1, 1), (2, 0)):
            for dx in range(-hw, hw + 1):
                face.px(s + dx, z + dz + 1, 0 if dx <= 0 else 1)
        if (n + phase) % 2 == 0:  # twinkle
            face.px(s, z + 4, 0)
            face.px(s - 2, z + 1, 0)
            face.px(s + 2, z + 1, 0)


def crate(name="crate", mark=None, big=False):
    """Wooden crate 1x1 (h 12): corner posts, a diagonal plank brace; optional mark drawn on the +j face ('V' = Vertice)."""
    S = Spr()
    h = 13 if big else 11
    b = Box(S, 1, 1, 1, 1, 6, 6, 0, h, 1, 0, 1, edge=3)
    J, I = b.J, b.I
    for face in (J, I):
        face.hline(0, face.wid, 2, 2)
        face.hline(0, face.wid, h - 3, 2)
        face.vline(1, 0, h, 2)
        face.vline(face.wid - 2, 0, h, 2)
    for k in range(min(J.wid - 4, h - 6)):
        J.px(2 + k, 3 + k, 2)
    b.top_line_a(3, 1, 6, 2)
    if mark == "V":  # the Vertice V, bold, on the +i face
        for k in range(5):
            I.px(4 + k, 9 - k, 3)
            I.px(13 - k, 9 - k, 3)
            I.px(5 + k, 9 - k, 3)
            I.px(12 - k, 9 - k, 3)
    finish(name, S, "object", 1, 1, outline=False)


def barrel(name="barrel"):
    S = Spr()
    for y in range(-13, 0):
        w = 6 - int(1.2 * ((y + 7) / 6.5) ** 2)
        for x in range(-w, w + 1):
            S.px(x, y, 1 if x < -1 else 2)
    for y in (-10, -4):
        S.hline(-6, 6, y, 3)
    for x in range(-6, 7):  # lid
        yy = int(1.5 * math.sqrt(max(0, 1 - (x / 6.5) ** 2)))
        for dy in range(-yy, yy + 1):
            S.px(x, -13 + dy, 0)
    finish(name, S, "object", 1, 1)


def bollard(name="bollard"):
    """Mooring post: short fat post with a rope coil."""
    S = Spr()
    for y in range(-11, -1):
        for x in range(-3, 4):
            S.px(x, y, 1 if x < 0 else 2)
    S.hline(-4, 4, -12, 3)
    S.hline(-4, 4, -11, 0)
    for y in (-8, -6, -4):
        S.hline(-3, 3, y, 3)
    S.hline(-2, 2, -2, 2)
    finish(name, S, "object", 1, 1)


def earth(name, kind, seed):
    """Dry packed earth: white with dithered light-gray patches, pebbles, hairline cracks; kind 'tuft' adds sparse dry grass."""
    rnd = random.Random(seed)
    bl = blob_field(rnd, 4, 6, 3)
    S = gtile(
        lambda x, y, u, v, r: 1 if in_blobs(x, y, bl) and dither(x, y, 0.4) else 0, seed
    )
    if kind == "pebbles":
        for _ in range(4):
            x, y = rnd.randrange(4, 24), rnd.randrange(3, 12)
            if in_diamond(x + 2, y + 1, 5) and in_diamond(x - 1, y - 1, 5):
                S.t(x, y, 1)
                S.t(x + 1, y, 1)
                S.t(x, y + 1, 3)
                S.t(x + 1, y + 1, 3)
                S.t(x + 2, y + 1, 2)
    elif kind == "cracks":
        for _ in range(2):
            x, y = rnd.randrange(6, 20), rnd.randrange(4, 10)
            for k in range(rnd.randrange(5, 9)):
                if in_diamond(x, y, 6):
                    S.t(x, y, 2)
                x += 1
                y += rnd.choice((-1, 0, 0, 1))
    elif kind == "tuft":
        for bx, by in ((rnd.randrange(8, 19), rnd.randrange(6, 11)),):
            for k in range(3):  # a dry grass clump: thin dark blades fanning out
                for dx in (-2, 0, 2):
                    S.t(bx + dx * (k + 1) // 2, by - k * 2 + (0 if dx else -1), 3)
            S.t(bx, by + 1, 2)
        marks(S, rnd, 3, 2, shapes=(((0, 0),),))
    else:
        marks(S, rnd, 5, 2, shapes=(((0, 0), (1, 0)), ((0, 0),)))
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def mtrail(name, seed):
    """The mountain trail: bare packed white, two faint wheel/foot ruts along the walking direction (+j) and a few kicked-up pebbles."""
    rnd = random.Random(seed)

    def f(x, y, u, v, r):
        if (abs(u - 0.3) < 0.035 or abs(u - 0.7) < 0.035) and int(v * 9) % 3 != 2:
            return 1
        return 0

    S = gtile(f, seed)
    marks(S, rnd, 4, 2, shapes=(((0, 0),), ((0, 0), (1, 0))))
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def trainer_spot(name, seed=0):
    """Trainer's standing spot: packed earth with a dotted iso rectangle of pebbles round a flat stone."""

    def f(x, y, u, v, r):
        d = max(abs(u - 0.5), abs(v - 0.5))
        if 0.38 < d < 0.45 and (int(u * 14) + int(v * 14)) % 2 == 0:
            return 3
        if d < 0.12:
            return 1
        return 0

    S = gtile(f, seed)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def slope(name, seed=0, tiers=3):
    """Low rock slope (the mountain's foot): a pile of 4-6 overlapping rock lumps, up to ~26 px, light stone with deep crevices.
    `tiers` 2 = a lower pile (~18 px)."""
    S = Spr()
    rnd = random.Random(seed)
    hmax = 26 if tiers >= 3 else 18
    lumps = [
        (-7, -6, 8, 6),
        (8, -6, 8, 6),
        (0, -3, 9, 4),
        (-3, -hmax // 2 - 3, 8, hmax // 2 - 1),
        (6, -hmax // 2 - 2, 7, hmax // 2 - 3),
        (-9, -hmax // 2 + 2, 5, 6),
    ]
    lumps.sort(key=lambda t: t[1] - t[3] * 0)  # back (top) first
    lumps.sort(key=lambda t: t[1])
    for cx, cy, rx, ry in lumps:
        lump(S, cx + rnd.randrange(-1, 2), cy, rx, ry, rnd, tone=1)
    finish(name, S, "object", 1, 1, outline=False)


def rock_fence(name, axis, seed=0):
    """Wooden post-and-rail fence 1 tile long (axis 'i' runs along +i, face +j), 2 posts, 2 rails, weathered."""
    S = Spr()
    P = 5
    rnd = random.Random(seed)
    for z0, z1 in ((4, 6), (8, 10)):
        if axis == "i":
            Box(S, 1, 1, 0, P, 7, P + 1, z0, z1, 1, 1, 2, edge=3)
        else:
            Box(S, 1, 1, P, 0, P + 1, 7, z0, z1, 1, 1, 2, edge=3)
    for a in (0, 4):
        if axis == "i":
            Box(S, 1, 1, a, P, a + 1, P + 1, 0, 13, 1, 2, 3, edge=3)
        else:
            Box(S, 1, 1, P, a, P + 1, a + 1, 0, 13, 1, 2, 3, edge=3)
    finish(name, S, "object", 1, 1, outline=False)


def dry_bush(name, seed=0):
    """Leafless dry shrub: forked dark twigs, a few clinging dead leaves, cracked pot-shaped base."""
    S = Spr()
    rnd = random.Random(seed)
    S.hline(-3, 3, -1, 2)
    S.hline(-1, 1, -2, 3)
    tips = [
        (-9, -14),
        (-5, -17),
        (0, -19),
        (5, -16),
        (9, -12),
        (-8, -8),
        (7, -7),
        (2, -13),
    ]
    for tx, ty in tips:
        S.line(0, -2, tx, ty, 3)
        mx, my = tx // 2, ty // 2 - 2
        S.line(mx, my, tx + (2 if tx > 0 else -2), ty - 3, 3)
    for _ in range(7):
        x, y = rnd.randrange(-8, 9), rnd.randrange(-17, -6)
        S.px(x, y, 2)
    finish(name, S, "object", 1, 1, outline=False)


def scree(name, seed=0):
    """A few loose rocks (1 tile) lying on the trail: three small stones of different sizes."""
    S = Spr()
    for cx, cy, rx, ry in ((-6, -3, 4, 3), (3, -2, 5, 3), (9, -5, 3, 2)):
        for y in range(-ry, ry + 1):
            for x in range(-rx, rx + 1):
                if (x / rx) ** 2 + (y / ry) ** 2 <= 1:
                    S.px(cx + x, cy + y, 1 if (x + y * 2) < -1 else 2)
        S.hline(cx - rx + 1, cx + rx - 1, cy + ry, 3)
    finish(name, S, "object", 1, 1)


def cairn(name, seed=0):
    """Trail marker: a small stack of three stones (tall, narrow, readable at a glance)."""
    S = Spr()
    for cx, cy, rx, ry in ((0, -3, 7, 3), (0, -8, 5, 3), (1, -12, 3, 2)):
        for y in range(-ry, ry + 1):
            for x in range(-rx, rx + 1):
                if (x / rx) ** 2 + (y / ry) ** 2 <= 1:
                    S.px(cx + x, cy + y, 1 if x * 0.5 + y < 0 else 2)
        S.hline(cx - rx + 1, cx + rx - 1, cy + ry, 3)
    finish(name, S, "object", 1, 1)


def cave_floor(name, kind, seed, base=1):
    """Cave rock floor: mid-light (level 1) with white flecks and hairline cracks (2), no big black."""
    rnd = random.Random(seed)
    bl = blob_field(rnd, 3, 7, 3)

    def f(x, y, u, v, r):
        if in_blobs(x, y, bl) and dither(x, y, 0.35):
            return 0 if base == 1 else 1
        return base

    S = gtile(f, seed)
    if kind == "cracks":
        for _ in range(3):
            x, y = rnd.randrange(4, 22), rnd.randrange(3, 11)
            for k in range(rnd.randrange(5, 10)):
                if in_diamond(x, y, 6):
                    S.t(x, y, 2)
                    if k % 4 == 3 and in_diamond(x, y + 1, 6):
                        S.t(x, y + 1, 3)
                x += 1 if rnd.random() < 0.7 else 0
                y += rnd.choice((-1, 0, 1))
    elif kind == "pebbles":
        for _ in range(5):
            x, y = rnd.randrange(4, 23), rnd.randrange(3, 12)
            if in_diamond(x + 2, y + 1, 5) and in_diamond(x - 1, y - 1, 5):
                S.t(x, y, 0)
                S.t(x + 1, y, 0)
                S.t(x, y + 1, 3)
                S.t(x + 1, y + 1, 3)
    elif kind == "damp":
        for _ in range(2):
            x, y = rnd.randrange(6, 20), rnd.randrange(5, 10)
            for k in range(4):
                if in_diamond(x + k, y + (k % 2), 5):
                    S.t(x + k, y + (k % 2), 0)
            if in_diamond(x + 1, y + 2, 5):
                S.t(x + 1, y + 2, 2)
                S.t(x + 2, y + 2, 2)
    else:
        marks(S, rnd, 6, 2, shapes=(((0, 0),), ((0, 0), (1, 0))))
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def cave_wall(name, seed, phase=0, veined=True, h=34):
    """Cave rock wall block: gray faces with dithered light patches and a few crevices, white crystal veins glowing (2 phases)."""
    S = Spr()
    rnd = random.Random(seed)
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, h, 1, 2, 3, edge=3)
    for face, hi, cov in ((b.J, 1, 0.3), (b.I, 3, 0.3)):
        face.rect(0, face.wid, 0, h, 2)
        for _ in range(3):
            rock_dither_face(face, rnd, h, 2, hi, cov)
        for _ in range(2):  # crevices
            s, z = rnd.randrange(2, face.wid - 2), rnd.randrange(2, h - 8)
            for k in range(rnd.randrange(4, 7)):
                face.px(s + (k // 3), z + k, 3 if face is b.J else 1)
    for _ in range(4):
        b.top_px(rnd.randrange(2, 13), rnd.randrange(2, 13), 2)
    if veined:
        vein(b.J, rnd, phase, h, rnd.randrange(5, b.J.wid - 5))
        vein(b.I, rnd, phase, h, rnd.randrange(5, b.I.wid - 5))
    finish(name, S, "object", 1, 1, outline=False)


def stalagmite(name, h=22, seed=0):
    """Stalagmite: a pointed cone of light stone (lit left, shaded right) with a dark foot; h 14 = small, 22 = tall."""
    S = Spr()
    rnd = random.Random(seed)
    r = 6 if h < 18 else 8
    for y in range(-h, 1):
        t = -y / h
        w = int(round(r * (1 - t) ** 0.85))
        for x in range(-w, w + 1):
            lv = 1 if x < w * 0.1 else 2
            if x < -w * 0.4 and dither(x, y, 0.5):
                lv = 0
            S.px(x, y, lv)
    for _ in range(3):
        y = -rnd.randrange(3, h - 3)
        S.px(rnd.randrange(0, 2), y, 3)
        S.px(rnd.randrange(0, 2) + 1, y + 1, 3)
    S.hline(-r, r, 0, 3)
    S.hline(-r + 1, r - 1, -1, 3)
    finish(name, S, "object", 1, 1)


def crystal_cluster(name, phase=0, h=18):
    """Cave crystals growing from the floor: three hexagonal shards (white lit face, light-gray shaded face), a glint that blinks."""
    S = Spr()
    shards = [(-8, -2, 3, int(h * 0.6)), (8, -1, 3, int(h * 0.7)), (0, 0, 4, h)]
    for cx, by, w, hh in shards:
        sh = hh - 5
        pts = [
            (cx - w, by),
            (cx - w, by - sh),
            (cx, by - hh),
            (cx + w, by - sh),
            (cx + w, by),
        ]
        S.poly([(int(x), int(y)) for x, y in pts], 0)
        S.poly(
            [(cx + 1, by), (cx + 1, by - hh + 1), (cx + w, by - sh), (cx + w, by)], 1
        )
        for y in range(by - sh + 2, by):  # facet line
            S.px(cx, y, 2)
        S.hline(cx - w + 1, cx + w - 1, by - 1, 2)
    S.outline(3)
    gx, gy = ((-1, -h + 5), (3, -h + 9))[phase % 2]
    for dx, dy in ((0, 0), (-1, 0), (1, 0), (0, -1), (0, 1)):
        S.px(gx + dx, gy + dy, 0 if (dx, dy) != (0, 0) else 3)
    finish(name, S, "object", 1, 1, outline=False)


def rope_ladder(name):
    """Stairs between floors: a round hole in the floor (light lip, black inside) with a rope ladder hanging down the back wall,
    tied to two wooden stakes."""
    S = Spr()
    for y in range(15):
        for x in range(28):
            if not in_diamond(x, y, 7):
                continue
            u, v = tile_uv(x, y)
            d = max(abs(u - 0.5), abs(v - 0.5))
            if d > 0.46:
                tpx(S, x, y, 3)
            elif d > 0.36:
                tpx(S, x, y, 0)
            elif d > 0.30:
                tpx(S, x, y, 2)
            else:
                lv = 3
                if (u < 0.40 or v < 0.40) and dither(x, y, 0.5):
                    lv = 2  # the far inner wall catches a little light
                tpx(S, x, y, lv)
    for rx in (-4, 4):  # the two ropes
        for ry in range(-24, -3):
            S.px(rx, ry, 0)
    for ry in range(-20, -3, 3):  # the rungs
        S.hline(-3, 3, ry, 0)
        S.hline(-3, 3, ry + 1, 1)
    for rx in (-6, 6):  # stakes
        for ry in range(-24, -15):
            S.px(rx, ry, 2)
            S.px(rx + 1, ry, 3)
        S.hline(rx - 1, rx + 2, -25, 3)
    S.hline(-6, 6, -22, 3)  # rope tie
    finish(name, S, "object", 1, 1, outline=False)


def crate_stack(name, seed=0):
    """A big crate with a smaller one on top and a 'V' stencil on the front face (the Vertice crates)."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, 12, 1, 0, 1, edge=3)
    for face in (b.J, b.I):
        face.hline(0, face.wid, 2, 2)
        face.hline(0, face.wid, 9, 2)
        face.vline(1, 0, 12, 2)
        face.vline(face.wid - 2, 0, 12, 2)
    for k in range(8):
        b.J.px(3 + k, 3 + k, 2)
    for k in range(5):  # the V
        b.I.px(5 + k, 9 - k, 3)
        b.I.px(6 + k, 9 - k, 3)
        b.I.px(13 - k, 9 - k, 3)
        b.I.px(12 - k, 9 - k, 3)
    t = Box(S, 1, 1, 1, 2, 5, 6, 12, 19, 1, 0, 1, edge=3)
    t.J.hline(0, t.J.wid, 3, 2)
    t.I.hline(0, t.I.wid, 3, 2)
    finish(name, S, "object", 1, 1, outline=False)


def dig_tools(name):
    """Excavation corner: a mound of loose dirt with a shovel stuck in it and a pickaxe leaning on it (head on the right)."""
    S = Spr()
    rnd = random.Random(4)
    lump(S, -2, -4, 10, 5, rnd, tone=2, cracks=0)
    lump(S, 6, -3, 6, 3, rnd, tone=2, cracks=0)
    for k in range(20):  # shovel: shaft up from the mound, T-grip, blade in the dirt
        S.px(-4 + k // 7, -6 - k, 3)
        S.px(-3 + k // 7, -6 - k, 2)
    S.hline(-7, -2, -27, 3)
    S.hline(-7, -2, -28, 0)
    for k in range(18):  # pickaxe: handle leaning right, double-pointed head
        S.px(4 + k // 2, -3 - k, 3)
        S.px(5 + k // 2, -3 - k, 2)
    for k in range(-5, 6):
        S.px(13 + k, -23 + abs(k) // 2 + 1, 3)
        S.px(13 + k, -24 + abs(k) // 2 + 1, 0 if abs(k) < 4 else 3)
    finish(name, S, "object", 1, 1, outline=False)


def fossil_dig(name):
    """Dig site: a shallow trench of dark loose soil whose floor shows an ammonite fossil (white spiral) and a rib."""

    def f(x, y, u, v, r):
        d = max(abs(u - 0.5), abs(v - 0.5))
        if d > 0.44:
            return 3
        if d > 0.36:
            return 0 if dither(x, y, 0.6) else 1  # spoil heap rim
        dx, dy = (x - 14) / 2.0, (y - 7)
        rr = math.hypot(dx, dy)
        ang = math.atan2(dy, dx)
        if rr < 5.6:
            ph = (rr + ang * 1.0 / math.pi * 1.0) % 2.0
            return 0 if ph < 0.9 else 2
        if abs(u - 0.5) < 0.06 and 0.5 < v < 0.86 and int(v * 20) % 2 == 0:
            return 0  # a rib / vertebrae line
        return 2 if dither(x, y, 0.75) else 3

    S = gtile(f, 0)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def v_flag(name, phase=0):
    """Vertice banner on a stake: black pennant with a white V, waving (2 phases)."""
    S = Spr()
    for y in range(-34, 0):
        S.px(0, y, 3)
        S.px(1, y, 2)
    S.hline(-2, 3, -35, 3)
    off = (0, 1)[phase % 2]
    for k in range(13):
        top = -33 + (k // 4) * (1 if phase else 0)
        for y in range(top, top + 14 - k // 2):
            S.px(2 + k, y + (off if k > 6 else 0), 3)
    for k in range(4):  # white V
        S.px(4 + k, -30 + k, 0)
        S.px(11 - k, -30 + k, 0)
    finish(name, S, "object", 1, 1, outline=False)


def sand(name, kind, seed):
    """Beach sand: white with soft dune ripples (light-gray dashes), a few dark grains / a shell."""
    rnd = random.Random(seed)

    def f(x, y, u, v, r):
        w = (v * 5 + 0.25 * math.sin(u * 6.3 + seed)) % 1.0
        if w < 0.08 and (int(u * 12) + seed) % 3:
            return 1
        return 0

    S = gtile(f, seed)
    marks(S, rnd, 5, 2, shapes=(((0, 0),),))
    if kind == "shell":
        x, y = rnd.randrange(9, 18), rnd.randrange(5, 9)
        for dx, dy, lv in (
            (0, 0, 3),
            (1, 0, 3),
            (2, 0, 3),
            (0, -1, 1),
            (1, -1, 0),
            (2, -1, 1),
            (1, -2, 3),
        ):
            S.t(x + dx, y + dy, lv)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def sea_tile(name, phase, edges):
    """Open sea: level-2 water (as the ponds) lightened by dithered gray patches, white wave arcs that drift with `phase` (0..3); on the sides
    facing sand a band of foam (white dashes that creep with the phase) and a light swash, so the shore reads as a beach.
    """
    rnd = random.Random(3)
    bl = blob_field(rnd, 3, 7, 3)
    S = tile_canvas()
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y, 7):
                S.t(x, y, 2)
    for r, y in enumerate((3, 6, 9, 12)):
        for x0 in range(-9, 32, 10):
            x = x0 + (5 if r % 2 else 0) + phase
            for k, dy in ((0, 0), (1, -1), (2, -1), (3, 0)):
                if in_diamond(x + k, y + dy, 6):
                    S.t(x + k, y + dy, 0)
    for y in range(15):
        for x in range(28):
            if not in_diamond(x, y, 7):
                continue
            u, v = tile_uv(x, y)
            d = min(
                [{"tl": u, "tr": v, "br": 1 - u, "bl": 1 - v}[e] for e in edges]
                or [1.0]
            )
            front = 0.12 + 0.02 * (phase % 4)
            if d < front - 0.04:
                S.t(x, y, 0 if (x + phase) % 4 else 1)  # foam / wet sand
            elif d < front:
                S.t(x, y, 0 if (x + y + phase) % 2 else 1)  # the breaking edge
            elif d < front + 0.10:
                S.t(x, y, 1)  # shallows
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def beach_grass(name, seed=0):
    """Dune grass: a tuft of long curved blades bending in the sea wind (two tones), ~14 px."""
    S = Spr()
    rnd = random.Random(seed)
    for k, (tx, ty) in enumerate(
        ((-7, -13), (-3, -16), (1, -17), (5, -14), (8, -10), (-9, -8))
    ):
        bend = 1 if tx > 0 else -1
        pts = [(0 + (k % 2), -1), (tx // 2, ty // 2 - 1), (tx + bend, ty)]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            S.line(x0, y0, x1, y1, 3 if k % 2 else 2)
    S.hline(-2, 3, 0, 2)
    finish(name, S, "object", 1, 1, outline=False)


def sea_rock(name, seed=0):
    """A rock standing in the shallows: two lumps with a ring of white foam at the waterline."""
    S = Spr()
    rnd = random.Random(seed)
    for x in range(-13, 14):  # foam ring on the water (flat ellipse)
        for y in range(-9, 3):
            d = (x / 13.0) ** 2 + ((y + 4) / 5.0) ** 2
            if 0.62 < d <= 1.0 and dither(x, y, 0.6):
                S.px(x, y - 3, 0)
    lump(S, -3, -9, 8, 6, rnd, tone=2, cracks=1)
    lump(S, 5, -7, 6, 5, rnd, tone=2, cracks=1)
    finish(name, S, "object", 1, 1, outline=False)


def sailboat(name, phase=0):
    """Small sailing boat out at sea: hull, mast, a white triangular sail; 2 phases bob it by 1 px."""
    S = Spr()
    dy = phase % 2
    S.poly([(-9, -4 + dy), (9, -4 + dy), (6, -1 + dy), (-6, -1 + dy)], 2)
    S.hline(-9, 9, -4 + dy, 3)
    S.hline(-6, 6, -1 + dy, 3)
    for y in range(-20, -4):
        S.px(0, y + dy, 3)
    S.poly([(2, -19 + dy), (2, -6 + dy), (10, -6 + dy)], 0)
    S.line(2, -19 + dy, 10, -6 + dy, 3)
    S.line(2, -6 + dy, 10, -6 + dy, 3)
    S.poly([(-1, -17 + dy), (-1, -7 + dy), (-6, -7 + dy)], 1)
    S.line(-1, -17 + dy, -6, -7 + dy, 3)
    for x in range(-12, 13, 4):  # wake ripples
        S.px(x + phase * 2, 0, 0)
    finish(name, S, "object", 1, 1, outline=False)


def cliff_grass(name, seed=0, h=22):
    """Seaside cliff block (Ruta 4): grassy top with dark tufts, faceted rock faces with strata."""
    S = Spr()
    rnd = random.Random(seed)
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, h, 0, 1, 2, edge=3)
    for face, ln in ((b.J, 2), (b.I, 3)):
        strata(face, rnd, h, ln)
        for _ in range(3):
            s, z0 = rnd.randrange(1, face.wid - 1), rnd.randrange(2, h - 6)
            for k in range(4):
                face.px(s, z0 + k, 3)
    for _ in range(7):
        b.top_px(rnd.randrange(2, 13), rnd.randrange(2, 13), 3)
    for _ in range(3):
        a2, b2 = rnd.randrange(3, 12), rnd.randrange(3, 12)
        b.top_px(a2, b2, 3)
        b.top_px(a2 + 1, b2 - 1, 3)
        b.top_px(a2 - 1, b2 + 1, 3)
    finish(name, S, "object", 1, 1, outline=False)


def deck_tile(name, axis, edges=(), seed=0, wet=False):
    """Wooden boardwalk tile: planks along `axis` (i or j) as 2:1 seam lines every 4 px (like floor_wood but whole-tile), staggered butt
    joints and nail dots; the sides facing water get the plank thickness (3 px dark band on the near edges, a thin lip on the far ones).
    """
    rnd = random.Random(seed)
    S = tile_canvas()
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y, 7):
                S.t(x, y, 0)

    def sy(c, x):
        return c + (x + 1) // 2 if axis == "i" else c + (28 - x) // 2

    for n, c in enumerate(range(-24, 14, 4) if axis == "i" else range(-12, 26, 4)):
        for x in range(28):
            y = sy(c, x)
            if in_diamond(x, y, 7) and in_diamond(x, y + 1, 7):
                S.t(x, y, 2)
        bx = (n * 7 + seed * 5) % 22 + 3  # one staggered butt joint per plank
        for dy in (1, 2):
            if in_diamond(bx, sy(c, bx) + dy, 6):
                S.t(bx, sy(c, bx) + dy, 1)
        if wet and rnd.random() < 0.5:
            wx, wy = rnd.randrange(5, 22), rnd.randrange(3, 12)
            for k in range(3):
                if in_diamond(wx + k, wy, 6):
                    S.t(wx + k, wy, 1)
    for y in range(15):
        for x in range(28):
            if not in_diamond(x, y, 7):
                continue
            u, v = tile_uv(x, y)
            if "tl" in edges and u < 0.1:
                S.t(x, y, 3 if u < 0.05 else 1)
            if "tr" in edges and v < 0.1:
                S.t(x, y, 3 if v < 0.05 else 1)
            if "br" in edges and u > 0.88:
                S.t(x, y, 3 if u > 0.95 else 2)
            if "bl" in edges and v > 0.88:
                S.t(x, y, 3 if v > 0.95 else 2)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def wet_stone(name, seed=0):
    """Harbour cobbles: rounded stones (jittered Voronoi, ~3 x 3 per tile) with light-gray joints, a few stones tinted, puddle glints."""
    rnd = random.Random(seed)
    pts = [
        (
            (i + 0.5 + rnd.uniform(-0.28, 0.28)) / 3.0,
            (j + 0.5 + rnd.uniform(-0.28, 0.28)) / 3.0,
        )
        for i in range(-1, 4)
        for j in range(-1, 4)
    ]
    tint = {k for k in range(len(pts)) if rnd.random() < 0.22}

    def f(x, y, u, v, r):
        d = sorted(
            ((((u - pu) * 28 / 2) ** 2 + ((v - pv) * 14 * 1.0) ** 2) ** 0.5, k)
            for k, (pu, pv) in enumerate(pts)
        )
        d1, k1 = d[0]
        d2 = d[1][0]
        if d2 - d1 < 1.5:
            return 1
        if k1 in tint and dither(x, y, 0.45):
            return 1
        return 0

    S = gtile(f, seed)
    for _ in range(2):  # puddle glints: a dark sliver with a white highlight
        x, y = rnd.randrange(7, 19), rnd.randrange(5, 10)
        if in_diamond(x + 3, y + 1, 6):
            for k in range(4):
                S.t(x + k, y, 2)
            S.t(x + 1, y + 1, 1)
            S.t(x + 2, y + 1, 1)
            S.t(x + 1, y - 1, 0)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def bridge_rails(name, axis):
    """Both hand-rails of a plank bridge tile (decor drawn over the deck): posts + 2 rails on the back side (tall) and a low front rail."""
    S = Spr()
    for c0, hgt, zs in ((0, 15, ((9, 11), (4, 6))), (6, 10, ((6, 8),))):
        for z0, z1 in zs:
            if axis == "i":
                Box(S, 1, 1, 0, c0, 7, c0 + 1, z0, z1, 1, 1, 2, edge=3)
            else:
                Box(S, 1, 1, c0, 0, c0 + 1, 7, z0, z1, 1, 1, 2, edge=3)
        for a in (0, 6):
            if axis == "i":
                Box(S, 1, 1, a, c0, a + 1, c0 + 1, 0, hgt, 1, 2, 3, edge=3)
            else:
                Box(S, 1, 1, c0, a, c0 + 1, a + 1, 0, hgt, 1, 2, 3, edge=3)
    finish(name, S, "object", 1, 1, outline=False)


def hull_boat(name, phase=0, flip=False):
    """Moored fishing boat along +i (2 x 1): a hull with a real sheer line (bow and stern rise, keel curves up), planked outer face,
    the far wall's inside, a dark deck, a small cabin, a mast with a pennant; phase 1 bobs it 1 px.
    """
    S = Spr()
    dy = phase % 2
    tmp = Spr()
    Fn = Box(tmp, 2, 1, 0, 1, 14, 6, 0, 1, 0, 0, 1, edge=None).J  # near wall (b = 6)
    Ff = Box(tmp, 2, 1, 0, 0, 14, 2, 0, 1, 0, 0, 1, edge=None).J  # far wall (b = 2)
    Fn.S = Ff.S = S
    W = Fn.wid

    def prof(sx):
        t = (sx / (W - 1.0)) * 2 - 1
        return int(round(4.5 * t**4)), 9 + int(round(3 * t * t)) - (
            0 if t < 0 else 0
        )  # keel lift, gunwale height

    def col(F, sx, z0, z1, lv):
        for z in range(z0, z1 + 1):
            x, y = F.at(sx, z)
            S.px(x, y + dy, lv)

    Xn = Fn.X0
    Xf = Ff.X0
    for x in range(min(Xn, Xf), max(Xn, Xf) + W + 1):
        sxf, sxn = x - Xf, x - Xn
        if 0 <= sxf < W:  # inside of the far wall
            lo, hi = prof(sxf)
            col(Ff, sxf, 3, hi, 1)
            if sxf % 5 == 0:
                col(Ff, sxf, 3, hi, 2)
            col(Ff, sxf, hi, hi, 3)
        if 0 <= sxf < W and 0 <= sxn < W:  # the deck between the walls
            yf = Ff.at(sxf, 3)[1]
            yn = Fn.at(sxn, prof(sxn)[1])[1]
            for y in range(yf + 1, yn):
                S.px(x, y + dy, 3 if (x + y) % 5 else 2)
    for sx in range(W):  # near wall: outer planks
        lo, hi = prof(sx)
        col(Fn, sx, lo, hi, 0)
        for z in (3, 6):
            if lo <= z < hi:
                col(Fn, sx, z, z, 2)
        col(Fn, sx, hi, hi, 3)
        col(Fn, sx, lo, lo, 3)
    for z in range(2, 11):
        col(Fn, 0, z - 1, z, 3)  # stern post
        col(Fn, W - 1, z - 1, z, 3)  # bow post
    c = Box(S, 2, 1, 3, 2, 8, 5, 10, 21, 0, 0, 1, edge=3)  # cabin (rides on the deck)
    c.J.window(1, 7, 4, 9, glass=1, frame=3, cross=False)
    c.I.window(2, 7, 4, 9, glass=1, frame=3, cross=False)
    for y in range(-44, -20):  # mast
        S.px(-2, y + dy, 3)
        S.px(-1, y + dy, 2)
    for k in range(10):  # pennant
        S.px(0 + k, -44 + dy + (k // 3) + (dy if k > 5 else 0), 3)
        S.px(0 + k, -43 + dy + (k // 3) + (dy if k > 5 else 0), 0)
        S.px(0 + k, -42 + dy + (k // 3) + (dy if k > 5 else 0), 3 if k < 5 else 0)
    if flip:  # the same boat along +j (the iso grid is symmetric: fw <-> fh)
        S = flip_spr(S)
    finish(name, S, "object", 1 if flip else 2, 2 if flip else 1, outline=False)


def harbor_lamp(name, phase=0):
    """Harbour lamp: iron post on a stone foot, a curved arm and a lantern hanging from it, with sparkles that flicker."""
    S = Spr()
    S.rect(-3, -3, 4, -1, 2)
    S.hline(-4, 5, -4, 3)
    S.hline(-4, 5, 0, 3)
    for y in range(-30, -3):
        S.px(0, y, 3)
        S.px(1, y, 2)
    for k, (dx, dy) in enumerate(
        ((2, -31), (3, -32), (4, -32), (5, -32), (6, -31), (7, -30))
    ):
        S.px(dx, dy, 3)
    for y in range(-30, -24):
        S.px(7, y, 3)
    for y in range(-25, -18):  # lantern body
        for x in range(4, 11):
            S.px(x, y, 3 if x in (4, 10) or y in (-25, -19) else 0)
    S.hline(5, 9, -26, 3)
    S.px(7, -22, 1)
    if phase % 2:
        for dx, dy in ((7, -28), (12, -22), (2, -22), (11, -26), (3, -26)):
            S.px(dx, dy, 3)
    else:
        for dx, dy in ((7, -28), (12, -21), (2, -23)):
            S.px(dx, dy, 3)
    finish(name, S, "object", 1, 1, outline=False)


def net_rack(name, axis_i=True):
    """Fishing net hung to dry on two poles (2 x 1, along +i): a diamond mesh with a scalloped hem and white cork floats."""
    S = Spr()
    for a in (0, 13):
        Box(S, 2, 1, a, 3, a + 1, 4, 0, 27, 1, 2, 3, edge=3)
    Box(S, 2, 1, -1, 3, 15, 4, 24, 27, 1, 1, 2, edge=3)
    tmp = Spr()
    bx = Box(tmp, 2, 1, 1, 3, 14, 4, 6, 24, 0, 0, 1, edge=None)
    F = bx.J
    F.S = S
    for sx in range(2, F.wid - 2):
        hem = 8 + int(3 * abs(math.sin(sx * 0.45)))
        for z in range(hem, 24):
            if (sx + z) % 4 == 0 or (sx - z) % 4 == 0:
                F.px(sx, z, 3)
        F.px(sx, hem, 3)
    for sx in range(4, F.wid - 3, 5):  # cork floats along the top pole
        F.px(sx, 22, 0)
        F.px(sx + 1, 22, 0)
        F.px(sx, 21, 1)
        F.px(sx + 1, 21, 1)
    finish(name, S, "object", 2, 1, outline=False)


def anchor_monument(name):
    """The harbour's big iron anchor standing on a stone plinth: ring, shank, stock bar, curved arms with flukes."""
    S = Spr()
    Box(S, 1, 1, 1, 1, 6, 6, 0, 5, 1, 1, 2, edge=3)
    for y in range(-30, -5):
        S.px(0, y, 3)
        S.px(1, y, 3)
        S.px(2, y, 2)
    S.hline(-7, 9, -23, 3)
    S.hline(-7, 9, -22, 2)
    for yy in (-23, -22):
        S.px(-8, yy, 3)
        S.px(10, yy, 3)
    for dx in (-1, 1):  # arms: arcs from the shank foot to the flukes
        pts = [
            (1, -6),
            (1 + dx * 4, -6),
            (1 + dx * 8, -9),
            (1 + dx * 10, -13),
            (1 + dx * 11, -15),
        ]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            S.line(x0, y0, x1, y1, 3)
            S.line(x0, y0 + 1, x1, y1 + 1, 3)
        fx, fy = 1 + dx * 11, -15
        S.poly([(fx, fy), (fx - dx * 4, fy - 1), (fx, fy - 6)], 3)
    for r in range(3, 5):
        S.disc(1, -33, r, 3)
    S.disc(1, -33, 2, 0)
    finish(name, S, "object", 1, 1, outline=False)


def lobster_trap(name):
    """Wicker trap: a half-round cage of dark slats with a funnel mouth (stacked pair)."""
    S = Spr()
    for z0, z1 in ((0, 9), (9, 17)):
        b = Box(S, 1, 1, 1, 2, 6, 6, z0, z1, 1, 0, 1, edge=3)
        for face in (b.J, b.I):
            for sx in range(2, face.wid - 1, 3):
                face.vline(sx, z0 + 1, z1 - 1, 3)
            face.hline(0, face.wid, z0 + (z1 - z0) // 2, 3)
    finish(name, S, "object", 1, 1, outline=False)


def pool_floor(name, seed=0):
    """Gym deck: white tiles, a 3x3 grid of light-gray joints per tile and a drip highlight."""

    def f(x, y, u, v, r):
        if (u * 3) % 1.0 < 0.07 or (v * 3) % 1.0 < 0.1:
            return 1
        return 0

    S = gtile(f, seed)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def walk_tile(name, edges=(), seed=0):
    """Walkway slab over the pool: white non-slip slab with an inset gray border (a raised plate) and the thickness at the water sides."""

    def f(x, y, u, v, r):
        d = max(abs(u - 0.5), abs(v - 0.5))
        if 0.40 < d < 0.46:
            return 1
        if d < 0.40 and dither(x, y, 0.07):
            return 1
        return 0

    S = gtile(f, seed)
    for y in range(15):
        for x in range(28):
            if not in_diamond(x, y, 7):
                continue
            u, v = tile_uv(x, y)
            if "tl" in edges and u < 0.1:
                S.t(x, y, 3 if u < 0.05 else 1)
            if "tr" in edges and v < 0.1:
                S.t(x, y, 3 if v < 0.05 else 1)
            if "br" in edges and u > 0.88:
                S.t(x, y, 3 if u > 0.95 else 2)
            if "bl" in edges and v > 0.88:
                S.t(x, y, 3 if v > 0.95 else 2)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def gym_wall(name, axis):
    """Pool-hall wall: white with a wave band (light gray) low on the face, tile joints below it and a water-drop emblem higher up."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 0, 0, 1, edge=3)
    for F, lvb, lvd in ((b.J, 1, 2), (b.I, 2, 3)):
        for sx in range(F.wid):
            crest = 9 + int(round(1.6 * math.sin(sx * 0.7)))
            for z in range(5, crest):
                F.px(sx, z, lvb)
            F.px(sx, crest, lvd)
        F.rect(0, F.wid, 0, 5, lvb)
        for sx in range(0, F.wid, 5):
            F.vline(sx, 0, 5, lvd)
        F.hline(0, F.wid, WALL_H - 6, lvb)
    if axis == "j":
        for z in range(15, 27):  # the water-drop emblem
            w = (
                (z - 15) * 5 // 12
                if z < 22
                else int(5 * math.sqrt(max(0, 1 - ((z - 22) / 5.0) ** 2)))
            )
            for sx in range(14 - w, 14 + w + 1):
                b.J.px(sx, z, 2)
        b.J.px(12, 22, 0)
        b.J.px(12, 21, 0)
    finish(name, S, "object", 1, 1, outline=False)


def diving_board(name):
    """Diving board (2 x 1 along +i): a ladder tower at the deck end and a long springboard sticking out over the water, tip kicked up,
    white with dark grip stripes."""
    S = Spr()
    t = Box(S, 2, 1, 0, 1, 4, 6, 0, 18, 0, 1, 2, edge=3)
    for z in range(2, 18, 3):  # ladder rungs on the front face
        t.J.hline(1, t.J.wid - 1, z, 3)
    for a0, a1, z0 in (
        (3, 8, 18),
        (8, 12, 19),
        (12, 15, 20),
    ):  # the board rises toward its tip
        b = Box(S, 2, 1, a0, 2, a1, 5, z0, z0 + 2, 0, 1, 2, edge=3)
        for a2 in range(2 * a0 + 1, 2 * a1, 4):
            b.top_px(a2, 7, 2)
            b.top_px(a2 + 1, 7, 2)
    Box(S, 2, 1, 5, 2, 7, 5, 14, 18, 1, 1, 2, edge=3)  # fulcrum under the board
    finish(name, S, "object", 2, 1, outline=False)


def buoy(name, phase=0):
    """Lane buoy: a striped ball floating on the water with a ripple ring (2 phases: bobs)."""
    S = Spr()
    dy = phase % 2
    for x in range(-10, 11):  # ripple ring
        for y in range(-4, 3):
            d = (x / 10.0) ** 2 + (y / 3.0) ** 2
            if 0.7 < d <= 1.0 and (x + phase) % 3:
                S.px(x, y - 1, 0)
    for y in range(-11, 0):
        for x in range(-6, 7):
            if (x / 6.0) ** 2 + ((y + 6 + dy) / 5.5) ** 2 <= 1:
                band = ((y + dy) // 3) % 2
                S.px(x, y + dy, 3 if band else 0)
    S.px(-3, -9 + dy, 0)
    S.line(0, -12 + dy, 0, -16 + dy, 3)
    S.disc(0, -17 + dy, 1, 3)
    finish(name, S, "object", 1, 1, outline=False)


def lane_rope(name, axis="i"):
    """Swim-lane rope: floats (every 4 px, alternating dark/white) on a line across a water tile, along +i (or +j)."""
    S = Spr()
    S.line(-13, -13, 12, 0, 3)
    for k in (
        -1,
        0,
        1,
    ):  # 3 floats per tile (a tile is 14 px along +i): stays inside its tile
        cx = k * 9
        cy = -7 + cx // 2 - 1
        S.disc(cx, cy, 2, 3 if k % 2 else 0, ring=3)
    if axis != "i":
        S = flip_spr(S)
    finish(name, S, "object", 1, 1, outline=False)


def lifeguard_chair(name):
    """Lifeguard chair: a tall white seat on four legs with a ladder and a parasol board."""
    S = Spr()
    for a, b in ((1, 1), (6, 1), (1, 6), (6, 6)):
        Box(S, 1, 1, a - 1, b - 1, a, b, 0, 22, 0, 1, 2, edge=3)
    Box(S, 1, 1, 0, 0, 7, 7, 22, 25, 0, 1, 2, edge=3)
    Box(S, 1, 1, 0, 0, 7, 1, 25, 36, 0, 1, 2, edge=3)
    finish(name, S, "object", 1, 1, outline=False)


def cottage_antenna(name, phase=0):
    """The PC inventor's cottage (2 x 2): board-and-batten walls, a door and two windows, a slate roof, a chimney, a solar plate, and a
    radio mast on the roof with a Yagi antenna, a dish and a blinking beacon (2 phases).
    """
    fw = fh = 2
    S = Spr()
    WH = 24
    b = Box(S, fw, fh, 0, 0, 14, 14, 0, WH, 1, 0, 1, edge=3)
    J, I = b.J, b.I
    for face, lv in ((J, 2), (I, 2)):
        for sx in range(3, face.wid, 4):  # battens
            face.vline(sx, 0, WH, lv)
    J.rect(0, J.wid, 0, 3, 2)
    I.rect(0, I.wid, 0, 3, 3)
    J.rect(17, 25, 0, 16, 3)  # door
    J.rect(18, 24, 0, 15, 1)
    J.px(23, 7, 3)
    J.window(4, 12, 10, 20, glass=1)
    I.window(6, 14, 10, 20, glass=0)
    I.window(18, 26, 10, 20, glass=0)
    r = Box(S, fw, fh, -1, -1, 15, 15, WH, WH + 3, 1, 2, 3, edge=3)
    for a in range(0, 16, 2):
        r.top_line_b(a, -1, 15, 3, dash=(4, 3))
    Box(S, fw, fh, 2, 9, 5, 12, WH + 3, WH + 10, 1, 2, 3, edge=3)  # chimney
    ox = 6  # mast stands on the roof, behind the middle
    MH = 31
    for y in range(-WH - MH, -WH - 3):
        S.px(ox, y - 0, 3)
        S.px(ox + 1, y, 2)
    my = -WH - MH + 2
    for k, (yy, hw) in enumerate(
        ((my + 0, 10), (my + 5, 8), (my + 10, 6), (my + 15, 4))
    ):  # Yagi elements
        S.hline(ox - hw, ox + hw + 1, yy, 3)
        S.px(ox - hw, yy - 1, 3)
        S.px(ox + hw + 1, yy - 1, 3)
    S.line(ox + 1, -WH - 18, ox + 12, -WH - 8, 3)  # guy wire to the eave
    for rr in range(1, 4):  # small dish on the mast
        S.hline(ox + 2 + rr // 2, ox + 7 - rr // 2, -WH - 12 - rr, 2 if rr % 2 else 3)
    S.line(ox + 4, -WH - 9, ox + 4, -WH - 13, 3)
    if phase % 2:  # beacon on the tip
        for dx, dy in ((-1, 0), (0, -1), (1, 0), (0, 1), (0, 0)):
            S.px(ox + dx, my - 3 + dy, 0 if (dx, dy) != (0, 0) else 3)
        S.px(ox - 2, my - 3, 0)
        S.px(ox + 3, my - 3, 0)
    finish(name, S, "object", fw, fh, outline=False)


def canal_tile(name, phase, edges, base=2):
    """Canal / pool water: water's look (level 2, white ripples, bank on the land sides) with half the ripples so big areas stay calm;
    base=1 gives the lighter pool water (dark ripples added)."""
    S = tile_canvas()
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y, 7):
                S.t(x, y, base)
    for r, y in enumerate((5, 11)):
        for x0 in range(-4, 30, 14):
            x = x0 + (6 if r % 2 else 0) + phase
            for k, dy in ((0, 0), (1, -1), (2, -1), (3, 0)):
                if in_diamond(x + k, y + dy, 6):
                    S.t(x + k, y + dy, 0)
                    if base == 1 and in_diamond(x + k, y + dy + 2, 6):
                        S.t(x + k, y + dy + 2, 2)
    for y in range(15):
        for x in range(28):
            if not in_diamond(x, y, 7):
                continue
            u, v = tile_uv(x, y)
            if "tl" in edges and u < 0.16:
                S.t(x, y, 3 if u < 0.07 else 1)
            if "tr" in edges and v < 0.16:
                S.t(x, y, 3 if v < 0.07 else 1)
            if "br" in edges and u > 0.95:
                S.t(x, y, 3)
            if "bl" in edges and v > 0.95:
                S.t(x, y, 3)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


# ---- the two fossils of the Monte Eco dig (actors with 1 direction on the dig tiles; the player picks one)
FOSSIL_ART = {
    "fossil_tri": """
....########....
..##wwwwwwww##..
.#wwwwwwwwwwww#.
.#wwd#wwww#dww#.
.#wwwwwwwwwwww#.
..#llllllllll#..
..#wwwwwwwwww#..
..#llllllllll#..
...#wwwwwwww#...
...#llllllll#...
....#wwwwww#....
.....######.....
""",
    "fossil_lily": """
...#.#.#.#.#...
..#w#w#w#w#w#..
..#www#w#www#..
...#wwwwwww#...
....#wwwww#....
.....#www#.....
.....#ldl#.....
......#w#......
.....#ldl#.....
......#w#......
.....#ldl#.....
....#######....
""",
}


def fossil_actor(name):
    import lvlib

    rows = [r for r in FOSSIL_ART[name].strip("\n").split("\n")]
    w = max(len(r) for r in rows)
    lv, mk = ascii_levels("\n".join(r.ljust(w, ".") for r in rows))
    lvlib.write_lv_png(
        os.path.join(ACTORS_DIR, name, "south", "f0.lv.png"),
        lv,
        mk,
        w // 2,
        len(rows) - 3,
    )


# ---- new tiles of the real maps (not in the mock): the encounter tiles of the dry trail and of the cave, and the ledge pairs
def scrub_tile(name, seed=0):
    """Ruta 3's tall grass (C_GRASS): the trail's packed earth with thin dry blades in dense clumps (dark stems, light tips)."""
    rnd = random.Random(seed)
    base = spr_of_tile("earth_b")
    S = tile_canvas()
    for y in range(15):
        for x in range(28):
            if base.mk[y][x]:
                S.t(x, y, base.lv[y][x])
    tufts = [
        (x, y)
        for r, y in enumerate((4, 8, 12))
        for x in range(3 + (4 if r % 2 else 0), 26, 8)
    ]
    for x, y in tufts:
        pts = []
        for dx, dy, lv in (
            (0, 0, 3),
            (-1, 0, 3),
            (1, 0, 3),
            (-1, -1, 3),
            (1, -1, 3),
            (0, -1, 3),
            (-2, -2, 3),
            (2, -2, 3),
            (0, -2, 3),
            (-3, -3, 1),
            (3, -3, 1),
            (0, -3, 3),
            (-1, -3, 1),
            (1, -3, 1),
            (0, -4, 1),
        ):
            if in_diamond(x + dx, y + dy, 7) and in_diamond(x + dx, y + dy + 1, 7):
                pts.append((x + dx, y + dy, lv))
        for px_, py_, lv in sorted(pts, key=lambda t: -t[1]):
            S.t(px_, py_, lv)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def moss_tile(name, seed=0):
    """Monte Eco's tall grass (C_GRASS; a cave has no grass): a damp moss patch on the cave floor, ferns with white tips and dark spines."""
    base = spr_of_tile("cave_b")
    S = tile_canvas()
    for y in range(15):
        for x in range(28):
            if base.mk[y][x]:
                S.t(x, y, base.lv[y][x])
    tufts = [
        (x, y)
        for r, y in enumerate((4, 8, 12))
        for x in range(3 + (4 if r % 2 else 0), 26, 8)
    ]
    for x, y in tufts:
        pts = []
        for dx, dy, lv in (
            (-1, 0, 3),
            (0, 0, 3),
            (1, 0, 3),
            (-1, -1, 3),
            (-2, -2, 0),
            (-3, -3, 0),
            (1, -1, 3),
            (2, -2, 0),
            (3, -3, 0),
            (0, -1, 3),
            (0, -2, 3),
            (0, -3, 0),
            (0, -4, 0),
            (-3, -2, 0),
            (3, -2, 0),
        ):
            if in_diamond(x + dx, y + dy, 7) and in_diamond(x + dx, y + dy + 1, 7):
                pts.append((x + dx, y + dy, lv))
        for px_, py_, lv in sorted(pts, key=lambda t: -t[1]):
            S.t(px_, py_, lv)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


LEDGE_FACE_C2 = 7  # px of rock face under the lip (the landing tile carries it: the next row's ground would paint over a face drawn by the lip tile)


def ledge_pair(lip_name, foot_name, d, base="grass_a"):
    """A one-way ledge made of two ground tiles (C_LEDGE on the lip only): the LIP tile (the ledge cell: base ground + a dark lip line with a
    lit rim along the edge the hero drops over) and the FOOT tile (the landing cell: base ground + the rock face along its edge shared with the lip,
    strata and a dark base line). d = 'sw' (the hero hops +j) or 'se' (+i)."""
    lip_edge = LEDGE_EDGE[d]  # the lip's own edge (x range, y of the edge at x)
    foot_edge = LEDGE_EDGE[
        "ne" if d == "sw" else "nw"
    ]  # the same edge seen from the landing tile
    rnd = random.Random(61 + ord(d[1]))
    S = spr_of_tile(base)
    xs, edge = lip_edge
    for x in xs:
        ye = edge(x)
        S.t(x, ye, 3)
        S.t(x, ye - 1, 1)
        if x % 2:
            S.t(x, ye - 2, 1)
    finish(lip_name, S, "tile", 1, 1, tile=True, outline=False)
    S = spr_of_tile(base)
    xs, edge = foot_edge
    for x in xs:
        ye = edge(x)
        for k in range(0, LEDGE_FACE_C2 + 1):
            y = ye + k
            if not in_diamond(x, y, 7):
                continue
            if k == 0 or k == LEDGE_FACE_C2:
                lv = 3
            elif k in (2, 5) and (x + k) % 5:
                lv = 3
            elif rnd.random() < 0.12:
                lv = 3
            else:
                lv = 2 if k < LEDGE_FACE_C2 - 1 else 1
            S.t(x, y, lv)
    finish(foot_name, S, "tile", 1, 1, tile=True, outline=False)


def unify_frames(names):
    """The frames of an animated object must have the same size and anchor (contract 22.2): re-save them padded to the union box (anchor-relative)."""
    ims, metas = [], []
    for n in names:
        lv, mk, m = load_lv(n)
        ims.append((lv, mk))
        metas.append(m)
    x0 = min(-m["ax"] for m in metas)
    y0 = min(-m["ay"] for m in metas)
    x1 = max(m["w"] - m["ax"] for m in metas)
    y1 = max(m["h"] - m["ay"] for m in metas)
    w, h = x1 - x0, y1 - y0
    for n, (lv, mk), m in zip(names, ims, metas):
        im = Image.new("RGBA", (w, h), (255, 255, 255, 0))
        p = im.load()
        for y in range(m["h"]):
            for x in range(m["w"]):
                if mk[y][x]:
                    g = GRAY[lv[y][x]]
                    p[x - m["ax"] - x0, y - m["ay"] - y0] = (g, g, g, 255)
        im.save(os.path.join(OUT_DIR, n + ".lv.png"))
        meta = dict(m)
        meta.update({"w": w, "h": h, "ax": -x0, "ay": -y0})
        with open(os.path.join(OUT_DIR, n + ".json"), "w") as f:
            json.dump(meta, f)
        for r in range(len(RESULTS)):
            if RESULTS[r][0] == n:
                RESULTS[r] = (
                    n,
                    RESULTS[r][1],
                    w,
                    h,
                    -x0,
                    -y0,
                    RESULTS[r][6],
                    RESULTS[r][7],
                )


def flat_piece(fn, new_name):
    """Draw one of the existing pieces again under another name with its opaque white turned into level 1 (no mask: 1/3 fewer bytes in a map's bank, the
    trick of Ciudad Nodo's small pieces, `Spr.flat_white`): Ciudad Marea's three big buildings would not fit its memory budget otherwise.
    """
    orig = globals()["finish"]

    def f2(name, S, kind, fw, fh, tile=False, outline=True):
        S.flat_white(1)
        orig(new_name, S, kind, fw, fh, tile, outline)

    globals()["finish"] = f2
    try:
        fn()
    finally:
        globals()["finish"] = orig


def edge_tag(edges):
    return "_".join(edges) if edges else "o"


# animated water variants the maps use (edge sets with a bank / foam on the land sides); frames 0 and 2 of the mock's 4 phases (2 frames per sprite)
SEA_VARIANTS = (("tl",), ("tr",), ("bl",), ("tl", "tr"), ())
CANAL_VARIANTS = (("tr",), ("bl",), ("tl", "tr"), ())
POOL_VARIANTS = (("tl",), ("tr",), ("tl", "tr"), ())
DECK_VARIANTS = (("bl",), ("tr",))
WALK_VARIANTS = tuple(
    tuple(e for k, e in enumerate(("tl", "tr", "br", "bl")) if m >> k & 1)
    for m in range(16)
)  # every combination of water sides


def c2_pieces():
    # Ruta 3 (mountain trail): dry earth, the trail with ruts, trainer spots, rock slopes, fences, dry bushes, scree, cairn, scrub (C_GRASS)
    for k, (kind, seed) in enumerate(
        (("pebbles", 1), ("plain", 2), ("tuft", 3), ("cracks", 4))
    ):
        earth("earth_" + "abcd"[k], kind, seed)
    mtrail("mtrail_a", 1)
    mtrail("mtrail_b", 2)
    trainer_spot("trainer_spot")
    slope("slope_a", 1)
    slope("slope_b", 2)
    slope("slope_c", 3, tiers=2)
    rock_fence("rfence_i", "i")
    rock_fence("rfence_j", "j")
    dry_bush("dry_bush_a", 1)
    dry_bush("dry_bush_b", 2)
    scree("scree")
    cairn("cairn")
    scrub_tile("scrub")
    # Monte Eco (cave): floor, glowing veined walls (2 frames), stalagmites, crystals, rope ladder, the Vertice dig
    cave_floor("cave_a", "cracks", 1)
    cave_floor("cave_b", "plain", 2)
    cave_floor("cave_c", "pebbles", 3)
    moss_tile("moss")
    fossil_dig("fossil_dig")
    for k, seed in enumerate((1, 3)):
        for ph in (0, 1):
            cave_wall("cwall_%s%d" % ("ab"[k], ph), seed, ph)
    cave_wall("cwall_p", 7, 0, veined=False)
    stalagmite("stalag_a", 22, 1)
    stalagmite("stalag_b", 22, 2)
    stalagmite("stalag_s", 14, 3)
    for ph in (0, 1):
        crystal_cluster("crystal_%d" % ph, ph)
        v_flag("vflag_%d" % ph, ph)
    rope_ladder("rope_ladder")
    fossil_actor("fossil_tri")
    fossil_actor("fossil_lily")
    crate("crate_v", "V")
    crate("crate_plain")
    crate_stack("crate_stack")
    dig_tools("dig_tools")
    barrel("barrel")
    # coast (Ruta 4, Rutas 24-25): sand, sea (foam on the shore sides), beach grass, rocks in the shallows, a sailboat, grassy cliffs, ledges
    for k, (kind, seed) in enumerate((("s", 1), ("s", 2), ("s", 3), ("shell", 4))):
        sand("sand_" + "abcd"[k], kind, seed)
    for e in SEA_VARIANTS:
        for f, ph in enumerate((0, 2)):
            sea_tile("sea_%s_%d" % (edge_tag(e), f), ph, e)
    beach_grass("beach_grass_a", 1)
    beach_grass("beach_grass_b", 2)
    sea_rock("sea_rock_a", 1)
    sea_rock("sea_rock_b", 2)
    for ph in (0, 1):
        sailboat("sailboat_%d" % ph, ph)
    for k in range(3):
        cliff_grass("cliff_grass_" + "abc"[k], k + 1)
    ledge_pair("ledge_lip_sw", "ledge_foot_sw", "sw")
    ledge_pair("ledge_lip_se", "ledge_foot_se", "se")
    # Ciudad Marea (harbour): wet cobbles, boardwalk, canal, bridge rails, moored boats, lamps, anchor, nets, traps
    for k in range(3):
        wet_stone("wet_" + "abc"[k], k + 1)
    for e in DECK_VARIANTS:
        for s in range(2):
            deck_tile("deck_%s_%d" % (edge_tag(e), s), "i", e, s, wet=True)
    for e in CANAL_VARIANTS:
        for f, ph in enumerate((0, 4)):
            canal_tile("canal_%s_%d" % (edge_tag(e), f), ph, e)
    deck_tile(
        "plank_tl_br", "i", ("tl", "br"), 0, wet=False
    )  # the 1-wide bridge over the canal (planks across, water on both sides)
    deck_tile("plank_o", "i", (), 0, wet=False)
    deck_tile("plank_bl", "i", ("bl",), 0, wet=False)
    deck_tile(
        "plank_tl", "i", ("tl",), 0, wet=False
    )  # the lanes of the long bridge of Ruta 24 (water on one side)
    deck_tile("plank_br", "i", ("br",), 0, wet=False)
    bridge_rails("bridge_rails_i", "i")
    bridge_rails("bridge_rails_j", "j")
    for ph in (0, 1):
        hull_boat("boat_%d" % ph, ph)
        hull_boat("boat_j%d" % ph, ph, flip=True)
        harbor_lamp("lamp_h%d" % ph, ph)
        cottage_antenna("cottage_%d" % ph, ph)
    bollard("bollard")
    net_rack("net_rack")
    anchor_monument("anchor")
    lobster_trap("trap")
    # Gimnasio de Agua: pool deck, pool water, walkways, lane ropes, buoys, diving board, lifeguard chair, wave walls
    flat_piece(gym_marea, "gym_marea_f")
    flat_piece(fb_center, "center_f")
    flat_piece(fb_shop, "mart_f")
    pool_floor("pool_a", 1)
    pool_floor("pool_b", 2)
    for e in POOL_VARIANTS:
        for f, ph in enumerate((0, 4)):
            canal_tile("pool_w_%s_%d" % (edge_tag(e), f), ph, e, base=1)
    for e in WALK_VARIANTS:
        walk_tile("walk_" + edge_tag(e), e)
    gym_wall("gym_wall_i", "i")
    gym_wall("gym_wall_j", "j")
    lane_rope("lane_rope_i", "i")
    lane_rope("lane_rope_j", "j")
    for ph in (0, 1):
        buoy("buoy_%d" % ph, ph)
    diving_board("diving_board")
    lifeguard_chair("lifeguard_chair")
    for fr in (
        ("cwall_a0", "cwall_a1"),
        ("cwall_b0", "cwall_b1"),
        ("crystal_0", "crystal_1"),
        ("vflag_0", "vflag_1"),
        ("sailboat_0", "sailboat_1"),
        ("boat_0", "boat_1"),
        ("boat_j0", "boat_j1"),
        ("lamp_h0", "lamp_h1"),
        ("cottage_0", "cottage_1"),
        ("buoy_0", "buoy_1"),
    ):
        unify_frames(fr)


# ================================================================ Chapter 3 (C3, contract 34): the zones of Ruta 5 .. Cueva Excavadora
# The drawings below are the pieces of tools/c3_mock.py (approved by the user zone by zone) moved here UNCHANGED; the mock now loads the generated files.
# Every function RETURNS the spec of a piece (the sprite and its finish() arguments), `c3_pieces()` writes them under their file names.
def _c3done(S, kind="object", fw=1, fh=1, tile=False, outline=True):
    return (S, kind, fw, fh, tile, outline)


def _c3T(S):
    return _c3done(S, "tile", tile=True, outline=False)


def sidewalk(seed=0):
    """Suburban paving: 3 x 3 square slabs with light joints, a few specks."""

    def f(x, y, u, v, r):
        k = 3
        if min(abs(u * k - round(u * k)), abs(v * k - round(v * k))) < 0.07:
            return 1
        return 2 if r.random() < 0.03 else 0

    return _c3T(gtile(f, seed))


def lawn(seed=0):
    """Mowed lawn: white with soft diagonal mowing stripes (dithered light gray) and a few short blades."""

    def f(x, y, u, v, r):
        if int((u + v) * 5) % 2 == 0 and dither(x, y, 0.35):
            return 1
        return 0

    S = gtile(f, seed)
    marks(S, random.Random(seed), 4, 2, shapes=(((0, 0), (0, -1)),))
    return _c3T(S)


def concrete(seed=0, base=1):
    """Underground concrete: level `base` with darker 2 x 2 slab joints and pitting."""

    def f(x, y, u, v, r):
        if min(abs(u * 2 - round(u * 2)), abs(v * 2 - round(v * 2))) < 0.05:
            return min(3, base + 1)
        if r.random() < 0.06:
            return min(3, base + 1) if r.random() < 0.7 else max(0, base - 1)
        return base

    return _c3T(gtile(f, seed))


def quay(seed=0, stripe=False):
    """Port quay: light concrete (white with gray dither) in big slabs; `stripe` = a painted white safety line with dark borders along +j."""

    def f(x, y, u, v, r):
        if stripe and 0.42 < u < 0.58:
            return 3 if (u < 0.45 or u > 0.55) else 0
        if min(abs(u * 2 - round(u * 2)), abs(v * 2 - round(v * 2))) < 0.04:
            return 2
        return 1 if dither(x, y, 0.3) else 0

    return _c3T(gtile(f, seed))


def metal(seed=0):
    """Gym floor: steel plates (level 1) with a diamond-tread pattern (2), plate seams (3) and white rivets at the corners."""

    def f(x, y, u, v, r):
        if u < 0.04 or v < 0.04:
            return 3
        if (u < 0.12 or u > 0.88) and (v < 0.12 or v > 0.88):
            return 0
        if (x + 2 * y) % 6 == 0 or (x - 2 * y) % 6 == 0:
            return 2
        return 1

    return _c3T(gtile(f, seed))


def metal_big(pi, pj, seed=0):
    """Gym floor, big steel plates of 2 x 2 tiles (the user found 1-tile plates too busy, like Cantera's small slabs): this tile is the
    (pi, pj) quarter of a plate. Plain light-gray steel with a soft brushed dither, seams (3) only on the plate's outer edges, white rivets at the
    plate's corners."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.035) or (pj == 0 and v < 0.035):
            return 3
        cu = u < 0.1 if pi == 0 else u > 0.9
        cv = v < 0.1 if pj == 0 else v > 0.9
        if cu and cv:
            return 0
        if (pi == 0 and u < 0.07) or (pj == 0 and v < 0.07):
            return 1  # the bevel along the seam
        return 1 if dither(x, y, 0.45) else 0

    return _c3T(gtile(f, seed))


def carpet(seed=0):
    """Ship-cabin carpet: mid gray with a light diamond lattice."""

    def f(x, y, u, v, r):
        a, b = (u * 4) % 1.0, (v * 4) % 1.0
        if abs(a - b) < 0.1 or abs(a + b - 1) < 0.1:
            return 1
        return 2

    return _c3T(gtile(f, seed))


def dirt(seed=0):
    """Loose earth (Cueva Excavadora): light gray base, darker dithered patches, crumbs (dark) and lighter clods; softer than Monte Eco's rock."""
    rnd = random.Random(seed)
    bl = blob_field(rnd, 4, 6, 3)

    def f(x, y, u, v, r):
        if in_blobs(x, y, bl) and dither(x, y, 0.5):
            return 2
        return 1

    S = gtile(f, seed)
    marks(S, rnd, 6, 3, shapes=(((0, 0),), ((0, 0), (1, 0))))
    marks(S, rnd, 3, 0, shapes=(((0, 0), (1, 0), (0, 1)),))
    return _c3T(S)


def grate(seed=0):
    """Floor drain grate in the passage: dark slats across a concrete tile."""

    def f(x, y, u, v, r):
        if 0.25 < u < 0.75 and 0.25 < v < 0.75:
            return 3 if int(v * 16) % 2 == 0 else 2
        return 1

    return _c3T(gtile(f, seed))


# ================================================================ new objects
def hedge(seed=0):
    """Trimmed hedge block (1 tile, h 13): leafy top and faces, a darker base."""
    S = Spr()
    rnd = random.Random(seed)
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, 13, 1, 2, 2, edge=3)
    for F, dark in ((b.J, 3), (b.I, 3)):
        for sx in range(F.wid):
            for z in range(1, 13):
                if rnd.random() < 0.22:
                    F.px(sx, z, dark)
                elif rnd.random() < 0.08:
                    F.px(sx, z, 1)
        F.hline(0, F.wid, 0, 3)
    for _ in range(26):
        b.top_px(rnd.randrange(0, 15), rnd.randrange(0, 15), rnd.choice((0, 0, 2)))
    return _c3done(S, outline=False)


def container(stack=True):
    """Shipping containers (2 tiles along +i): corrugated faces, door bars on the +j end; two stacked (dark under light)."""
    S = Spr()
    tiers = ((0, 18, 2, 3, 2), (18, 36, 1, 1, 2)) if stack else ((0, 18, 2, 3, 2),)
    for z0, z1, top, left, right in tiers:
        b = Box(S, 1, 2, 0, 0, 7, 14, z0, z1, top, left, right, edge=3)
        for sx in range(
            0, b.I.wid, 2
        ):  # corrugation along the long +i face (Face z is absolute height)
            b.I.vline(sx, z0 + 1, z1 - 1, 3 if right == 2 else 2)
        for sx in (3, 6, 9):  # the door end (+j face): locking bars
            b.J.vline(sx, z0 + 2, z1 - 2, 3 if left != 3 else 1)
        b.J.hline(1, b.J.wid - 1, (z0 + z1) // 2, 3 if left != 3 else 1)
    return _c3done(S, fw=1, fh=2, outline=False)


def crane():
    """Dock crane: a lattice tower (two legs with X bracing) and a boom reaching up-left with a hook on a cable."""
    S = Spr()
    Box(S, 1, 1, 1, 1, 6, 6, 0, 6, 2, 3, 3, edge=3)
    H = 74
    for lx in (-5, 5):
        S.line(lx, -6, lx // 2, -H, 3)
    for k in range(6, H - 4, 10):  # X bracing
        x0 = -5 + (k * 5) // (2 * H)
        S.line(-5 + k // 30, -k, 5 - k // 30, -k - 10, 3)
        S.line(5 - k // 30, -k, -5 + k // 30, -k - 10, 3)
    S.rect(-6, -H - 6, 5, -H, 2)  # the cab
    S.hline(-6, 5, -H - 7, 3)
    S.rect(-4, -H - 4, -1, -H - 2, 0)
    for k in range(0, 46):  # boom up-left
        S.px(-6 - k, -H - 4 - k // 3, 3)
        S.px(-6 - k, -H - 2 - k // 3, 3)
        if k % 6 == 0:
            S.line(-6 - k, -H - 4 - k // 3, -6 - k, -H - 2 - k // 3, 3)
    S.line(-44, -H - 16, -44, -H + 26, 2)  # cable + hook
    S.rect(-46, -H + 26, -42, -H + 28, 3)
    S.px(-42, -H + 30, 3)
    S.px(-43, -H + 31, 3)
    return _c3done(S, outline=False)


def ferry():
    """The Ferry Albatros moored (4 x 2 tiles along +i): dark hull with a white waterline stripe, white two-deck superstructure with windows,
    a funnel with a light band."""
    S = Spr()
    hull = Box(S, 4, 2, 0, 0, 28, 14, 0, 14, 1, 3, 2, edge=3)
    hull.J.hline(0, hull.J.wid, 4, 0)
    hull.I.hline(0, hull.I.wid, 4, 1)
    sup = Box(S, 4, 2, 4, 3, 22, 12, 14, 28, 1, 0, 1, edge=3)
    for s0 in range(3, sup.I.wid - 4, 6):
        sup.I.window(s0, s0 + 4, 20, 25, glass=2, cross=False, glint=False)
    for s0 in range(3, sup.J.wid - 3, 6):
        sup.J.window(s0, s0 + 4, 20, 25, glass=2, cross=False, glint=False)
    up = Box(S, 4, 2, 8, 5, 18, 10, 28, 36, 0, 0, 1, edge=3)
    fun = Box(S, 4, 2, 13, 6, 16, 9, 36, 50, 3, 2, 3, edge=3)
    fun.J.hline(0, fun.J.wid, 45, 0)
    fun.I.hline(0, fun.I.wid, 45, 0)
    return _c3done(S, fw=4, fh=2, outline=False)


def fuse_box(lit=False):
    """Electrical cabinet (1 tile, h 30): dark steel body, a light door with a lightning bolt and a vent, a conduit pipe up to the ceiling and a
    status lamp on top (`lit` = its switch is on: white lamp, door open a crack)."""
    S = Spr()
    b = Box(S, 1, 1, 1, 2, 6, 6, 0, 26, 2, 2, 3, edge=3)
    J, I = b.J, b.I
    J.rect(1, J.wid - 1, 3, 23, 1 if not lit else 0)
    J.hline(1, J.wid - 1, 2, 3)
    J.hline(1, J.wid - 1, 23, 3)
    J.vline(J.wid - 2, 3, 23, 3)
    for k, (sx, z) in enumerate(
        ((5, 20), (4, 18), (3, 16), (6, 15), (5, 13), (4, 11), (3, 9))
    ):
        J.px(sx, z, 3)
        J.px(sx + 1, z, 3)
    for z in (5, 7):
        J.hline(2, J.wid - 3, z, 2)
    for z in range(4, 22, 3):
        I.hline(2, I.wid - 2, z, 3)
    S.rect(-1, -36, 0, -27, 3)  # conduit
    S.rect(-2, -30, 1, -27, 0 if lit else 3)  # status lamp
    if lit:
        for x, y in ((-4, -31), (3, -31), (-1, -34)):
            S.px(x, y, 0)
    return _c3done(S, outline=False)


def barrier(phase=0):
    """Electric barrier across a doorway (two dark posts along +i, a jumping arc between them): 2 frames."""
    S = Spr()
    for a in (0, 6):
        Box(S, 1, 1, a, 3, a + 1, 4, 0, 22, 2, 3, 3, edge=3)
    rnd = random.Random(phase * 7 + 3)
    for z in (6, 11, 16):
        prev = None
        for k in range(1, 12):
            x = -12 + k * 2 + 2
            y = -3 - k - z + rnd.choice((-1, 0, 1, 2) if phase else (-2, -1, 0, 1))
            if prev:
                S.line(prev[0], prev[1], x, y, 0)
                S.px(x, y + 1, 1)
            prev = (x, y)
    return _c3done(S, outline=False)


def cable_wall(axis="i"):
    """Gym wall: dark steel panels with sagging cables and small status lights."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 3, 2, 3, edge=3)
    for F, lv in ((b.J, 1), (b.I, 1)):
        for k, z0 in enumerate((24, 17)):
            for sx in range(F.wid):
                F.px(
                    sx,
                    z0 - int(3 * math.sin(math.pi * sx / max(1, F.wid - 1))),
                    3 if k else lv,
                )
        F.rect(2, 7, 4, 11, 1)
        F.px(4, 9, 0)
        F.px(5, 7, 3)
        F.rect(F.wid - 7, F.wid - 2, 4, 11, 1)
        F.px(F.wid - 5, 9, 0)
    if axis == "j":  # the bolt emblem
        for k in range(8):
            b.J.px(12 + (k % 4), 28 - k, 0)
            b.J.px(13 + (k % 4), 28 - k, 0)
    return _c3done(S, outline=False)


def railing(axis="i"):
    """Ship railing: white posts and two light rails (1 tile)."""
    S = Spr()
    P_ = 5
    for z0, z1 in ((5, 6), (10, 11)):
        if axis == "i":
            Box(S, 1, 1, 0, P_, 7, P_ + 1, z0, z1, 0, 0, 1, edge=3)
        else:
            Box(S, 1, 1, P_, 0, P_ + 1, 7, z0, z1, 0, 0, 1, edge=3)
    for a in (0, 4):
        if axis == "i":
            Box(S, 1, 1, a, P_, a + 1, P_ + 1, 0, 12, 0, 1, 1, edge=3)
        else:
            Box(S, 1, 1, P_, a, P_ + 1, a + 1, 0, 12, 0, 1, 1, edge=3)
    return _c3done(S, outline=False)


def lifebuoy():
    """A lifebuoy ring on a short stand: white ring with 4 dark quarter bands."""
    S = Spr()
    S.rect(-1, -6, 1, 0, 2)
    cx, cy = 0, -13
    for y in range(-8, 9):
        for x in range(-8, 9):
            d = math.hypot(x, y * 1.15)
            if 4.2 <= d <= 8.2:
                ang = math.atan2(y, x)
                q = int(((ang + math.pi) / (math.pi / 2)) + 0.5) % 4
                band = abs(((ang + math.pi) % (math.pi / 2)) - math.pi / 4) < 0.28
                S.px(cx + x, cy + y, 3 if d > 7.6 or d < 4.8 else (2 if band else 0))
    return _c3done(S, outline=False)


def funnel():
    """Ship funnel on deck (1 tile, tall): a dark cylinder with a white band and smoke puffs."""
    S = Spr()
    for y in range(-46, 0):
        for x in range(-7, 8):
            lv = 2 if x < -2 else 3
            if -36 <= y <= -31:
                lv = 0 if x < 3 else 1
            S.px(x, y, lv)
    for x in range(-7, 8):
        S.px(x, -47, 3)
        S.px(x, -46 + (1 if abs(x) < 6 else 0), 2)
    for cx, cy, r in ((3, -52, 3), (7, -58, 4), (12, -64, 5)):
        for y in range(-r, r + 1):
            for x in range(-r, r + 1):
                if x * x + y * y <= r * r:
                    S.px(cx + x, cy + y, 1 if (x + y) < 0 else 2)
    return _c3done(S, outline=False)


def bunk():
    """Cabin bunk bed (1 tile): two mattresses, a ladder post, dark frame."""
    S = Spr()
    for z0 in (2, 16):
        Box(S, 1, 1, 1, 1, 7, 6, z0, z0 + 3, 2, 3, 3, edge=3)
        Box(S, 1, 1, 1, 1, 7, 6, z0 + 3, z0 + 6, 0, 1, 1, edge=3)
    for a, b_ in ((1, 6), (6, 6), (6, 1)):
        Box(S, 1, 1, a, b_, a + 1, b_ + 1, 0, 24, 3, 3, 3, edge=3)
    return _c3done(S, outline=False)


def porthole_wall(axis="i"):
    """Cabin wall: light panels with a round porthole (dark rim, sea-gray glass, glint)."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 3, 0, 1, edge=3)
    for F in (b.J, b.I):
        F.rect(0, F.wid, 0, 4, 2)
        cs, cz = F.wid // 2, 20
        for s in range(F.wid):
            for z in range(10, 31):
                d = math.hypot((s - cs) * 0.9, z - cz)
                if d <= 6.5:
                    F.px(
                        s, z, 3 if d > 5 else (0 if (s - cs < -1 and z > cz + 1) else 2)
                    )
    return _c3done(S, outline=False)


def tile_wall(axis="i"):
    """Underground passage wall: small white tiles (light joints), a dark baseboard and a mid stripe."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 3, 0, 1, edge=3)
    for F in (b.J, b.I):
        for sx in range(F.wid):
            for z in range(4, WALL_H):
                if sx % 4 == 0 or z % 4 == 0:
                    F.px(sx, z, 1 if F is b.J else 2)
        F.rect(0, F.wid, 0, 4, 3)
        F.rect(0, F.wid, 15, 18, 2)
    return _c3done(S, outline=False)


def light_post():
    """Passage light: a slim post with a glowing white box head and halo dither."""
    S = Spr()
    S.rect(-1, -24, 0, 0, 3)
    S.rect(-4, -31, 3, -25, 0)
    S.hline(-4, 3, -32, 3)
    S.hline(-4, 3, -24, 3)
    for y in range(-38, -20):
        for x in range(-9, 9):
            if (
                not (-4 <= x <= 3 and -32 <= y <= -24)
                and math.hypot(x, (y + 28) * 1.3) < 8
                and dither(x, y, 0.25)
            ):
                S.px(x, y, 1)
    return _c3done(S, outline=False)


def gatehouse():
    """Guard hut / gate to Ciudad Cenit (2 x 2): light walls, dark flat roof, a barred door on the +j face and a small 'X' plate."""
    S = Spr()
    b = Box(S, 2, 2, 0, 0, 14, 14, 0, 24, 3, 0, 1, edge=3)
    Box(S, 2, 2, -1, -1, 15, 15, 24, 27, 2, 3, 3, edge=3)
    J = b.J
    J.rect(10, 18, 0, 16, 3)
    for sx in range(11, 18, 2):
        J.vline(sx, 1, 15, 1)
    J.rect(20, 25, 18, 22, 2)
    for k in range(4):
        J.px(21 + k, 18 + k, 0)
        J.px(24 - k, 18 + k, 0)
    b.I.window(6, 14, 8, 16)
    return _c3done(S, fw=2, fh=2, outline=False)


def root_wall(seed=0):
    """Earthen cave wall: dark earth faces with lighter dithered clods and roots hanging from the top."""
    S = Spr()
    rnd = random.Random(seed)
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, 34, 3, 2, 3, edge=3)
    for F, lo in ((b.J, 2), (b.I, 3)):
        rock_dither_face(F, rnd, 34, lo, 1 if lo == 2 else 2, cover=0.4)
        for _ in range(3):
            s = rnd.randrange(2, F.wid - 2)
            for z in range(33, 33 - rnd.randrange(10, 22), -1):
                F.px(s, z, 0 if lo == 3 else 1)
                if rnd.random() < 0.25:
                    s += rnd.choice((-1, 1))
                    F.px(s, z, 0 if lo == 3 else 1)
    return _c3done(S, outline=False)


def mound(seed=0):
    """A dug-up dirt mound."""
    S = Spr()
    rnd = random.Random(seed)
    lump(S, 0, -4, 9, 4, rnd, tone=1, cracks=1)
    lump(S, 3, -7, 5, 3, rnd, tone=1, cracks=0)
    return _c3done(S, outline=False)


def hole():
    """A burrow hole in the floor (flat): black ellipse with a crumbly rim."""
    S = Spr()
    for y in range(-6, 1):
        for x in range(-9, 10):
            d = (x / 9.0) ** 2 + ((y + 3) / 3.6) ** 2
            if d <= 1:
                S.px(x, y, 3 if d < 0.6 else 2)
    for x in (-8, -3, 4, 8):
        S.px(x, -6 + abs(x) // 4, 1)
    return _c3done(S, outline=False)


def daycare_sign():
    """The day-care sign: a board on two posts with a heart (light) on it."""
    S = Spr()
    S.rect(-6, -10, -5, 0, 3)
    S.rect(5, -10, 6, 0, 3)
    S.rect(-9, -22, 9, -10, 0)
    S.hline(-9, 9, -23, 3)
    S.hline(-9, 9, -9, 3)
    for x in range(-9, 10):
        S.px(x, -23, 3)
    for y in range(-22, -9):
        S.px(-10, y, 3)
        S.px(10, y, 3)
    for y in range(-20, -11):
        w = (y + 20) if y < -16 else (-11 - y) * 2 // 2
        for x in range(-w, w + 1):
            if abs(x) <= 4:
                S.px(x, y, 2)
    return _c3done(S, outline=False)


def pylon():
    """High-voltage pylon (1 tile, ~84 px): a tapering lattice tower with X bracing, two cross-arms with hanging insulators."""
    S = Spr()
    H = 84
    for y in range(0, H + 1):
        w = 9 - (y * 7) // H
        S.px(-w, -y, 3)
        S.px(w, -y, 3)
    for y0 in range(0, H - 8, 10):
        w0, w1 = 9 - (y0 * 7) // H, 9 - ((y0 + 10) * 7) // H
        S.line(-w0, -y0, w1, -y0 - 10, 2)
        S.line(w0, -y0, -w1, -y0 - 10, 2)
        S.hline(-w0, w0, -y0, 3)
    for ya, half in ((H - 26, 20), (H - 8, 14)):
        S.hline(-half, half, -ya, 3)
        S.hline(-half + 2, half - 2, -ya + 1, 3)
        for x in (-half, half):
            for k in range(5):
                S.px(x, -ya + 2 + k, 0 if k % 2 else 3)
    S.rect(-1, -H - 4, 1, -H, 3)
    return _c3done(S, outline=False)


def transformer():
    """Fenced transformer box: a dark box with cooling fins, a warning plate with a bolt, a low wire fence in front."""
    S = Spr()
    b = Box(S, 1, 1, 1, 1, 6, 6, 0, 16, 2, 2, 3, edge=3)
    for sx in range(1, b.I.wid - 1, 2):
        b.I.vline(sx, 2, 14, 3)
    b.J.rect(3, 8, 7, 13, 0)
    for k, (sx, z) in enumerate(((6, 12), (5, 11), (4, 10), (6, 9), (5, 8))):
        b.J.px(sx, z, 3)
    for a in (0, 3, 6):
        Box(S, 1, 1, a, 6, a + 1, 7, 0, 9, 1, 2, 2, edge=3)
    Box(S, 1, 1, 0, 6, 7, 7, 7, 8, 1, 1, 2, edge=3)
    return _c3done(S, outline=False)


def kiosk():
    """Park kiosk / gazebo (1 tile): four slim posts, a low rail and a pointed dark roof."""
    S = Spr()
    for a, b_ in ((0, 0), (6, 0), (0, 6), (6, 6)):
        Box(S, 1, 1, a, b_, a + 1, b_ + 1, 0, 24, 0, 1, 2, edge=3)
    Box(S, 1, 1, 0, 6, 7, 7, 6, 7, 0, 1, 1, edge=3)
    Box(S, 1, 1, 6, 0, 7, 7, 6, 7, 0, 1, 1, edge=3)
    for y in range(0, 14):
        w = 18 - (y * 18) // 14
        for x in range(-w, w + 1):
            S.px(x, -24 - 7 - y + abs(x) // 4, 2 if x < 0 else 3)
    S.rect(0, -48, 1, -44, 3)
    return _c3done(S, outline=False)


def lily():
    """Lily pads on the pond (flat, 1 tile): two light pads with a notch and one white flower."""
    S = Spr()
    for cx, cy, r in ((-6, -4, 5), (5, -2, 4)):
        for y in range(-r, r + 1):
            for x in range(-r - 2, r + 3):
                if (x / (r + 1.5)) ** 2 + (y / r) ** 2 <= 1 and not (
                    x > 0 and abs(y) <= x // 3
                ):
                    S.px(cx + x, cy + y // 2, 1)
        S.px(cx, cy, 2)
    for x, y in ((-7, -7), (-6, -8), (-5, -7), (-6, -6)):
        S.px(x, y, 0)
    S.px(-6, -7, 2)
    return _c3done(S, outline=False)


def cut_shrub():
    """The cuttable bush (an actor of 1 direction, replaces the placeholder copy of dry_bush_a): a ROUND, dense, leafy dome, clearly not the trimmed
    hedge block nor the leafless dry bushes: the silhouette is the union of leaf lobes (a scalloped rim, one dark outline), the foliage is mid-gray with
    leaf clusters (light on the lit upper-left half, dark on the shaded lower-right half) and a darker core: a dithered 3 / 2 patch low in the middle.
    """
    S = Spr()
    rnd = random.Random(5)
    lobes = (
        (-8, -4, 4),
        (8, -4, 4),
        (-5, -8, 5),
        (5, -8, 5),
        (0, -10, 5),
        (0, -5, 8),
        (-3, -2, 5),
        (3, -2, 5),
    )
    for cx, cy, r in lobes:
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                if (x - cx) ** 2 + (y - cy) ** 2 <= r * r + r * 0.5 and y <= 0:
                    S.px(x, y, 2)
    x0, y0, x1, y1 = S.bbox()
    cells = [
        (x - S.ox, y - S.oy)
        for y in range(y0, y1 + 1)
        for x in range(x0, x1 + 1)
        if S.mk[y][x]
    ]
    for (
        x,
        y,
    ) in (
        cells
    ):  # foliage: light clusters on the lit side, dark clusters on the shaded side
        t = (x - 0) * 0.5 + (y + 6) * 0.9
        if t < -3 and rnd.random() < 0.34:
            S.px(x, y, 1)
        elif t > 3 and rnd.random() < 0.3:
            S.px(x, y, 3)
    for (
        cx,
        cy,
        r,
    ) in (
        lobes
    ):  # every lobe reads as a clump of leaves: a dark arc on its shaded lower-right, a light one upper-left
        for k in range(-r + 1, r):
            x, y = cx + k, cy + int(math.sqrt(max(0, (r - 1) ** 2 - k * k)) * 0.8)
            if k >= -1 and S.get(x, y) == 2 and y < -1:
                S.px(x, y, 3)
            x, y = cx + k, cy - int(math.sqrt(max(0, (r - 1) ** 2 - k * k)) * 0.8)
            if k <= 1 and S.get(x, y) in (1, 2) and y < -2 and dither(x, y, 0.8):
                S.px(x, y, 0)
    for y in range(
        -6, 0
    ):  # the darker core: low in the middle, dithered so it reads as deep foliage and not a hole
        for x in range(-6, 7):
            d = (x / 6.5) ** 2 + ((y + 3) / 3.2) ** 2
            if d <= 1.0 and S.get(x, y) is not None:
                S.px(x, y, 3 if dither(x, y, 0.8 - 0.5 * d) else 2)
    S.outline(3)
    for x in range(-6, 7):  # the foot: a flat dark base where the dome meets the ground
        S.px(x, 1, 3)
    for x in range(-4, 5):
        S.px(x, 2, 3 if x % 2 == 0 else 2)
    return S


def cut_bush_actor(name="cut_bush"):
    """Write the shrub as the ACTOR art assets/art/npcs/cut_bush/south/f0 (1 direction, 1 frame; the anchor like fossil_actor: 3 rows below it)."""
    import lvlib

    S = cut_shrub()
    x0, y0, x1, y1 = S.bbox()
    lv = [
        [S.lv[y][x] if S.mk[y][x] else 0 for x in range(x0, x1 + 1)]
        for y in range(y0, y1 + 1)
    ]
    mk = [[S.mk[y][x] for x in range(x0, x1 + 1)] for y in range(y0, y1 + 1)]
    lvlib.write_lv_png(
        os.path.join(ACTORS_DIR, name, "south", "f0.lv.png"),
        lv,
        mk,
        (x1 - x0 + 1) // 2,
        len(lv) - 3,
    )


# ---- new pieces of the real maps (not in the mock): the gym building of Puerto Dinamo, the cave's encounter tile, the overhead cables of Ruta 11
def gym_dinamo(name="gym_dinamo"):
    """Gimnasio Voltio (C3): the gym block of Ciudad Marea's family (3 x 2, an OPEN doorway in the middle tile) with the grid on it: a zigzag cable band round the front, an insulator
    bead on every pilaster, a lightning bolt on the plaque and a lightning rod with two cross arms and a ball on the roof. Flat white (opaque white -> level 1, no mask), like
    Marea's three big buildings: the memory budget."""
    fw, fh = 3, 2
    S = Spr()
    WH = 34
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 1, 1, 2, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 4, 2)
    I.rect(0, I.wid, 0, 4, 3)
    for sf, lvb, lvd in (
        (J, 0, 2),
        (I, 1, 3),
    ):  # the cable band: a zigzag (saw) line over the lower wall
        for sx in range(sf.wid):
            z = 9 + abs((sx % 8) - 4)
            sf.px(sx, z, lvd)
            sf.px(sx, z + 1, lvb)
    for s_ in range(
        0, J.wid, 14
    ):  # pilasters along the front, one per tile, an insulator bead on each
        J.vline(s_, 16, WH - 3, 3)
        J.vline(s_ + 1, 16, WH - 3, 2)
        J.px(s_, 20, 0)
    for s_ in range(0, I.wid, 14):
        I.vline(s_, 16, WH - 3, 3)
    J.hline(0, J.wid, WH - 4, 3)
    I.hline(0, I.wid, WH - 4, 3)
    J.rect(0, J.wid, WH - 3, WH, 2)
    I.rect(0, I.wid, WH - 3, WH, 3)
    J.rect(
        15, 27, 0, 24, 3
    )  # the open doorway (middle tile): dark inside, a few white sparks at the bottom
    J.rect(17, 25, 0, 22, 3)
    for sx in range(17, 26):
        J.px(sx, 1 + (sx % 3 == 0), 0)
        J.px(sx, 5 + (sx % 4 == 1), 1)
    J.rect(14, 29, 25, 31, 3)  # plaque above the door with the bolt
    J.rect(15, 28, 26, 30, 0)
    for k, (sx, z) in enumerate(
        ((23, 30), (22, 29), (21, 28), (23, 28), (22, 27), (21, 26))
    ):
        J.px(sx, z, 3)
        J.px(sx + 1, z, 3)
    r = roof(S, fw, fh, WH, 3)
    rz = WH + 3
    Box(S, fw, fh, 1, 1, 20, 13, rz, rz + 1, 1, 1, 2, edge=2)
    Box(S, fw, fh, 14, 8, 19, 12, rz + 1, rz + 4, 0, 1, 2, edge=3)  # roof plinth
    gx, gy = -1, -9 - (
        rz + 4
    )  # the lightning rod: a mast, two cross arms with beads, a ball on top
    S.line(gx, gy, gx, gy - 24, 3)
    S.line(gx + 1, gy, gx + 1, gy - 24, 3)
    for dz, half in ((10, 9), (17, 6)):
        S.hline(gx - half, gx + half, gy - dz, 3)
        S.px(gx - half, gy - dz + 1, 0)
        S.px(gx + half, gy - dz + 1, 0)
    S.disc(gx, gy - 28, 3, 0, ring=3)
    S.flat_white(1)
    finish(name, S, "object", fw, fh)


def dig_patch(name="dig_patch", seed=7):
    """The Cueva Excavadora's encounter tile (C_GRASS, like `moss` in Monte Eco): freshly turned burrow earth, a darker mid-gray with black crumbs and two small burrow rings
    (a dark ring around a light heart) where a root creature has been digging."""
    rnd = random.Random(seed)
    rings = ((0.28, 0.32), (0.7, 0.66))

    def f(x, y, u, v, r):
        for u0, v0 in rings:
            d = ((u - u0) / 0.15) ** 2 + ((v - v0) / 0.15) ** 2
            if 0.45 < d <= 1.0:
                return 3
            if d <= 0.45:
                return 1
        if r.random() < 0.09:
            return 3
        return 2 if dither(x, y, 0.92) else 1

    S = gtile(f, seed)
    finish(name, S, "tile", 1, 1, tile=True, outline=False)


def cable_span(name="cable_i"):
    """Overhead high-voltage cables across ONE tile along +i (Ruta 11): four straight cables at the height of the pylons' two cross arms, each a 2:1 segment that joins the one in the
    next tile. Placed on a non-solid cell between two pylons."""
    S = Spr()
    for xt, ya in ((-20, 58), (20, 58), (-14, 76), (14, 76)):
        yc = -7 - ya
        S.line(xt - 7, yc - 3, xt + 7, yc + 4, 3)
        S.line(xt - 7, yc - 2, xt + 7, yc + 5, 3)
    finish(name, S, "object", 1, 1, outline=False)


def blank_piece(name="blank"):
    """An invisible 1 x 1 object (one opaque level-0 pixel, catalog `mask = false` = nothing is drawn): lets an NPC stand ON a solid tile (the legend `object=blank npc=...` form of
    mapc's "an NPC may stand on a piece of furniture"), e.g. the ferry and its gangway on the sea of Puerto Dinamo.
    """
    S = Spr()
    S.px(0, 0, 0)
    finish(name, S, "object", 1, 1, outline=False)


def fuse_lamp_spr():
    """The gym's puzzle lamp (an actor that appears when the first switch is on): a white bulb in a dark cage on a short pole, rays around it."""
    S = Spr()
    S.rect(-1, -12, 0, 0, 3)
    S.rect(-3, -2, 2, 0, 3)
    S.disc(0, -17, 5, 0, ring=3)
    S.px(-2, -19, 1)
    S.px(-1, -19, 1)
    for x, y in (
        (-9, -17),
        (9, -17),
        (0, -25),
        (-7, -24),
        (7, -24),
        (-7, -10),
        (7, -10),
    ):
        S.line(
            x // 3 * 2 if x else 0,
            -17 + (y + 17) * 2 // 3 if y != -17 else -17,
            x,
            y,
            3,
        )
    return S


def gangway_spr():
    """The ferry's gangway (1 x 2 tiles along +j, from the quay down onto the hull): a slab of light planks with plank seams across it, two low rail posts at each end and a rail along its
    +i side. The NPC stands on its BACK tile (the quay-side one), see `_actor_write(back=True)`.
    """
    S = Spr()
    b = Box(S, 1, 2, 1, 0, 6, 14, 0, 3, 1, 2, 3, edge=3)
    for bb in range(2, 14, 2):
        b.top_line_a(bb, 1, 6, 2)
    for b0 in (0, 13):
        Box(S, 1, 2, 5, b0, 6, b0 + 1, 3, 11, 3, 3, 3, edge=3)
    Box(S, 1, 2, 5, 0, 6, 14, 9, 10, 3, 3, 3, edge=3)
    return S


def _actor_write(name, per_dir, dy=6, back=False):
    """Write the sprites of an ACTOR made from object-style drawings (anchor = the bottom vertex of the footprint's front tile): the NPC stands on that
    front tile, whose centre is 7 px above the vertex (the actor foot sits 1 px under the centre: `dy` = 6 rows between the actor anchor and the vertex).
    per_dir: {"south": Spr, ...}; every direction is cropped to the union box so the frames have one size. back=True: the NPC stands on the footprint's tile one step BACK along -j (the
    drawing is the same on screen: the anchor moves by (+14, -7) px): the ferry and its gangway need a walkable neighbour (the boardwalk) so that mapc finds them reachable.
    """
    import lvlib

    boxes = [S.bbox() for S in per_dir.values()]
    ox = [S.ox for S in per_dir.values()]
    oy = [S.oy for S in per_dir.values()]
    # union box in anchor-relative coordinates
    x0 = min(b[0] - o for b, o in zip(boxes, ox))
    x1 = max(b[2] - o for b, o in zip(boxes, ox))
    y0 = min(b[1] - o for b, o in zip(boxes, oy))
    y1 = max(b[3] - o for b, o in zip(boxes, oy))
    for d, S in per_dir.items():
        lv = [
            [
                S.lv[S.oy + y][S.ox + x] if S.mk[S.oy + y][S.ox + x] else 0
                for x in range(x0, x1 + 1)
            ]
            for y in range(y0, y1 + 1)
        ]
        mk = [
            [S.mk[S.oy + y][S.ox + x] for x in range(x0, x1 + 1)]
            for y in range(y0, y1 + 1)
        ]
        lvlib.write_lv_png(
            os.path.join(ACTORS_DIR, name, d, "f0.lv.png"),
            lv,
            mk,
            -x0 + (14 if back else 0),
            -y0 - dy - (7 if back else 0),
        )


def _flat_spec(spec):
    """The spec of a piece with its opaque white drawn light gray (`Spr.flat_white`): no mask, a third fewer bytes in a bank (the memory budget of the heavy maps)."""
    spec[0].flat_white(1)
    return spec


def c3_actors():
    """Actors drawn in code (assets/art/npcs/<name>/<dir>/f0): the zapping barrier (4 facings = 2 arc drawings: the gym's barrier NPC is `behavior=turn`, so it
    flickers between them), the puzzle lamp, the moored ferry and its gangway (both vanish with a flag when the ferry departs).
    """
    a, b = barrier(0)[0], barrier(1)[0]
    _actor_write(
        "barrier", {"south": a, "east": b, "north": a}
    )  # west = the mirrored east (catalog mirror_west)
    _actor_write("fuse_lamp", {"south": fuse_lamp_spr()})
    fs = ferry()[0]
    fs.flat_white(1)
    _actor_write("ferry", {"south": fs}, back=True)
    _actor_write("gangway", {"south": gangway_spr()}, back=True)


# the generated files (name -> spec); the variant counts are the ones of the mock's legends
def c3_pieces():
    def put(name, spec):
        S, kind, fw, fh, tile, outline = spec
        finish(name, S, kind, fw, fh, tile, outline)

    # ground
    for s in range(2):
        put("sidewalk_" + "ab"[s], sidewalk(s))
    for s in range(3):
        put("lawn_" + "abc"[s], lawn(s))
        put("concrete_" + "abc"[s], concrete(s, 1))
        put("dirt_" + "abc"[s], dirt(s))
    for s in range(2):
        put("quay_" + "ab"[s], quay(s))
    put("quay_s", quay(5, True))
    for pi in (0, 1):
        for pj in (0, 1):
            put("metal_%d%d" % (pi, pj), metal_big(pi, pj))
    put("carpet", carpet(0))
    put("grate", grate(0))
    # Ruta 5 / Ruta 6 (suburbs, park) and the passage
    for s in range(2):
        put("hedge_" + "ab"[s], hedge(s))
    put("dc_sign", daycare_sign())
    put("gatehouse", gatehouse())
    put("gatehouse_f", _flat_spec(gatehouse()))
    put("kiosk", kiosk())
    put("lily", lily())
    put("twall", tile_wall("i"))
    put("light_post", light_post())
    # Puerto Dinamo and the ferry
    put("container", container())
    put("crane", crane())
    put("crane_f", _flat_spec(crane()))
    put("ferry_moored", ferry())
    put("rail_i", railing("i"))
    put("rail_j", railing("j"))
    put("lifebuoy", lifebuoy())
    put("funnel", funnel())
    put("bunk", bunk())
    put("pwall", porthole_wall("i"))
    # the gym
    put("fuse_box", fuse_box())
    put("fuse_box_on", fuse_box(True))
    for ph in (0, 1):
        put("barrier_%d" % ph, barrier(ph))
    put("cable_wall_i", cable_wall("i"))
    put("cable_wall_j", cable_wall("j"))
    # Ruta 11
    put("pylon", pylon())
    put("pylon_f", _flat_spec(pylon()))
    cable_span()
    put("transformer", transformer())
    # the burrows
    for s in range(2):
        put("root_wall_" + "ab"[s], root_wall(s))
        put("mound_" + "ab"[s], mound(s))
    put("hole", hole())
    dig_patch()
    gym_dinamo()
    blank_piece()
    flat_piece(fb_house, "house_f")
    unify_frames(("barrier_0", "barrier_1"))
    cut_bush_actor()
    c3_actors()


# ================================================================ Chapter 4 (C4a, contract 39): Ruta 9 .. Metropolis Neon (+ the C4b zones: casino, hideout, tower)
# The drawings below are the pieces of tools/c4_mock.py (approved by the user zone by zone) moved here UNCHANGED; the mock now loads the generated files.
# Same convention as `c3_pieces()`: every function RETURNS a spec (the sprite and its finish() arguments) and `c4_pieces()` writes them under their file names.
def old_flags(pi, pj, seed=0):
    """Bruma street (the user found the small dark cobbles odd): big worn flagstones, 2 per 2x2 cell block, light stone (white with a soft
    gray dither), dark joints only at the stone edges with moss specks in them, a hairline crack here and there.
    """

    def f(x, y, u, v, r):
        U, V = u + pi, v + pj  # 0..2 across the 2x2 block
        if (pi == 0 and u < 0.035) or (pj == 0 and v < 0.035):
            return 3
        if (
            abs(V - (1.0 if U < 1.2 else 0.8)) < 0.03
        ):  # the joint between the two stones of the block, offset like laid stone
            return 2 if dither(x, y, 0.5) else 3
        if r.random() < 0.012:
            return 2
        return 1 if dither(x, y, 0.22) else 0

    S = gtile(f, seed + pi * 2 + pj)
    return _c3T(S)


def asphalt(seed=0, line=False):
    """City asphalt: dark gray with sparse specks; `line` = a dashed white lane line along +j."""

    def f(x, y, u, v, r):
        if line and 0.46 < u < 0.54 and int(v * 4) % 2 == 0:
            return 0
        return 3 if r.random() < 0.05 else (1 if r.random() < 0.04 else 2)

    return _c3T(gtile(f, seed))


def polished(pi, pj, seed=0):
    """Mall floor: big glossy 2x2 slabs (white), a hairline seam only on the slab's outer edges, a diagonal reflection streak."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        su, sv = u + pi, v + pj
        if abs((su - sv) - 0.3) < 0.06:
            return 1
        return 0

    return _c3T(gtile(f, seed))


def old_planks(pi, pj, seed=0):
    """Tower floor: light old boards running along +i, a seam every ~5 px, a darker joint only at the 2x2 edges, a few knots."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        if int((v + pj) * 7) != int((v + pj) * 7 - 0.03):
            return 1
        return 2 if r.random() < 0.02 else 0

    return _c3T(gtile(f, seed))


def casino_carpet(seed=0):
    """Casino carpet: LIGHT with a gray diamond lattice and dark dots (the first, dark version read almost black on the device and hid
    the hero; the user wants light floors)."""

    def f(x, y, u, v, r):
        a, b = (u * 2) % 1.0, (v * 2) % 1.0
        if abs(a - b) < 0.07 or abs(a + b - 1) < 0.07:
            return 2
        if abs(a - 0.5) < 0.08 and abs(b - 0.5) < 0.08:
            return 3
        return 1 if dither(x, y, 0.3) else 0

    return _c3T(gtile(f, seed))


def soil_path(seed=0):
    """Greenhouse path: plain light earth with a few leaf litter specks."""

    def f(x, y, u, v, r):
        return 2 if r.random() < 0.04 else (1 if dither(x, y, 0.2) else 0)

    return _c3T(gtile(f, seed))


def arrow_tile(d, pi, pj):
    """Hideout floor: a big steel plate with a bold arrow (d = 'i+' along +i ...)."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.035) or (pj == 0 and v < 0.035):
            return 3
        a, b = (u, v) if d[0] == "i" else (v, u)
        if d[1] == "-":
            a = 1 - a
        if 0.42 < b < 0.58 and 0.2 < a < 0.6:
            return 3
        if 0.6 <= a < 0.85 and abs(b - 0.5) < (0.85 - a) * 0.9:
            return 3
        return 1 if dither(x, y, 0.35) else 0

    return _c3T(gtile(f, 0))


def stonewall(axis="i", seed=0):
    """Low dry-stone wall (1 tile, h 10): stacked rounded stones."""
    S = Spr()
    rnd = random.Random(seed)
    b = (
        Box(S, 1, 1, 0, 2, 7, 5, 0, 10, 1, 2, 2, edge=3)
        if axis == "i"
        else Box(S, 1, 1, 2, 0, 5, 7, 0, 10, 1, 2, 2, edge=3)
    )
    for F in (b.J, b.I):
        for z in (3, 7):
            off = rnd.randrange(4)
            for s in range(F.wid):
                if (s + off) % 5 == 0:
                    F.vline(s, z - 3, z + 1, 3)
            F.hline(0, F.wid, z, 3)
    return _c3done(S, outline=False)


def tower_far():
    """The Torre de los Recuerdos seen from town (2 x 2, ~96 px): a slim stone tower, tiers, narrow windows, a pointed dark roof."""
    S = Spr()
    for k, (a0, a1, z0, z1) in enumerate(
        ((1, 13, 0, 34), (3, 11, 34, 62), (4, 10, 62, 82))
    ):
        b = Box(S, 2, 2, a0, a0, a1, a1, z0, z1, 1, 0, 1, edge=3)
        for F in (b.J, b.I):
            for s0 in range(3, F.wid - 3, 6):
                F.window(s0, s0 + 3, z0 + 8, z0 + 16, glass=3, cross=False, glint=False)
    for y in range(0, 26):
        w = 13 - (y * 13) // 26
        for x in range(-w, w + 1):
            S.px(x, -82 - 7 - y + abs(x) // 3, 3 if x > 0 else 2)
    return _c3done(S, fw=2, fh=2, outline=False)


def stele(seed=0):
    """Memorial monolith (1 tile): a narrow upright stone with a rounded top and engraved lines."""
    S = Spr()
    b = Box(S, 1, 1, 1, 2, 6, 5, 0, 22, 1, 2, 3, edge=3)
    for z in (8, 11, 14):
        b.J.hline(2, b.J.wid - 2, z, 0)
    for x in range(-7, 5):
        S.px(x, -29 + abs(x + 1) // 2, 3)
        S.px(x, -28 + abs(x + 1) // 2, 1)
    return _c3done(S, outline=False)


def candle(phase=0):
    """Candle on a little stand: white stick, a flame that flickers (2 frames)."""
    S = Spr()
    S.rect(-1, -10, 1, 0, 3)
    S.rect(-1, -18, 1, -11, 0)
    S.hline(-2, 2, -10, 3)
    if phase:
        S.rect(-1, -23, 0, -19, 0)
        S.px(0, -24, 1)
    else:
        S.rect(0, -22, 1, -19, 0)
        S.px(1, -23, 1)
    for y in range(-27, -15):
        for x in range(-5, 6):
            if (
                math.hypot(x, (y + 21) * 0.9) < 5
                and dither(x + phase, y, 0.2)
                and S.get(x, y) is None
            ):
                S.px(x, y, 1)
    return _c3done(S, outline=False)


def chainlink(axis="i"):
    """Chain-link fence (1 tile, h 16): dark posts, a diamond mesh dither, a top rail."""
    S = Spr()
    if axis == "i":
        b = Box(S, 1, 1, 0, 3, 7, 4, 0, 16, 2, 1, 1, edge=None)
        F = b.I
    else:
        b = Box(S, 1, 1, 3, 0, 4, 7, 0, 16, 2, 1, 1, edge=None)
        F = b.J
    for G in (b.J, b.I):
        for s in range(G.wid):
            for z in range(16):
                if (s + z) % 4 == 0 or (s - z) % 4 == 0:
                    G.px(s, z, 3)
                elif G.S.get(*G.at(s, z)) is not None:
                    pass
        G.hline(0, G.wid, 15, 3)
    for a in (0, 6):
        if axis == "i":
            Box(S, 1, 1, a, 3, a + 1, 4, 0, 17, 3, 3, 3, edge=3)
        else:
            Box(S, 1, 1, 3, a, 4, a + 1, 0, 17, 3, 3, 3, edge=3)
    # punch the mesh: keep only the wires (drop the light fill so the ground shows through)
    for y in range(S.H):
        for x in range(S.W):
            if S.mk[y][x] and S.lv[y][x] == 1:
                S.mk[y][x] = False
    return _c3done(S, outline=False)


def road_sign():
    """Road sign: a post with a white plate and a dark arrow."""
    S = Spr()
    S.rect(0, -16, 1, 0, 3)
    S.rect(-8, -26, 9, -16, 0)
    S.hline(-8, 9, -27, 3)
    S.hline(-8, 9, -15, 3)
    for y in range(-26, -15):
        S.px(-9, y, 3)
        S.px(10, y, 3)
    S.hline(-5, 4, -21, 3)
    for k in range(4):
        S.px(4 - k, -21 - k, 3)
        S.px(4 - k, -21 + k, 3)
    return _c3done(S, outline=False)


def neon_block(phase=0, sign=0):
    """City block (2 x 2, h 54): a dark tower with lit windows and a big neon sign on the +j face that blinks (2 frames)."""
    S = Spr()
    rnd = random.Random(sign * 7 + 1)
    b = Box(S, 2, 2, 0, 0, 14, 14, 0, 54, 3, 2, 3, edge=3)
    for F, lit in ((b.J, 0), (b.I, 1)):
        for z0 in range(28, 50, 7):
            for s0 in range(2, F.wid - 3, 5):
                if rnd.random() < 0.7:
                    F.rect(s0, s0 + 3, z0, z0 + 4, lit if rnd.random() < 0.6 else 2)
    J = b.J
    J.rect(3, J.wid - 3, 8, 22, 3)
    on = (phase + sign) % 2 == 0
    J.rect(4, J.wid - 4, 9, 21, 0 if on else 3)
    for k, s in enumerate(range(6, J.wid - 6, 4)):
        J.rect(s, s + 2, 12, 18, 3 if on else (1 if k % 2 == 0 else 3))
    return _c3done(S, fw=2, fh=2, outline=False)


def holo_screen(phase=0):
    """Holographic billboard on a pole: a light veil with moving scanlines and a dark logo."""
    S = Spr()
    S.rect(-1, -30, 0, 0, 3)
    S.rect(-14, -56, 13, -30, 0)
    for x in range(-15, 15):
        S.px(x, -57, 3)
        S.px(x, -29, 3)
    for y in range(-56, -29):
        S.px(-15, y, 3)
        S.px(14, y, 3)
        if (y + phase * 3) % 6 == 0:
            S.hline(-13, 12, y, 1)
    S.disc(0, -43, 6, 3)
    S.disc(0, -43, 3, 0)
    return _c3done(S, outline=False)


def car():
    """Parked car (1 x 2 along +i): low body, light cabin windows, dark wheels."""
    S = Spr()
    Box(S, 1, 2, 1, 1, 6, 13, 2, 9, 1, 2, 2, edge=3)
    c = Box(S, 1, 2, 1, 4, 6, 10, 9, 15, 1, 0, 1, edge=3)
    for F in (c.J, c.I):
        F.rect(1, F.wid - 1, 10, 14, 2)
    for a, bb in ((1, 3), (1, 11)):
        Box(S, 1, 2, a, bb, a + 1, bb + 1, 0, 3, 3, 3, 3, edge=3)
    return _c3done(S, fh=2, outline=False)


def vitrina():
    """Display case: a light glass box on a dark base with little goods inside."""
    S = Spr()
    Box(S, 1, 1, 1, 1, 6, 6, 0, 8, 2, 3, 3, edge=3)
    b = Box(S, 1, 1, 1, 1, 6, 6, 8, 18, 0, 0, 1, edge=3)
    for F in (b.J, b.I):
        F.rect(2, 4, 10, 13, 2)
        F.rect(6, 8, 10, 12, 3)
    return _c3done(S, outline=False)


def escalator():
    """Escalator (1 x 2): a rising band of steps between two dark rails."""
    S = Spr()
    for k in range(7):
        Box(
            S,
            1,
            2,
            1,
            2 * k,
            6,
            2 * k + 2,
            0,
            3 + 4 * (6 - k),
            0,
            0 if k % 2 else 1,
            1,
            edge=3,
        )
    for a in (0, 6):
        for k in range(7):
            Box(
                S, 1, 2, a, 2 * k, a + 1, 2 * k + 2, 0, 6 + 4 * (6 - k), 2, 2, 2, edge=3
            )
    return _c3done(S, fh=2, outline=False)


def slot_machine(phase=0):
    """Slot machine (1 tile, h 30): cabinet, a screen with three reels, a lever, a top light that blinks."""
    S = Spr()
    b = Box(S, 1, 1, 1, 2, 6, 6, 0, 26, 2, 1, 3, edge=3)
    J = b.J
    J.rect(1, J.wid - 1, 13, 21, 3)
    for k in range(3):
        J.rect(2 + k * 3, 4 + k * 3, 14, 20, 0)
        J.px(3 + k * 3, 17 - ((k + phase) % 3), 3)
    J.hline(1, J.wid - 1, 8, 3)
    for s in range(1, J.wid - 1, 2):
        J.px(s, 4, 0 if (s // 2 + phase) % 2 else 3)
    S.rect(-1, -33, 2, -29, 0 if phase else 3)
    S.line(9, -14, 12, -22, 3)
    S.disc(12, -23, 1, 3)
    return _c3done(S, outline=False)


def stool():
    S = Spr()
    S.rect(0, -6, 1, 0, 3)
    for x in range(-4, 6):
        S.px(x, -7, 3)
        S.px(x, -8, 1)
    return _c3done(S, outline=False)


def glass_wall(axis="i"):
    """Greenhouse wall: white glass panes in a dark frame, a few leaves pressed against the glass."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 3, 0, 0, edge=3)
    rnd = random.Random(3 if axis == "i" else 5)
    for F in (b.J, b.I):
        for s in range(0, F.wid, 7):
            F.vline(s, 0, WALL_H, 3)
        for z in (0, 12, 24, WALL_H - 1):
            F.hline(0, F.wid, z, 3)
        for _ in range(6):
            s, z = rnd.randrange(1, F.wid - 2), rnd.randrange(2, 12)
            F.rect(s, s + 2, z, z + 2, 2)
    return _c3done(S, outline=False)


def console():
    """Hideout console: a dark desk with a light veil and keys."""
    S = Spr()
    b = Box(S, 1, 1, 1, 1, 6, 6, 0, 10, 2, 3, 3, edge=3)
    s = Box(S, 1, 1, 2, 2, 5, 3, 10, 22, 3, 3, 3, edge=3)
    s.J.rect(1, s.J.wid - 1, 12, 20, 0)
    s.J.hline(2, s.J.wid - 2, 15, 2)
    s.J.hline(2, s.J.wid - 4, 18, 2)
    for k in range(1, 9, 2):
        b.top_px(2 + k, 8, 0)
    return _c3done(S, outline=False)


def gym_neon(name="gym_neon"):
    """Gimnasio Planta (C4a, not in the mock): the GREENHOUSE gym block of Metropolis Neon (3 x 2, an OPEN doorway in the middle tile): a stone footing, glass panes in a dark frame all round (a few leaves
    pressed against the glass), a glass lantern on the roof, and a plaque with a LEAF over the door. Flat white (opaque white -> level 1, no mask), like the C3 gym: the memory budget.
    """
    fw, fh = 3, 2
    S = Spr()
    WH = 34
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 1, 0, 1, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 6, 2)  # the stone footing
    I.rect(0, I.wid, 0, 6, 3)
    for sf, lvl in (
        (J, 0),
        (I, 1),
    ):  # glass panes: dark mullions every 7 px, bars at 7 / 18 / 29, panes white with a few leaves
        for s_ in range(0, sf.wid, 7):
            sf.vline(s_, 6, WH - 3, 3)
        for z_ in (6, 18, WH - 4):
            sf.hline(0, sf.wid, z_, 3)
        rnd = random.Random(11 + lvl)
        for _ in range(9):
            sx_, z_ = rnd.randrange(1, sf.wid - 3), rnd.randrange(8, WH - 8)
            sf.rect(sx_, sx_ + 2, z_, z_ + 1, 2)
    J.rect(0, J.wid, WH - 3, WH, 2)
    I.rect(0, I.wid, WH - 3, WH, 3)
    J.rect(15, 27, 0, 24, 3)  # the open doorway (middle tile), dark inside
    J.rect(17, 25, 0, 22, 3)
    for sx in range(17, 26):
        J.px(sx, 1 + (sx % 3 == 0), 0)
    J.rect(
        14, 29, 25, 31, 3
    )  # plaque above the door with a leaf (a lens shape with a midrib)
    J.rect(15, 28, 26, 30, 0)
    for sx, z in (
        (20, 30),
        (21, 30),
        (22, 30),
        (19, 29),
        (23, 29),
        (18, 28),
        (24, 28),
        (19, 27),
        (23, 27),
        (20, 26),
        (21, 26),
        (22, 26),
    ):
        J.px(sx, z, 3)  # the leaf: a lens outline with a midrib
    J.hline(19, 23, 28, 3)
    J.px(21, 29, 3)
    J.px(21, 27, 3)
    roof(S, fw, fh, WH, 3)
    rz = WH + 3
    lan = Box(
        S, fw, fh, 5, 4, 16, 10, rz, rz + 6, 0, 1, 2, edge=3
    )  # the glass lantern on the roof
    lan.J.vline(5, 0, 6, 3)
    S.flat_white(1)
    finish(name, S, "object", fw, fh)


def gym_dojo(name="gym_dojo", plaque="drop"):
    """Gimnasio Veneno (C5a, not in the mock): the DOJO of Ciudad Ambar (3 x 2, an OPEN doorway in the middle tile): a dark wooden body on a stone footing, two paper screens (white panes in a dark lattice) on each
    face, a swept double roof and a plaque with a poison DROP over the door. Flat white (opaque white -> level 1, no mask), like the other gyms: the memory budget.
    """
    fw, fh = 3, 2
    S = Spr()
    WH = 34
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 1, 2, 3, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 5, 3)  # the stone footing
    I.rect(0, I.wid, 0, 5, 3)
    for sf, panes in (
        (J, ((2, 12), (30, 40))),
        (I, ((2, 12), (16, 26))),
    ):  # paper screens: a white pane, a dark frame and lattice
        for a_, b_ in panes:
            sf.rect(a_ - 1, b_ + 1, 7, 27, 3)
            sf.rect(a_, b_, 8, 26, 0)
            sf.vline((a_ + b_) // 2, 8, 26, 3)
            sf.hline(a_, b_, 17, 3)
    J.rect(0, J.wid, WH - 5, WH, 3)  # the eaves' beam
    I.rect(0, I.wid, WH - 5, WH, 3)
    J.rect(15, 27, 0, 24, 3)  # the open doorway (middle tile), dark inside
    J.rect(17, 25, 0, 22, 3)
    for sx in range(17, 26):
        J.px(sx, 1 + (sx % 3 == 0), 0)
    J.rect(
        12, 31, 24, 33, 3
    )  # plaque above the door with a poison drop (C6: or a FIST, the Fighting dojo's `dojo_puno`)
    J.rect(13, 30, 25, 32, 0)
    if plaque == "fist":
        for sx in range(15, 28, 2):  # the plate over the door: three bars (a "belt")
            J.rect(sx, sx + 1, 26, 32, 3)
        J.rect(
            29, 42, 6, 29, 3
        )  # the banner on the right wall: a black cloth with a WHITE FIST
        J.rect(31, 40, 8, 27, 3)
        for sx in (32, 34, 36, 38):  # four knuckles
            J.rect(sx, sx + 2, 22, 24, 0)
        J.rect(32, 40, 16, 22, 0)  # the clenched hand
        for sx in (34, 36, 38):  # finger lines
            J.vline(sx, 17, 22, 3)
        J.rect(30, 33, 14, 18, 0)  # the thumb
        J.rect(33, 38, 11, 15, 0)  # the wrist
    else:
        for z, hw in ((32, 0), (31, 1), (30, 2), (29, 3), (28, 3), (27, 3), (26, 2)):
            J.rect(21 - hw, 22 + hw, z, z + 1, 3)
        J.px(19, 28, 0)
    r = roof(S, fw, fh, WH, 4)
    rz = WH + 4
    Box(
        S, fw, fh, 3, 2, 18, 12, rz, rz + 5, 1, 2, 3, edge=3
    )  # the upper tier of the roof
    Box(S, fw, fh, 6, 4, 15, 10, rz + 5, rz + 8, 1, 3, 3, edge=3)
    S.flat_white(1)
    finish(name, S, "object", fw, fh)


def gym_mente(name="gym_mente"):
    """Gimnasio Mente (C6, not in the mock): the psychic gym of Ciudad Cenit (3 x 2, an OPEN doorway in the middle tile): a pale glass block (fine mullions, a wide band of dark glass under the eaves), a plaque with an
    EYE over the door and a thin floating RING over the roof (the city's motif: the plaza's inlay, the orbs). Flat white like the other gyms (the memory budget).
    """
    fw, fh = 3, 2
    S = Spr()
    WH = 34
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 1, 0, 1, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 5, 2)  # the footing
    I.rect(0, I.wid, 0, 5, 3)
    for sf in (J, I):  # fine glass grid
        for s_ in range(0, sf.wid, 6):
            sf.vline(s_, 5, WH - 6, 2)
        for z_ in range(5, WH - 5, 8):
            sf.hline(0, sf.wid, z_, 2)
        sf.rect(0, sf.wid, WH - 6, WH, 3)  # the dark glass band under the eaves
        sf.hline(0, sf.wid, WH - 7, 0)
    J.rect(15, 27, 0, 24, 3)  # the open doorway (middle tile), dark inside
    J.rect(17, 25, 0, 22, 3)
    for sx in range(17, 26):
        J.px(sx, 1 + (sx % 3 == 0), 0)
    J.rect(13, 30, 25, 32, 3)  # the plaque: an eye (almond with an iris)
    J.rect(14, 29, 26, 31, 0)
    for sx in range(15, 28):  # the lids: an almond (two arcs meeting at the corners)
        d = abs(sx - 21)
        top = 30 - (d > 3) - (d > 5)
        bot = 27 + (d > 3) + (d > 5)
        J.px(sx, top, 3)
        J.px(sx, bot, 3)
    J.rect(19, 24, 27, 31, 3)  # the iris
    J.rect(20, 23, 28, 30, 0)
    J.rect(21, 22, 28, 30, 3)  # the pupil
    roof(S, fw, fh, WH, 3)
    rz = WH + 3
    for k in range(
        0, 360, 6
    ):  # the floating ring over the roof (a flat ellipse in the iso view)
        a = math.radians(k)
        S.px(int(round(15 * math.cos(a))), -(rz + 9) + int(round(7 * math.sin(a))), 3)
    S.flat_white(1)
    finish(name, S, "object", fw, fh)


def torre_vertice(name="torre_vertice"):
    """Torre Vertice (C6, not in the mock): a corporation's tall tower of Ciudad Cenit (3 x 2, an OPEN doorway in the middle tile under a dark canopy): a pale glass curtain wall (mullions every
    5 px, a floor line every 7), the company's big V on the front, a dark crown with an antenna. Flat white (the memory budget).
    """
    fw, fh = 3, 2
    S = Spr()
    WH = 50
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 1, 0, 1, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 7, 3)  # the dark base
    I.rect(0, I.wid, 0, 7, 3)
    for sf, lv in ((J, 2), (I, 3)):  # the curtain wall
        for s_ in range(0, sf.wid, 5):
            sf.vline(s_, 7, WH - 4, lv)
        for z_ in range(14, WH - 4, 7):
            sf.hline(0, sf.wid, z_, lv)
    J.rect(0, J.wid, WH - 4, WH, 3)  # the dark crown band
    I.rect(0, I.wid, WH - 4, WH, 3)
    J.rect(15, 27, 0, 24, 3)  # the open doorway (middle tile), dark inside
    J.rect(17, 25, 0, 22, 3)
    for sx in range(17, 26):
        J.px(sx, 1 + (sx % 3 == 0), 0)
    J.rect(11, 31, 24, 28, 3)  # the canopy over the door
    J.rect(12, 30, 25, 27, 2)
    for k in range(
        11
    ):  # the big V of the company on the front wall: two thick strokes, symmetric on the SCREEN (the face slopes: the arms have different z slopes)
        for t in range(2):
            J.px(11 + k, 30 + (10 - k) // 2 + t, 3)  # left arm: down to the right
            J.px(21 + k, 30 + (3 * k) // 2 + t, 3)  # right arm: up to the right
    roof(S, fw, fh, WH, 3)
    rz = WH + 3
    Box(S, fw, fh, 8, 5, 13, 9, rz, rz + 6, 1, 2, 3, edge=3)  # a small dark crown block
    S.rect(0, -(rz + 6) - 12, 1, -(rz + 6), 3)  # the antenna mast
    S.px(0, -(rz + 6) - 13, 3)
    S.flat_white(1)
    finish(name, S, "object", fw, fh)


def c4_pieces():
    def put(name, spec):
        S, kind, fw, fh, tile, outline = spec
        finish(name, S, kind, fw, fh, tile, outline)

    # ground
    for a in (0, 1):
        for b in (0, 1):
            put(
                "oflag_%d%d" % (a, b), old_flags(a, b)
            )  # Pueblo Bruma: big worn flagstones, 2 x 2 per slab
            put(
                "polished_%d%d" % (a, b), polished(a, b)
            )  # the department store: glossy slabs
            put("oplank_%d%d" % (a, b), old_planks(a, b))  # the tower: old boards
    put(
        "asph_a", asphalt(0, False)
    )  # Metropolis Neon / Ruta 7: asphalt, and with the dashed lane line
    put("asph_b", asphalt(1, True))
    put("ccarpet", casino_carpet(0))  # the casino
    put("soil_a", soil_path(0))  # the plant gym: soil paths
    put("soil_b", soil_path(1))
    for d_, nm in (
        ("i+", "se"),
        ("i-", "nw"),
        ("j+", "sw"),
        ("j-", "ne"),
    ):  # the hideout's arrow floors (big steel plates, a bold arrow in every tile): `slide` in the catalog
        for a in (0, 1):
            for b in (0, 1):
                put("arr_%s_%d%d" % (nm, a, b), arrow_tile(d_, a, b))
    # objects
    for ax_ in ("i", "j"):
        for s in range(2):
            put(
                "stonewall_%s_%s" % (ax_, "ab"[s]), stonewall(ax_, s)
            )  # Ruta 9: dry-stone walls
        put("chain_" + ax_, chainlink(ax_))  # Ruta 7 / 8: chain-link fences
        put("glass_" + ax_, glass_wall(ax_))  # the plant gym: glass walls
    put("tower_far", tower_far())
    put("tower_far_f", _flat_spec(tower_far()))
    for s in range(2):
        put("stele_" + "ab"[s], stele(s))
    for ph in range(2):
        put("candle_%d" % ph, candle(ph))
        put("slot_%d" % ph, slot_machine(ph))
    unify_frames(("candle_0", "candle_1"))
    unify_frames(("slot_0", "slot_1"))
    put("road_sign", road_sign())
    for k in range(2):
        for ph in range(2):
            put("nblock_%d_%d" % (k, ph), neon_block(ph, k))
        unify_frames(("nblock_%d_0" % k, "nblock_%d_1" % k))
    for ph in range(3):
        put("holo_%d" % ph, holo_screen(ph))
    unify_frames(("holo_0", "holo_1", "holo_2"))
    # flat-white versions (opaque white -> light gray, no mask: the memory budget of the city maps): the catalog's `nblock_0` / `holo` point at these, `nblock_1` at the single static frame
    for ph in range(2):
        put("nblock_f0_%d" % ph, _flat_spec(neon_block(ph, 0)))
        put("holo_f_%d" % ph, _flat_spec(holo_screen(ph)))
    put("nblock_f1_0", _flat_spec(neon_block(0, 1)))
    unify_frames(("nblock_f0_0", "nblock_f0_1"))
    unify_frames(("holo_f_0", "holo_f_1"))
    put("car", car())
    put("vitrina", vitrina())
    put("escalator", escalator())
    put("stool", stool())
    put("console", console())
    gym_neon()


# ---------------------------------------------------------------- C4b (contract 41): the casino's poster wall and the tower's ghost-dust boards
def wall_poster(name, on):
    """A wall piece (wall_j on the back-right, wall_i on the back-left) with a Vertice poster on its lit face: a dark frame, white paper, a bold V (the mark of the villains), a torn corner."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 0, 0, 1, edge=3)
    wall_deco(b, None)
    F = b.J if on == "j" else b.I
    F.rect(2, 12, 9, 31, 3)
    F.rect(3, 11, 10, 30, 0)
    for k in range(13):
        sl, sr = 6 - k // 5, 7 + k // 5
        z = 14 + k
        F.px(sl, z, 3)
        F.px(sr, z, 3)
        if k % 2 == 0:
            F.px(sl + 1 if sl < 6 else sl, z, 3)
    F.hline(4, 10, 28, 1)  # a line of small print
    F.hline(4, 8, 26, 1)
    F.px(10, 10, 1)  # the torn corner
    F.px(9, 10, 1)
    F.px(10, 11, 1)
    finish(name, S, "object", 1, 1)


def old_planks_dust(pi, pj, seed=0):
    """The tower's floor where the ghosts walk (the encounter tile): the same boards as old_planks with a patch of grey dust and a few dark specks in the middle of the tile."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        base = (
            1
            if int((v + pj) * 7) != int((v + pj) * 7 - 0.03)
            else (2 if r.random() < 0.02 else 0)
        )
        if (u - 0.5) ** 2 + (v - 0.5) ** 2 < 0.2:
            if dither(x, y, 0.5):
                base = max(base, 1)
            if r.random() < 0.07:
                base = 3
        return base

    return _c3T(gtile(f, seed))


def arrow_plate(f):
    """The 8 arrow floors of contract 35.3 (arrow_<facing>) in the approved look of the hideout's plates (c4_mock): the plate's dither with a bold arrow along the facing f (0..7 = S SE E NE N NW W SW).
    The world-axis facings (SE NW SW NE) are drawn exactly like arrow_tile(); the screen-cardinal ones (S E N W) use the same arrow turned onto the tile's diagonal.
    """
    di, dj = (1, 1, 1, 0, -1, -1, -1, 0)[f], (1, 0, -1, -1, -1, 0, 1, 1)[f]
    n = math.hypot(di, dj)
    ux, uy = di / n, dj / n
    L = (
        1.0 if (di == 0 or dj == 0) else 1.3
    )  # (a diagonal run crosses the tile's square corner to corner: the arrow is a little shorter in the square)

    def fn(x, y, u, v, r):
        a = 0.5 + ((u - 0.5) * ux + (v - 0.5) * uy) / L
        b = 0.5 + (-(u - 0.5) * uy + (v - 0.5) * ux) / L
        if 0.42 < b < 0.58 and 0.2 < a < 0.6:
            return 3
        if 0.6 <= a < 0.85 and abs(b - 0.5) < (0.85 - a) * 0.9:
            return 3
        return 1 if dither(x, y, 0.35) else 0

    return _c3T(gtile(fn, 0))


def c4b_pieces():
    for f_, nm_ in enumerate(
        ("s", "se", "e", "ne", "n", "nw", "w", "sw")
    ):  # contract 35.3's arrow floors: the placeholder arrows are replaced by the approved plate look (gen_arrow_tiles.py is locked out)
        S, kind, fw, fh, tile, outline = arrow_plate(f_)
        finish("arrow_" + nm_, S, kind, fw, fh, tile, outline)
    wall_poster("wall_poster_j", "j")
    wall_poster("wall_poster_i", "i")
    for a in (0, 1):
        for b in (0, 1):
            S, kind, fw, fh, tile, outline = old_planks_dust(a, b, 3 + a + 2 * b)
            finish("oplank_d_%d%d" % (a, b), S, kind, fw, fh, tile, outline)


# ---------------------------------------------------------------- build everything
# ================================================================ Chapter 5 (C5a, contract 47): Ruta 12 .. Ruta 15, Ciudad Ambar, the poison gym (+ the C5b zones: Autopista, Ruta 18, the Reserva, the sea)
# The drawings of tools/c5_mock.py (approved by the user zone by zone; the Autopista as the light elevated road) moved here UNCHANGED; the mock now loads the generated files.
def wildflowers(seed=0):
    """Meadow: white with sparse soft gray dither and little 3-dot flowers."""
    rnd = random.Random(seed)
    S = gtile(lambda x, y, u, v, r: 1 if dither(x, y, 0.15) else 0, seed)
    marks(S, rnd, 3, 2, shapes=(((0, 0), (1, 0), (0, 1), (-1, 0), (0, -1)),))
    marks(S, rnd, 3, 3, shapes=(((0, 0),),))
    return _c3T(S)


def savanna(seed=0):
    """Savanna ground: light dry earth (white with gray dither patches) and a few dry blade marks."""
    rnd = random.Random(seed)
    bl = blob_field(rnd, 3, 7, 3)
    S = gtile(
        lambda x, y, u, v, r: 1 if in_blobs(x, y, bl) and dither(x, y, 0.35) else 0,
        seed,
    )
    marks(S, rnd, 4, 2, shapes=(((0, 0), (1, -1)), ((0, 0), (-1, -1))))
    return _c3T(S)


def dry_grass(seed=0):
    """Savanna tall grass (encounters): light tufts of long dry blades, airy, not dark."""
    S = tile_canvas()
    rnd = random.Random(seed)
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y, 7) and dither(x, y, 0.2):
                S.t(x, y, 1)
    for _ in range(7):
        x, y = rnd.randrange(4, 24), rnd.randrange(5, 14)
        for k in range(5):
            for dx in (-1, 1):
                if in_diamond(x + dx * (k // 2), y - k, 7):
                    S.t(x + dx * (k // 2), y - k, 2 if k < 4 else 3)
    return _c3T(S)


def tatami(pi, pj, seed=0):
    """Poison gym floor: BIG light tatami mats (2 x 2 tiles), a dark border only on the mat's outer edges, fine straw lines along +i."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.04) or (pj == 0 and v < 0.04):
            return 3
        if int((v + pj) * 10) != int((v + pj) * 10 - 0.03):
            return 1
        return 0

    return _c3T(gtile(f, seed))


def boardwalk(seed=0):
    """Ruta 18 promenade: light planks along +j, a seam every few px (plain, light)."""

    def f(x, y, u, v, r):
        if int(u * 5) != int(u * 5 - 0.03):
            return 2
        return 1 if r.random() < 0.03 else 0

    return _c3T(gtile(f, seed))


# ================================================================ objects
def windmill(phase=0):
    """Field windmill (1 tile, tall): a white tapering tower, a dark cap and four sails turning (3 frames)."""
    S = Spr()
    for y in range(0, 40):
        w = 7 - (y * 3) // 40
        for x in range(-w, w + 1):
            S.px(x, -y, 0 if x < 2 else 1)
        S.px(-w, -y, 3)
        S.px(w, -y, 3)
    S.rect(-2, -10, 1, -1, 3)
    for y in range(40, 47):
        w = 6 - (y - 40)
        S.hline(-w, w, -y, 2)
    cx, cy = 0, -42
    for k in range(4):
        a = phase * (math.pi / 6) + k * math.pi / 2
        p0 = (cx + int(6 * math.cos(a)), cy + int(6 * math.sin(a) * 0.9))
        p1 = (cx + int(22 * math.cos(a)), cy + int(22 * math.sin(a) * 0.9))
        o = (
            int(5 * math.cos(a + math.pi / 2)),
            int(5 * math.sin(a + math.pi / 2) * 0.9),
        )
        quad = [p0, p1, (p1[0] + o[0], p1[1] + o[1]), (p0[0] + o[0], p0[1] + o[1])]
        S.poly(quad, 0)  # the white canvas sail
        for (xa, ya), (xb, yb) in zip(quad, quad[1:] + quad[:1]):
            S.line(xa, ya, xb, yb, 3)
        S.line(cx, cy, p1[0], p1[1], 3)  # the spar
    S.disc(cx, cy, 2, 3)
    return _c3done(S, outline=False)


def palm(seed=0):
    """Palm tree: a curved ringed trunk and a crown of drooping fronds."""
    S = Spr()
    rnd = random.Random(seed)
    pts = [(int(4 * math.sin(t / 14.0)), -t) for t in range(0, 44)]
    for k, (x, y) in enumerate(pts):
        S.hline(x - 2, x + 2, y, 2 if k % 4 else 3)
        S.px(x - 2, y, 3)
        S.px(x + 2, y, 3)
    tx, ty = pts[-1]
    for k in range(7):
        a = -math.pi / 2 + (k - 3) * 0.55
        for t in range(0, 20):
            x = tx + int(t * math.cos(a) * 1.1)
            y = ty + int(t * math.sin(a) * 0.6 + (t * t) / 28.0)
            S.px(x, y, 3)
            S.px(x, y + 1, 1)
            if t % 3 == 0:
                S.px(x + (1 if k > 3 else -1), y + 2, 2)
    S.disc(tx, ty, 3, 2)
    return _c3done(S, outline=False)


def reserve_arch():
    """The Reserva gate arch (2 tiles wide): two log posts and a beam with a hanging sign."""
    S = Spr()
    for a in (0, 12):
        Box(S, 2, 1, a, 2, a + 2, 5, 0, 34, 1, 2, 2, edge=3)
    b = Box(S, 2, 1, 0, 2, 14, 5, 34, 39, 1, 2, 2, edge=3)
    for sx in range(0, b.J.wid, 4):
        b.J.vline(sx, 34, 39, 3)
    s = Box(S, 2, 1, 4, 4, 10, 5, 24, 32, 0, 0, 1, edge=3)
    s.J.hline(2, s.J.wid - 2, 28, 3)
    return _c3done(S, fw=2, fh=1, outline=False)


def acacia(seed=0):
    """Savanna acacia: a thin forked trunk and a wide flat dark canopy."""
    S = Spr()
    rnd = random.Random(seed)
    S.line(0, 0, -2, -22, 3)
    S.line(-1, -14, 6, -26, 3)
    S.line(-2, -22, -7, -27, 3)
    for y in range(-34, -24):
        w = 18 - abs(y + 29) * 2
        for x in range(-w, w + 1):
            if rnd.random() < 0.85:
                S.px(x, y, 2 if (x + y) % 5 else 3)
    for x in range(-18, 19):
        S.px(x, -24 - (1 if abs(x) < 14 else 0), 3)
    return _c3done(S, outline=False)


def shoji(axis="i"):
    """Gym wall: light paper screen panels in a dark wooden lattice."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 3, 0, 1, edge=3)
    for F in (b.J, b.I):
        for s in range(0, F.wid, 5):
            F.vline(s, 0, WALL_H, 3)
        for z in range(0, WALL_H, 7):
            F.hline(0, F.wid, z, 3)
        F.rect(0, F.wid, 0, 3, 3)
    return _c3done(S, outline=False)


def graffiti_rail():
    """Autopista barrier (1 tile, along +i): a concrete low wall with a neon tag."""
    S = Spr()
    b = Box(S, 1, 1, 0, 4, 7, 6, 0, 12, 1, 0, 1, edge=3)
    J = b.J
    rnd = random.Random(5)
    for k in range(10):
        s = 1 + k
        J.px(s, 4 + int(3 * math.sin(k * 0.9)), 3)
        J.px(s, 5 + int(3 * math.sin(k * 0.9)), 2)
    return _c3done(S, outline=False)


def road_light(seed=0, mark=None):
    """Autopista pavement, LIGHT (the dark asphalt read poorly): light gray dither, a dashed white lane line along +i; mark = 'arrow' paints a
    big dark downhill chevron (pointing +j, the downhill direction)."""

    def f(x, y, u, v, r):
        if mark == "lane" and 0.42 < v < 0.58 and int(u * 3) % 2 == 0:
            return 3
        if mark == "arrow":
            a = abs(u - 0.5)
            if 0.18 < v - a * 1.1 < 0.4 and a < 0.42:
                return 3
        return 1 if dither(x, y, 0.45) else 0

    return _c3T(gtile(f, seed))


def guardrail():
    """White highway guardrail (1 tile along +i): a bright double rail on dark posts."""
    S = Spr()
    for a in (1, 5):
        Box(S, 1, 1, a, 5, a + 1, 6, 0, 11, 3, 3, 3, edge=3)
    b = Box(S, 1, 1, 0, 5, 7, 6, 6, 11, 0, 0, 1, edge=3)
    b.J.hline(0, b.J.wid, 8, 2)
    return _c3done(S, outline=False)


def downhill_sign():
    """Downhill road sign: a triangle plate with a slope mark."""
    S = Spr()
    S.rect(0, -14, 1, 0, 3)
    for y in range(0, 12):
        w = y
        S.hline(-w, w + 1, -26 + y, 0)
        S.px(-w, -26 + y, 3)
        S.px(w + 1, -26 + y, 3)
    S.hline(-11, 12, -14, 3)
    S.line(-6, -16, 6, -22, 3)
    return _c3done(S, outline=False)


def road_slope(f):
    """The Autopista's bike slope (`slide_bike` in the catalog): the light pavement of road_light with a bold dark chevron that points DOWNHILL along the facing f (0..7 = S SE E NE N NW W SW),
    the arrow_plate() geometry on the road's dither. The world-axis ones (SE NW SW NE) lie along the road; the screen-cardinal ones use the same chevron turned onto the tile's diagonal.
    """
    di, dj = (1, 1, 1, 0, -1, -1, -1, 0)[f], (1, 0, -1, -1, -1, 0, 1, 1)[f]
    n = math.hypot(di, dj)
    ux, uy = di / n, dj / n
    L = 1.0 if (di == 0 or dj == 0) else 1.3

    def fn(x, y, u, v, r):
        a = 0.5 + ((u - 0.5) * ux + (v - 0.5) * uy) / L
        b = 0.5 + (-(u - 0.5) * uy + (v - 0.5) * ux) / L
        if (
            0.2 < a < 0.62 and abs(abs(b - 0.5) * 1.1 - (0.62 - a)) < 0.1
        ):  # the chevron: its tip leads along the facing
            return 3
        return 1 if dither(x, y, 0.45) else 0

    return _c3T(gtile(fn, 2))


def c5_pieces():
    def put(name, spec):
        S, kind, fw, fh, tile, outline = spec
        finish(name, S, kind, fw, fh, tile, outline)

    # ground
    for s in range(3):
        put("wildf_%d" % s, wildflowers(s))  # Rutas 13 - 15: the wildflower meadow
        put("sav_%d" % s, savanna(s))  # the Reserva: dry savanna earth
    for s in range(2):
        put("dgrass_%d" % s, dry_grass(s))  # the Reserva: dry tall grass (encounters)
        put("roadl_%d" % s, road_light(s))  # the Autopista: light pavement
    for a in (0, 1):
        for b in (0, 1):
            put(
                "tatami_%d%d" % (a, b), tatami(a, b)
            )  # the poison gym: BIG light tatami mats, 2 x 2 tiles
    put("bwalk", boardwalk(0))  # Ruta 18: the promenade planks
    put("road_lane", road_light(0, "lane"))  # the Autopista: dashed lane line
    put(
        "road_arrow", road_light(0, "arrow")
    )  # the Autopista: a painted downhill chevron (plain floor)
    for f_, nm_ in enumerate(("s", "se", "e", "ne", "n", "nw", "w", "sw")):
        put(
            "slope_%s" % nm_, road_slope(f_)
        )  # the Autopista's bike slopes (`slide_bike` in the catalog)
    # objects
    for ph in range(3):
        put("windmill_%d" % ph, windmill(ph))  # Ruta 15: field windmills (3 frames)
    unify_frames(("windmill_0", "windmill_1", "windmill_2"))
    for s in range(2):
        put("palm_%d" % s, palm(s))  # Ambar / Ruta 18
        put("acacia_%d" % s, acacia(s))  # the Reserva
    put("res_arch", reserve_arch())  # Ambar: the Reserva's wooden arch (2 x 1)
    put("shoji_i", shoji("i"))  # the poison gym's paper screens
    put("shoji_j", shoji("j"))
    put("graffiti_rail", graffiti_rail())  # the Autopista's tagged concrete rail
    put("guardrail", guardrail())  # the Autopista's white guardrail
    put("downhill_sign", downhill_sign())  # the Autopista's downhill sign
    gym_dojo()  # Ambar: the poison gym's dojo (3 x 2, flat white)


# ================================================================ Chapter 6 (C6, contract 46): the teleport pad and the card-key door
# The drawings of tools/c6_mock.py (approved by the user) moved here UNCHANGED; the mock keeps its own copies.
def c6_pad(phase=0):
    """Teleport pad (flat tile): a light disc ring with a pulsing center (2 frames: pad_0 / pad_1, an animated tile of the catalog)."""

    def f(x, y, u, v, r):
        d = math.hypot(u - 0.5, v - 0.5)
        if d < 0.13:
            return 0 if phase else 2
        if 0.28 < d < 0.36:
            return 3
        if d < 0.28:
            return 1 if phase else 0
        return 0 if dither(x, y, 0.85) else 1

    return _c3T(gtile(f, phase))


def c6_keydoor(locked=True):
    """Card-key door (an ACTOR, legend `keydoor=<item>`): a dark sliding door in a light frame, a small reader with a light (dark = locked)."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 0, 1, 1, edge=3)
    b.J.rect(3, b.J.wid - 3, 0, 26, 3 if locked else 1)
    b.J.vline(b.J.wid // 2, 0, 26, 2)
    b.J.rect(b.J.wid - 3, b.J.wid - 1, 14, 18, 3)
    b.J.px(b.J.wid - 2, 16, 0 if not locked else 2)
    return S


# Chapter 6 content (contract 49): the rest of the drawings of tools/c6_mock.py (approved by the user), moved here UNCHANGED (the mock now loads the generated files). Ground slabs are BIG (2 x 2 tiles:
# the four quarter tiles <name>_<i%2><j%2> follow the parity of the cell), interior floors and paved streets always LIGHT. (The C8 earth gym's sand slabs are NOT here: them, `c8_pieces`.)
def c6_ring(pi, pj, seed=0):
    """Cenit plaza / the psychic gym's floor: big pale stone slabs with a faint concentric-ring inlay crossing them (the psychic city's motif)."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        d = math.hypot((u + pi) - 1.0, (v + pj) - 1.0)
        if abs(d % 0.5 - 0.25) < 0.025:
            return 1
        return 0

    return _c3T(gtile(f, seed))


def c6_carpet(pi, pj, seed=0):
    """Torre Vertice office floor: big light carpet tiles (2x2) with a thin gray border."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        return 1 if dither(x, y, 0.15) else 0

    return _c3T(gtile(f, seed))


def c6_boards(pi, pj, seed=0):
    """Dojo: big light wooden boards (2x2 block), seams along +i, dark edge only at the block border."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 3
        if int((v + pj) * 6) != int((v + pj) * 6 - 0.03):
            return 1
        return 0

    return _c3T(gtile(f, seed))


def c6_glass_tower(seed=0):
    """Cenit tower (2 x 2, tall): pale glass curtain wall with a grid, a ring emblem near the top."""
    S = Spr()
    b = Box(S, 2, 2, 1, 1, 13, 13, 0, 62, 1, 0, 1, edge=3)
    for F in (b.J, b.I):
        for s in range(0, F.wid, 4):
            F.vline(s, 0, 62, 2)
        for z in range(0, 62, 6):
            F.hline(0, F.wid, z, 2)
    cs = b.J.wid // 2
    for s in range(b.J.wid):
        for z in range(44, 58):
            d = math.hypot(s - cs, (z - 51) * 1.2)
            if 4 < d < 6.2:
                b.J.px(s, z, 3)
    return _c3done(S, fw=2, fh=2, outline=False)


def c6_orb(phase=0):
    """Floating psychic orb on a slim stand (2 frames: it bobs)."""
    S = Spr()
    S.rect(0, -12, 1, 0, 3)
    S.hline(-3, 4, 0, 3)
    cy = -20 - phase
    S.disc(0, cy, 5, 0, ring=3)
    S.px(-2, cy - 2, 1)
    return _c3done(S, outline=False)


def c6_desk():
    """Office desk with a monitor."""
    S = Spr()
    Box(S, 1, 1, 0, 1, 7, 6, 0, 10, 1, 2, 2, edge=3)
    m = Box(S, 1, 1, 2, 2, 5, 3, 10, 20, 3, 3, 3, edge=3)
    m.J.rect(1, m.J.wid - 1, 12, 19, 0)
    return _c3done(S, outline=False)


def c6_dummy():
    """Training dummy: a post with a padded straw body and cross-arm."""
    S = Spr()
    S.rect(0, -10, 1, 0, 3)
    for y in range(-26, -10):
        w = 4 if y > -22 else 3
        S.hline(-w, w + 1, y, 1 if y % 4 else 2)
        S.px(-w, y, 3)
        S.px(w + 1, y, 3)
    S.hline(-8, 9, -20, 3)
    S.disc(0, -29, 3, 1, ring=3)
    return _c3done(S, outline=False)


def c6_scroll_wall(axis="i"):
    """Dojo wall: light plaster with dark wooden posts and a hanging calligraphy scroll."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 3, 0, 1, edge=3)
    for F in (b.J, b.I):
        F.vline(0, 0, WALL_H, 3)
        F.vline(F.wid - 1, 0, WALL_H, 3)
        F.rect(0, F.wid, 0, 3, 3)
    b.J.rect(5, 9, 10, 30, 0)
    for z in range(12, 28, 3):
        b.J.hline(6, 8, z, 3)
    return _c3done(S, outline=False)


def c6_pieces():
    for ph in (0, 1):
        S, kind, fw, fh, tile, outline = c6_pad(ph)
        finish("pad_%d" % ph, S, kind, fw, fh, tile, outline)
    _actor_write("keydoor", {"south": c6_keydoor(True)})
    # ground (BIG slabs: quarter tiles by cell parity)
    for a in (0, 1):
        for b in (0, 1):
            for nm, fn in (
                ("ring", c6_ring),
                ("carpet", c6_carpet),
                ("boards", c6_boards),
            ):
                S, kind, fw, fh, tile, outline = fn(a, b)
                finish("%s_%d%d" % (nm, a, b), S, kind, fw, fh, tile, outline)
    # objects
    for nm, spec in (
        ("glass_tower", c6_glass_tower()),
        ("orb_0", c6_orb(0)),
        ("orb_1", c6_orb(1)),
        ("desk", c6_desk()),
        ("dummy", c6_dummy()),
        ("scroll_wall", c6_scroll_wall()),
    ):
        S, kind, fw, fh, tile, outline = spec
        finish(nm, S, kind, fw, fh, tile, outline)
    unify_frames(
        ("orb_0", "orb_1")
    )  # the orb's two frames share one box (contract 22.2)
    gym_mente()  # Cenit: the psychic gym (3 x 2, flat white, not in the mock)
    gym_dojo(
        "dojo_puno", plaque="fist"
    )  # Cenit: the Fighting dojo (the Ambar dojo's block with a fist on the plaque)
    torre_vertice()  # Cenit: the Torre Vertice (3 x 2, tall, flat white)


# ================================================================ Chapter 8 (C8, contract 51): the Ground gym's sandstone
# The drawing of tools/c6_mock.py `sand_slab` (the approved TIERRA scene) moved here UNCHANGED (pixel for pixel; the mock now loads the generated files). Its other pieces (wall_i / wall_j,
# rock_code, boulder, pillar) are old catalog objects.
def sand_slab(pi, pj, seed=0):
    """Ground gym: big sandstone slabs (2 x 2 tiles), light with sparse grit, a dark border only on the slab's outer edges."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.035) or (pj == 0 and v < 0.035):
            return 2
        return 1 if r.random() < 0.05 else 0

    return _c3T(gtile(f, seed))


def c8_pieces():
    for a in (0, 1):
        for b in (0, 1):
            S, kind, fw, fh, tile, outline = sand_slab(a, b)
            finish(
                "sslab_%d%d" % (a, b), S, kind, fw, fh, tile, outline
            )  # the Ground gym: BIG light sandstone slabs, 2 x 2 tiles


# ================================================================ Chapter 7 (C7, contract 53): Routes 19 - 21, the Islas Escarcha, Isla Ceniza, the mansion, the fossil lab and the Power Plant
# The drawings of tools/c7_mock.py (the approved MAR / ESCARCHA / CENIZA / FUEGO / MANSION / LAB / CENTRAL scenes) moved here UNCHANGED, pixel for pixel (the mock now loads the generated files). Floors are BIG
# light slabs (the four quarter tiles <name>_<i%2><j%2> follow the parity of the cell); every zone has its own ground, border and 2 - 4 signature props.
# ---------------------------------------------------------------- ground tiles
def c7_ice_slab(pi, pj, seed=0):
    """Islas Escarcha floor: big pale ice slabs (2x2), a fine frost dither, thin dark crack lines that run across the block."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        U, V = u + pi, v + pj
        if (
            abs((U - 0.3) - 0.7 * (V - 0.2)) < 0.025 and 0.4 < V < 1.6
        ):  # one long hairline crack per block
            return 2
        return 1 if dither(x, y, 0.08) else 0

    return _c3T(gtile(f, seed))


def c7_ash(seed=0):
    """Isla Ceniza ground: pale volcanic ash, sparse dark pumice grains, a few light streaks (wind-blown)."""

    def f(x, y, u, v, r):
        k = r.random()
        if k < 0.035:
            return 3
        if k < 0.10:
            return 1
        return 0

    return _c3T(gtile(f, seed))


def c7_burnt_boards(pi, pj, seed=0):
    """Mansion floor: big LIGHT boards along +i (2x2 block), seams, and soft scorch patches (dither) that do not darken the whole tile."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        U, V = u + pi, v + pj
        if int(V * 6) != int(V * 6 - 0.03):
            return 1
        d = math.hypot(U - (0.6 + 0.6 * ((seed + pi) % 2)), V - (1.2 - 0.5 * pj))
        if d < 0.35:
            return 2 if dither(x, y, 0.35) else 1
        if d < 0.5:
            return 1 if dither(x, y, 0.4) else 0
        return 0

    return _c3T(gtile(f, seed))


def c7_current(phase=0):
    """Escarcha current: dark water with light flow streaks pointing SE (2 frames: the streaks advance)."""

    def f(x, y, u, v, r):
        s = (u * 6 + v * 2 + phase * 0.5) % 1.0
        if s < 0.12 and dither(x, y, 0.8):
            return 0
        if s < 0.22:
            return 1
        return 2

    return _c3T(gtile(f, phase))


# ---------------------------------------------------------------- objects
def c7_kelp(phase=0):
    """A kelp bed on the water: three dark fronds waving (2 frames)."""
    S = Spr()
    for k, dx in enumerate((-6, 0, 6)):
        sway = 1 if (phase + k) % 2 else -1
        for t in range(10):
            S.px(dx + (sway if t > 5 else 0), -t, 3)
            if t % 3 == 1:
                S.px(dx + (sway if t > 5 else 0) + 1, -t, 2)
    return _c3done(S, outline=False)


def c7_sandbar():
    """A small sand islet with a tuft of beach grass (flat, sits on the water)."""
    S = Spr()
    for y in range(-4, 3):
        w = 12 - abs(y) * 2
        S.hline(-w, w + 1, y, 0)
        S.px(-w, y, 2)
        S.px(w, y, 2)
    S.hline(-12, 13, 3, 2)
    for t in range(6):
        S.px(-2 + (t % 3), -4 - t, 3)
        S.px(2 - (t % 2), -4 - t, 3)
    return _c3done(S, outline=False)


def c7_ice_pillar(seed=0):
    """Ice stalagmite: a tall pale crystal column with dark facet edges and a light glint."""
    S = Spr()
    h = 30 + seed * 4
    S.poly([(-6, 0), (-4, -h + 6), (0, -h), (4, -h + 8), (6, 0)], 0)
    S.line(0, -h, 0, 0, 1)
    S.line(-4, -h + 6, -2, 0, 1)
    S.px(-1, -h + 6, 1)
    S.outline(3)
    S.line(-3, -h + 12, -3, -h + 9, 0)
    return _c3done(S, outline=False)


def c7_snow_drift():
    """A low snow mound with a dark rim on its shaded side."""
    S = Spr()
    for y in range(-6, 1):
        w = int(11 * math.sqrt(max(0.0, 1 - ((y + 3) / 4.0) ** 2)))
        S.hline(-w, w + 1, y, 0)
        if w:
            S.px(w, y, 1)
    return _c3done(S)


def c7_lava_rock(seed=0):
    """Dark lava rock cluster with light cracks (the volcanic island's signature prop)."""
    S = Spr()
    S.disc(-4, -5, 6, 3)
    S.disc(5, -4, 5, 3)
    S.disc(0, -9, 5, 3)
    for x0, y0, x1, y1 in ((-7, -6, -2, -3), (2, -9, 5, -6), (4, -3, 8, -5)):
        S.line(x0, y0, x1, y1, 1)
    return _c3done(S, outline=False)


def c7_steam_vent(phase=0):
    """A fumarole: a ring of stones with a rising puff of steam (3 frames)."""
    S = Spr()
    for a in range(0, 360, 40):
        S.px(int(7 * math.cos(math.radians(a))), int(3 * math.sin(math.radians(a))), 2)
    for k in range(3):
        cy = -6 - ((phase + k) % 3) * 7
        r = 3 + ((phase + k) % 3)
        S.disc((k - 1) * 2, cy, r, 1 if k % 2 else 0, ring=1)
    return _c3done(S, outline=False)


def c7_volcano():
    """The island's volcano (far backdrop, 2x2 footprint): a pale cone with a dark crater rim and a lava streak."""
    S = Spr()
    S.poly([(-40, 0), (-12, -46), (12, -46), (40, 0)], 1)
    S.poly([(-12, -46), (12, -46), (8, -42), (-8, -42)], 3)
    for t in range(0, 40, 2):
        S.px(2 + t // 6, -42 + t, 3)
    for x in range(-38, 39, 7):
        S.px(x, -2, 2)
    return _c3done(S, fw=2, fh=2)


def c7_brazier(phase=0):
    """Fire gym brazier: a stone bowl on a pedestal with a flickering flame (2 frames)."""
    S = Spr()
    S.rect(-2, -10, 3, 0, 2)
    S.hline(-6, 7, -10, 3)
    S.hline(-5, 6, -11, 2)
    S.hline(-4, 5, -12, 3)
    fl = (
        [(-3, -13), (0, -20 - phase), (3, -13)]
        if phase
        else [(-3, -13), (-1, -19), (1, -16), (3, -21), (4, -13)]
    )
    S.poly(fl, 0)
    S.line(0, -14, 0 + phase, -17, 1)
    return _c3done(S)


def c7_quiz_machine():
    """Quiz terminal: a light cabinet with a dark screen showing a light '?'."""
    S = Spr()
    b = Box(S, 1, 1, 1, 1, 6, 6, 0, 20, 1, 0, 1, edge=3)
    b.J.rect(2, b.J.wid - 2, 9, 18, 3)
    q = [(0, 0), (1, 0), (2, 0), (2, 1), (1, 2), (1, 4)]
    cx = b.J.wid // 2 - 1
    for s, z in q:
        b.J.px(cx + s, 16 - z, 0)
    return _c3done(S, outline=False)


def c7_statue_switch(on=False):
    """Mansion statue: a small creature bust on a pedestal with a lever on its side (the door switch)."""
    S = Spr()
    Box(S, 1, 1, 2, 2, 5, 5, 0, 10, 1, 2, 2, edge=3)
    S.disc(0, -17, 5, 1, ring=3)
    S.px(-2, -18, 3)
    S.px(2, -18, 3)
    S.line(6, -6, 9 if on else 6, -11, 3)
    S.px(9 if on else 6, -12, 0)
    return _c3done(S, outline=False)


def c7_charred_wall(axis="i"):
    """Burnt mansion wall: light plaster with dark scorch tongues rising from the floor and a broken top edge."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 1, 0, 0, edge=3)
    for F in (b.J, b.I):
        for s in range(F.wid):
            h = 8 + int(6 * math.sin(s * 0.9)) + (s * 7) % 5
            F.vline(s, 0, h, 2)
            if s % 3 == 0:
                F.vline(s, h, h + 3, 1)
        for s in range(0, F.wid, 5):
            F.px(s, WALL_H - 1, 1)
    return _c3done(S, outline=False)


def c7_rubble():
    """A heap of fallen beams and plaster chunks."""
    S = Spr()
    S.disc(-3, -2, 4, 1)
    S.disc(4, -2, 3, 0)
    S.line(-9, -1, 8, -7, 3)
    S.line(-8, 0, 9, -6, 2)
    return _c3done(S)


def c7_revival_machine():
    """Fossil reviver: a tall glass capsule (light, dark ring bands) on a base, a fossil silhouette inside."""
    S = Spr()
    Box(S, 1, 1, 0, 0, 7, 7, 0, 6, 1, 2, 2, edge=3)
    for y in range(-34, -6):
        w = 7
        S.hline(-w, w + 1, y, 0)
        S.px(-w, y, 3)
        S.px(w, y, 3)
    for y in (-34, -26, -14):
        S.hline(-7, 8, y, 3)
    S.disc(0, -20, 3, 2, ring=3)
    S.line(-2, -18, 2, -22, 1)
    S.hline(-8, 9, -36, 3)
    S.hline(-6, 7, -37, 2)
    return _c3done(S, outline=False)


def c7_generator(phase=0):
    """Power plant generator (1x1, tall): a light housing with dark coil rings on top and a spark arcing between two posts (2 frames)."""
    S = Spr()
    b = Box(S, 1, 1, 1, 1, 6, 6, 0, 16, 1, 0, 1, edge=3)
    for F in (b.J, b.I):
        for z in (4, 8, 12):
            F.hline(1, F.wid - 1, z, 2)
    S.rect(-6, -30, -4, -18, 3)
    S.rect(5, -30, 7, -18, 3)
    pts = [(-4, -28), (-1, -25 - phase * 2), (1, -29 + phase * 2), (5, -27)]
    for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
        S.line(x0, y0, x1, y1, 0)
    return _c3done(S, outline=False)


# The pieces below are NOT in tools/c7_mock.py (the approved scenes only needed the drawings above): the places the maps need that the mock did not show.
def c7_frost_tuft(seed=0):
    """Islas Escarcha: a tuft of frost-grass on the pale ice (the cave's wild-encounter tile, C_GRASS): short snow-white blades with dark tips on a light frosted floor."""
    S = tile_canvas()
    for y in range(15):
        for x in range(28):
            if in_diamond(x, y, 7) and dither(x, y, 0.08):
                S.t(x, y, 1)
    tufts = []
    for r, y in enumerate((4, 8, 12)):
        for x in range(3 + (4 if r % 2 else 0), 26, 8):
            tufts.append((x, y))
    for x, y in tufts:
        for dx, dy, lv in (
            (0, 0, 2),
            (-1, -1, 2),
            (1, -1, 2),
            (0, -2, 3),
            (-2, -3, 3),
            (2, -3, 3),
            (0, -1, 1),
        ):
            if in_diamond(x + dx, y + dy, 7) and in_diamond(x + dx, y + dy + 1, 7):
                S.t(x + dx, y + dy, lv)
    return _c3T(S)


def c7_ember_boards(pi, pj, seed=0):
    """Mansion Calcinada: a scorched patch of the same boards (the mansion's wild-encounter tile, C_GRASS): the board seams, a drift of grey ash and dark cinders that no plain board has."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        U, V = u + pi, v + pj
        if int(V * 6) != int(V * 6 - 0.03):
            return 1
        k = r.random()
        if k < 0.07:
            return 3
        if dither(x, y, 0.42):
            return 1
        return 0

    return _c3T(gtile(f, seed + 7))


def c7_gym_fuego(name="gym_fuego"):
    """Gimnasio Fuego (not in the mock): the forge-gym of Isla Ceniza (3 x 2, an OPEN doorway in the middle tile): a dark stone body on a light footing, a big chimney stack with a flame on the roof and a plaque
    with a flame over the door. Flat white like the other gyms (the memory budget)."""
    fw, fh = 3, 2
    S = Spr()
    WH = 32
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 1, 2, 3, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 5, 1)  # the light footing
    I.rect(0, I.wid, 0, 5, 1)
    for sf in (J, I):  # courses of stone
        for z_ in range(10, WH - 5, 7):
            sf.hline(0, sf.wid, z_, 3)
        for s_ in range(2, sf.wid, 8):
            sf.vline(s_, 5, 9, 3)
    J.rect(0, J.wid, WH - 5, WH, 3)  # the eaves
    I.rect(0, I.wid, WH - 5, WH, 3)
    J.rect(15, 27, 0, 24, 3)  # the open doorway (middle tile), dark inside
    J.rect(17, 25, 0, 22, 3)
    for sx in range(17, 26):
        J.px(sx, 1 + (sx % 3 == 0), 0)
    J.rect(12, 31, 24, 31, 3)  # the plaque over the door: a flame
    J.rect(13, 30, 25, 30, 0)
    for sx, h in (
        (16, 2),
        (17, 3),
        (18, 4),
        (19, 4),
        (20, 5),
        (21, 4),
        (22, 5),
        (23, 4),
        (24, 4),
        (25, 3),
        (26, 2),
    ):
        J.rect(sx, sx + 1, 26, 26 + h - 1, 3)
    roof(S, fw, fh, WH, 3)
    rz = WH + 3
    Box(S, fw, fh, 14, 3, 19, 8, rz, rz + 16, 1, 2, 3, edge=3)  # the chimney stack
    Box(S, fw, fh, 13, 2, 20, 9, rz + 16, rz + 18, 1, 2, 3, edge=3)
    cx_, cy_ = 8, -(rz + 18) - 14  # the top of the stack on the screen
    S.poly(
        [
            (cx_ - 5, cy_),
            (cx_ - 3, cy_ - 7),
            (cx_ - 1, cy_ - 4),
            (cx_, cy_ - 12),
            (cx_ + 2, cy_ - 5),
            (cx_ + 4, cy_ - 8),
            (cx_ + 5, cy_),
        ],
        3,
    )  # the flame over the stack
    S.poly(
        [
            (cx_ - 3, cy_),
            (cx_ - 1, cy_ - 5),
            (cx_, cy_ - 8),
            (cx_ + 2, cy_ - 4),
            (cx_ + 3, cy_),
        ],
        0,
    )
    S.flat_white(1)
    finish(name, S, "object", fw, fh)


def c7_mansion_ruin(name="mansion_ruin"):
    """Mansion Calcinada (not in the mock): the burnt manor of Isla Ceniza (3 x 2, an OPEN doorway in the middle tile): pale plaster with scorch tongues above every window, boarded windows, a broken roofline and a
    leaning chimney. Flat white (the memory budget)."""
    fw, fh = 3, 2
    S = Spr()
    WH = 36
    b = Box(S, fw, fh, 0, 0, 21, 14, 0, WH, 1, 0, 1, edge=3)
    J, I = b.J, b.I
    J.rect(0, J.wid, 0, 4, 2)  # the footing
    I.rect(0, I.wid, 0, 4, 3)
    for sf, wins in (
        (J, (3, 31)),
        (I, (3, 17)),
    ):  # boarded windows: a dark pane with two planks across
        for a_ in wins:
            sf.rect(a_ - 1, a_ + 8, 14, 29, 3)
            sf.rect(a_, a_ + 7, 15, 28, 1)
            sf.hline(a_, a_ + 7, 18, 3)
            sf.hline(a_, a_ + 7, 24, 3)
            for s_ in range(a_, a_ + 8):  # the scorch tongue above the window
                sf.vline(s_, 29, 29 + (s_ * 5) % 6 + 2, 2)
    J.rect(15, 27, 0, 24, 3)  # the open doorway (middle tile), dark inside
    J.rect(17, 25, 0, 22, 3)
    for sx in range(17, 26):
        J.px(sx, 1 + (sx % 3 == 0), 0)
    for sf in (J, I):  # a broken top edge
        for s_ in range(0, sf.wid, 5):
            sf.vline(s_, WH - 4 - (s_ // 5) % 3, WH, 3)
        sf.hline(0, sf.wid, WH - 5, 3)
    roof(S, fw, fh, WH, 2)
    rz = WH + 2
    Box(S, fw, fh, 14, 4, 18, 8, rz, rz + 12, 1, 2, 3, edge=3)  # the chimney
    S.flat_white(1)
    finish(name, S, "object", fw, fh)


def c7_central_door():
    """Ruta 10's lake -> Central Electrica (not in the mock): the plant's service door (1 x 1, a walkable warp cell like the tunnel's hole), set in a light concrete frame: a dark steel door, a lintel with a bolt and ribbed fins either side. Flat (no mask)."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, 24, 1, 2, 3, edge=3)
    J = b.J
    for s_ in range(1, J.wid, 3):
        J.vline(s_, 2, 16, 3)
    J.rect(3, J.wid - 3, 0, 15, 3)  # the dark door
    J.rect(4, J.wid - 4, 0, 14, 0)
    J.rect(5, J.wid - 5, 0, 13, 3)
    J.rect(4, J.wid - 4, 17, 22, 3)  # the lintel with a bolt
    J.rect(5, J.wid - 5, 18, 21, 0)
    for k in range(3):
        J.px(8 - k, 21 - k, 3)
        J.px(9 - k, 21 - k, 3)
        J.px(7 + k // 2, 18 + k, 3)
    S.flat_white(1)
    return _c3done(S, outline=False)


def c7_ice_mouth():
    """Islas Escarcha: the cave mouth on the islands' shore (1 x 1, the warp tile): a mound of pale ice with a dark arched opening and a fringe of icicles."""
    S = Spr()
    S.poly([(-12, 0), (-10, -12), (-5, -22), (0, -25), (5, -22), (10, -12), (12, 0)], 0)
    S.poly([(-6, 0), (-6, -9), (-3, -15), (0, -17), (3, -15), (6, -9), (6, 0)], 3)
    for x in range(-12, 13, 3):
        S.line(x, -8 - (x * x) % 5, x, -(x * x) % 4 - 14 + (x % 2) * 3, 1)
    S.line(-10, -12, -5, -22, 1)
    for x in range(-5, 6, 2):
        S.px(x, -17 + (abs(x) // 2), 0)
        S.px(x, -16 + (abs(x) // 2), 0)
    return _c3done(S, outline=True)


def c7_rapids():
    """Islas Escarcha: the rapids (an ACTOR, 1 x 1, solid): a swirling current of dark water with light streaks, standing on the channel's tile until the boulders plug its source."""
    S = Spr()
    for y in range(-8, 8):
        for x in range(-14, 15):
            if abs(x) / 14.0 + abs(y) / 7.0 <= 1.0:
                s = (x * 3 + y * 6) % 12
                S.px(x, y - 7, 3 if s < 3 else (2 if s < 6 else (0 if s < 7 else 3)))
    for k in range(10):
        a = k * 0.55
        S.px(
            int(10 * math.cos(a) * (1 - k / 14)),
            int(5 * math.sin(a) * (1 - k / 14)) - 7,
            0,
        )
    S.line(-10, -9, -4, -12, 0)
    S.line(-3, -12, 4, -10, 1)
    S.line(5, -9, 11, -6, 0)
    return S


def c7_spark_plate(pi, pj, seed=0):
    """Central Electrica: the plant's wild-encounter tile (C_GRASS): the same big steel plates as the gym floor with a burnt scorch blotch and a hairline bolt crack across the quarter (nothing a plain plate has)."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.035) or (pj == 0 and v < 0.035):
            return 3
        cu = u < 0.1 if pi == 0 else u > 0.9
        cv = v < 0.1 if pj == 0 else v > 0.9
        if cu and cv:
            return 0
        U, V = u + pi, v + pj
        zig = abs(((V * 5) % 1.0) - 0.5) * 0.18 + 0.9
        if abs(U - zig - 0.15 * pj) < 0.035 and 0.3 < V < 1.8:
            return 3
        d = math.hypot(U - (0.7 + 0.6 * ((seed + pj) % 2)), V - 1.0)
        if d < 0.28:
            return 2 if dither(x, y, 0.6) else 1
        if (pi == 0 and u < 0.07) or (pj == 0 and v < 0.07):
            return 1
        return 1 if dither(x, y, 0.45) else 0

    return _c3T(gtile(f, seed + 3))


def c7_board_door():
    """Mansion Calcinada: a boarded-up door (an ACTOR, 1 x 1, solid): charred planks nailed across a doorway; it is a wall piece that vanishes (the door is open) when a statue's switch is thrown."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 1, 2, 3, edge=3)
    for F in (b.J, b.I):
        for s_ in range(1, F.wid, 3):
            F.vline(s_, 2, WALL_H - 2, 3)
        for z in (8, 20, 29):
            F.rect(0, F.wid, z, z + 2, 3)
        for s_ in range(2, F.wid, 5):
            F.px(s_, 14 + (s_ % 3), 0)
            F.px(s_ + 1, 25, 0)
    return S


def c7_stairs():
    """Mansion Calcinada: a flight of charred wooden stairs (1 x 1, the warp cell between floors): four steps rising toward the back wall, light treads and dark risers."""
    S = Spr()
    for k in range(4):
        b0, b1 = 5 - 2 * k, 7 - 2 * k
        Box(S, 1, 1, 0, max(b0, 0), 7, b1, 0, 4 * (k + 1), 1, 2, 3, edge=3)
    return _c3done(S, outline=False)


def c7_air(lv):
    """The invisible wall of a room's front sides (a SOLID tile of one flat level: level 0 on a white map, 3 on a dark one: it looks like nothing next to the map's background): the walls of the other two sides are drawn."""
    return _c3T(gtile(lambda x, y, u, v, r: lv))


def c7_pieces():
    def put(name, spec):
        S, kind, fw, fh, tile, outline = spec
        finish(name, S, kind, fw, fh, tile, outline)

    # ground (BIG light slabs: quarter tiles by cell parity)
    for a in (0, 1):
        for b in (0, 1):
            put(
                "ice_%d%d" % (a, b), c7_ice_slab(a, b)
            )  # Islas Escarcha: big pale ice slabs
            put(
                "burnt_%d%d" % (a, b), c7_burnt_boards(a, b)
            )  # Mansion Calcinada: big light scorched boards
            put(
                "ember_%d%d" % (a, b), c7_ember_boards(a, b)
            )  # ... and their cinder patches (C_GRASS)
    for s in range(3):
        put("ash_%d" % s, c7_ash(s))  # Isla Ceniza: volcanic ash
    for ph in (0, 1):
        put(
            "current_%d" % ph, c7_current(ph)
        )  # the cave's channel (an animated water tile)
    put("frost_tuft", c7_frost_tuft())  # Islas Escarcha: the wild tile
    # objects
    for ph in (0, 1):
        put("kelp_%d" % ph, c7_kelp(ph))
        put("generator_%d" % ph, c7_generator(ph))
        put("brazier_%d" % ph, c7_brazier(ph))
    for ph in range(3):
        put("steam_vent_%d" % ph, c7_steam_vent(ph))
    for s in range(2):
        put("ice_pillar_%d" % s, c7_ice_pillar(s))
        put("lava_rock_%d" % s, c7_lava_rock(s))
    put("sandbar", c7_sandbar())
    put("snow_drift", c7_snow_drift())
    put("volcano", c7_volcano())
    put("quiz_machine", c7_quiz_machine())
    put("statue_off", c7_statue_switch(False))
    put("statue_on", c7_statue_switch(True))
    put("charwall", c7_charred_wall("i"))
    put("rubble_c7", c7_rubble())
    put("reviver", c7_revival_machine())
    put("stairs", c7_stairs())
    unify_frames(("kelp_0", "kelp_1"))
    unify_frames(("generator_0", "generator_1"))
    unify_frames(("brazier_0", "brazier_1"))
    unify_frames(("steam_vent_0", "steam_vent_1", "steam_vent_2"))
    put(
        "air_0", c7_air(0)
    )  # the invisible solid cells of the mansion's / the gym's front sides (white maps)
    put("air_3", c7_air(3))  # ... and of the power plant's (a dark map)
    put("ice_mouth", c7_ice_mouth())
    for a in (0, 1):
        for b in (0, 1):
            put(
                "spark_%d%d" % (a, b), c7_spark_plate(a, b)
            )  # Central Electrica: the plant's wild-encounter plates (C_GRASS)
    _actor_write(
        "statue_off", {"south": c7_statue_switch(False)[0]}
    )  # the mansion's statue switches (solid NPCs: the lever is down / up)
    _actor_write("statue_on", {"south": c7_statue_switch(True)[0]})
    _actor_write(
        "bdoor", {"south": c7_board_door()}
    )  # the mansion's boarded doors (solid NPCs that vanish)
    _actor_write(
        "rapids", {"south": c7_rapids()}
    )  # the cave's rapids (a solid NPC that dissolves when the boulders plug it)
    c7_gym_fuego()  # Isla Ceniza: the fire gym (3 x 2, flat white)
    c7_mansion_ruin()  # Isla Ceniza: the burnt mansion (3 x 2, flat white)
    put(
        "central_door", c7_central_door()
    )  # Ruta 10: the power plant's service door on the lake (1 x 1, a walkable warp cell)


# ================================================================ Chapter 9 (C9, contract 56): Ruta 23, the Senda de Campeones, the Meseta Cumbre, the five league rooms, the Cueva Secreta
# The drawings of tools/c9_mock.py (approved by the user on the device) moved here UNCHANGED (pixel for pixel: `python3 tools/c9_mock.py` now loads the generated files and
# `build/c9/pieces_check.py` compared every piece with the in-memory drawing of the old mock). The four PixelLab decor objects of the ice / dragon / champion rooms become catalog objects
# from their PNGs (assets/art/objects_c9/*.png): the same adaptive typecast and scale as the mock's `pl_object`. Ground slabs are BIG (2 x 2 tiles: the four quarter tiles
# <name>_<i%2><j%2> follow the parity of the cell). The ice slab and the snow drift are the drawings of tools/c7_mock.py (C7's Islas Escarcha), copied under C9 names (`iceslab_*`, `snowdrift`):
# when C7's own pieces land the maintainer may point the Hielo room at them and drop these.
def c9_rock_floor(seed=0):
    """Senda de Campeones floor: LIGHT worn rock with sparse dark grit and a few hairline cracks."""

    def f(x, y, u, v, r):
        if abs((u - 0.2) - 0.9 * (v - 0.3)) < 0.02 and seed % 2 and 0.2 < v < 0.8:
            return 2
        k = r.random()
        return 3 if k < 0.02 else (1 if k < 0.07 else 0)

    return _c3T(gtile(f, seed))


def c9_marble(pi, pj, seed=0):
    """League marble: big light slabs (2x2) with a soft vein crossing each block and a thin gray joint."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        U, V = u + pi, v + pj
        if abs(math.sin(U * 2.2 + V * 1.3 + seed) * 0.3 + 0.9 - V) < 0.025:
            return 1
        return 0

    return _c3T(gtile(f, seed))


def c9_tatami_ring(pi, pj, seed=0):
    """Fighting room: a big light canvas mat (2x2) with a dark ring line near the block edge (the ring's floor)."""

    def f(x, y, u, v, r):
        U, V = u + pi, v + pj
        if min(U, V, 2 - U, 2 - V) < 0.06:
            return 3
        if min(U, V, 2 - U, 2 - V) < 0.12:
            return 1
        return 0

    return _c3T(gtile(f, seed))


def c9_ghost_floor(pi, pj, seed=0):
    """Ghost room: light old flagstones (2x2) under a mist dither (the room is `@fog yes`: the real mist drifts over it)."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        return 1 if dither(x, y, 0.12) else 0

    return _c3T(gtile(f, seed))


def c9_crystal_floor(seed=0):
    """Cueva Secreta: LIGHT cave rock with tiny bright crystal glints (dark ring + white core)."""

    def f(x, y, u, v, r):
        k = r.random()
        if k < 0.015:
            return 3
        if k < 0.05:
            return 1
        return 0

    return _c3T(gtile(f, seed))


def c9_ice_slab(pi, pj, seed=0):
    """Ice room floor (tools/c7_mock.py ice_slab): big pale ice slabs (2x2), a fine frost dither, thin dark crack lines that run across the block."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        U, V = u + pi, v + pj
        if (
            abs((U - 0.3) - 0.7 * (V - 0.2)) < 0.025 and 0.4 < V < 1.6
        ):  # one long hairline crack per block
            return 2
        return 1 if dither(x, y, 0.08) else 0

    return _c3T(gtile(f, seed))


def c9_snow_drift():
    """A low snow mound with a dark rim on its shaded side (tools/c7_mock.py snow_drift)."""
    S = Spr()
    for y in range(-6, 1):
        w = int(11 * math.sqrt(max(0.0, 1 - ((y + 3) / 4.0) ** 2)))
        S.hline(-w, w + 1, y, 0)
        if w:
            S.px(w, y, 1)
    return _c3done(S)


def c9_badge_gate():
    """Ruta 23 checkpoint gate: two stone posts with a lintel; a round badge emblem (dark ring, light core) on the lintel."""
    S = Spr()
    for x in (-9, 8):
        S.rect(x, -26, x + 3, 0, 1)
        S.px(x, -26, 3)
    S.rect(-10, -30, 12, -25, 0)
    S.hline(-10, 12, -30, 3)
    S.hline(-10, 12, -25, 3)
    S.disc(1, -28, 3, 0, ring=3)
    S.px(1, -28, 2)
    return _c3done(S)


def c9_torch(phase=0):
    """Wall torch on a post: a dark iron bowl and a flickering flame (2 frames)."""
    S = Spr()
    S.rect(0, -16, 2, 0, 3)
    S.hline(-3, 6, -16, 3)
    S.poly([(-2, -17), (1, -24 - phase), (4, -17)], 0)
    S.px(1, -19, 1)
    return _c3done(S)


def c9_banner(axis="i"):
    """League banner wall: light plaster wall with a hanging long banner bearing a crown emblem."""
    S = Spr()
    b = Box(S, 1, 1, 0, 0, 7, 7, 0, WALL_H, 0, 1, 1, edge=3)
    F = b.J if axis == "i" else b.I
    F.rect(4, F.wid - 4, 6, WALL_H - 2, 2)
    for z in range(6, 10):
        F.px(F.wid // 2 - (10 - z), z, 1)
        F.px(F.wid // 2 + (10 - z), z, 1)
    cx = F.wid // 2
    F.rect(cx - 2, cx + 3, 20, 23, 0)
    F.px(cx - 2, 24, 0)
    F.px(cx, 24, 0)
    F.px(cx + 2, 24, 0)
    return _c3done(S, outline=False)


def c9_statue():
    """Meseta Cumbre statue: a tall pedestal with a stylised winged crown (the league's emblem)."""
    S = Spr()
    Box(S, 1, 1, 1, 1, 6, 6, 0, 14, 0, 1, 2, edge=3)
    S.rect(-1, -28, 2, -14, 1)
    S.poly([(-9, -30), (0, -26), (9, -30), (6, -24), (0, -22), (-6, -24)], 0)
    S.poly([(-4, -38), (-2, -32), (0, -36), (2, -32), (4, -38), (4, -31), (-4, -31)], 1)
    S.outline(3)
    return _c3done(S, outline=False)


def c9_ring_post():
    """Fighting room ring corner: a padded post with three ropes leaving it."""
    S = Spr()
    S.rect(-1, -20, 2, 0, 3)
    S.rect(-2, -22, 3, -18, 1)
    for z in (-8, -12, -16):
        S.line(1, z, 14, z + 7, 2)
    return _c3done(S)


def c9_candelabra(phase=0):
    """Ghost room candelabra: three candles on a dark stand, flames flicker (2 frames)."""
    S = Spr()
    S.rect(0, -14, 1, 0, 3)
    S.hline(-6, 7, -14, 3)
    for x in (-6, 0, 6):
        S.rect(x, -20, x + 1, -14, 0)
        S.px(x, -21 - ((phase + x) % 2), 1)
        S.px(x, -22 - ((phase + x) % 2), 0)
    return _c3done(S)


def c9_glow_crystal(seed=0):
    """Cueva Secreta crystal cluster: light prisms with dark facet lines (the two variants blink against each other)."""
    S = Spr()
    for dx, h in ((-5, 14 + seed * 3), (0, 22), (5, 12)):
        S.poly(
            [(dx - 3, 0), (dx - 2, -h + 4), (dx, -h), (dx + 2, -h + 4), (dx + 3, 0)], 0
        )
        S.line(dx, -h, dx, 0, 1)
    S.outline(3)
    return _c3done(S, outline=False)


def c9_rock_gate():
    """The Senda's boulder gate (an ACTOR, vanishes when its pit is filled): a wall of three stacked rocks, light stone with dark joints and a crack. Not in the mock (the mock has no gate)."""
    S = Spr()
    S.disc(-5, -5, 6, 0, ring=3)
    S.disc(6, -5, 6, 1, ring=3)
    S.disc(0, -13, 6, 0, ring=3)
    S.disc(-9, -14, 5, 1, ring=3)
    S.disc(9, -14, 5, 0, ring=3)
    S.line(-2, -17, 1, -11, 3)
    S.line(1, -11, 4, -9, 3)
    S.px(-6, -8, 2)
    S.px(8, -8, 2)
    return S


def c9_pl_piece(name, png, max_h, dy=4):
    """A PixelLab object PNG (assets/art/objects_c9/<png>.png) as a catalog OBJECT, scaled to max_h px tall, anchored at its base centre (the mock's `pl_object`)."""
    import lvlib
    import battle_mock as bm

    sp = bm.load(
        os.path.join(ROOT, "assets", "art", "objects_c9", png + ".png"), max_h=max_h
    )
    lvlib.write_lv_png(
        os.path.join(OUT_DIR, name + ".lv.png"), sp.lv, sp.mk, sp.w // 2, sp.h - 1 - dy
    )
    meta = os.path.join(OUT_DIR, name + ".json")
    d = json.load(open(meta))
    d.update({"fw": 1, "fh": 1, "kind": "object"})
    with open(meta, "w") as f:
        json.dump(d, f)
    RESULTS.append((name, "object", sp.w, sp.h, sp.w // 2, sp.h - 1 - dy, 1, 1))


def c9_pieces():
    for a in (0, 1):
        for b in (0, 1):
            for nm, fn in (
                ("marb", c9_marble),
                ("ringm", c9_tatami_ring),
                ("ghf", c9_ghost_floor),
                ("iceslab", c9_ice_slab),
            ):
                S, kind, fw, fh, tile, outline = fn(a, b)
                finish("%s_%d%d" % (nm, a, b), S, kind, fw, fh, tile, outline)
    for s in range(3):
        for nm, fn in (("rockf", c9_rock_floor), ("crys", c9_crystal_floor)):
            S, kind, fw, fh, tile, outline = fn(s)
            finish("%s_%d" % (nm, s), S, kind, fw, fh, tile, outline)
    for nm, spec in (
        ("bgate", c9_badge_gate()),
        ("snowdrift", c9_snow_drift()),
        ("lstatue", c9_statue()),
        ("ringpost", c9_ring_post()),
        ("torch_0", c9_torch(0)),
        ("torch_1", c9_torch(1)),
        ("cand_0", c9_candelabra(0)),
        ("cand_1", c9_candelabra(1)),
        ("gcry_0", c9_glow_crystal(0)),
        ("gcry_1", c9_glow_crystal(1)),
        ("ban_i", c9_banner("i")),
        ("ban_j", c9_banner("j")),
    ):
        S, kind, fw, fh, tile, outline = spec
        finish(nm, S, kind, fw, fh, tile, outline)
    unify_frames(
        ("torch_0", "torch_1")
    )  # the animated pieces' two frames share one box (contract 22.2)
    unify_frames(("cand_0", "cand_1"))
    c9_pl_piece("icecr", "ice_crystals", 34)
    c9_pl_piece("dstat", "dragon_statue", 44)
    c9_pl_piece("cpil", "champion_column", 44)
    c9_pl_piece("troph", "champion_trophy", 30)
    _actor_write("rock_gate", {"south": c9_rock_gate()})


# ================================================================ Chapter 10 (D1, contract 68): the Ruinas del Origen and the hidden Gruta del Eclipse
# The ground (big light `aslab_<i%2><j%2>` slabs, `vrock` dark rock with veins) and the cave mouth / light shaft are drawn in code (pixel for pixel the user's approved mock; `python3 tools/c10_pieces_check.py`
# compares the floors with the old in-memory drawing kept in tools/attic/c10_mock_before_pieces.py). The PROPS are PixelLab objects (contract 68.9: assets/art/objects_c10/*.png, 96 x 96 RGBA, 3/4 high angle, the
# same family and the same typecast as chapter 9's `objects_c9`): `c10_pl_*` below. `python3 tools/c10_mock.py` loads the generated files.
# NEW in the game (not in the mock): the lit pedestal as an ACTOR laid over the empty pedestal object (`ped_lit`), the sealed gate of the Meseta (`seal`, an actor), the cave mouth of the islet (`gmouth`), the light
# shaft and the Piedra Eclipse of the altar (actors `lshaft`, `estone`; the mock draws the shaft as a screen overlay and the stone as part of the altar: `altar_stone` below is that mock composite, in no map).
def c10_ancient_slab(pi, pj, seed=0):
    """Ruins floor: big light slabs (2x2) with a thin gray joint, a few hairline cracks and worn corners."""

    def f(x, y, u, v, r):
        if (pi == 0 and u < 0.03) or (pj == 0 and v < 0.03):
            return 2
        U, V = u + pi, v + pj
        if (
            abs(U - 0.6 - 0.25 * math.sin(V * 5 + pi * 3)) < 0.02
            and 0.3 < V < 1.2
            and (pi + pj) % 2 == 0
        ):
            return 1
        if min(U, V, 2 - U, 2 - V) < 0.12 and dither(x, y, 0.25):
            return 1
        return 0

    return _c3T(gtile(f, seed))


def c10_vein_rock(seed=0):
    """Gruta del Eclipse floor: dark rock (level 2) with light veins (1) and a few white glints."""

    def f(x, y, u, v, r):
        if (
            abs(math.sin(u * 7 + v * 3 + seed * 2) * 0.12 + 0.5 - (u * 0.6 + v * 0.4))
            < 0.03
        ):
            return 1
        if (x * 7 + y * 13 + seed * 5) % 97 == 0:
            return 0
        return 2

    return _c3T(gtile(f, seed))


def c10_shaft_spr(h=58):
    """The light shaft that falls from a crack in the ceiling to the altar (an ACTOR: the mock overlays it on the screen): a slanted column, white core, dithered edges, a few motes."""
    S = Spr()
    for y in range(-h, 1):
        xc = y * 0.2  # leans from the upper left down to the altar
        for x in range(int(xc) - 6, int(xc) + 7):
            d = abs(x - xc)
            if d < 1.5:
                S.px(x, y, 0)
            elif d < 3.5:
                S.px(x, y, 0 if dither(x, y, 0.6) else 1)
            elif d < 6 and dither(x, y, 0.3):
                S.px(x, y, 1)
    for k in range(4):  # motes
        y = -6 - k * 13
        S.px(int(y * 0.2) + (k % 3) * 2 - 2, y, 0)
    return S


def c10_cave_mouth():
    """The Gruta del Eclipse's mouth on its islet (1 x 1, the warp tile): a mound of dark rock with light veins and a black arched opening."""
    S = Spr()
    S.poly([(-12, 0), (-10, -12), (-5, -21), (0, -24), (5, -21), (10, -12), (12, 0)], 2)
    S.poly([(-6, 0), (-6, -9), (-3, -15), (0, -17), (3, -15), (6, -9), (6, 0)], 3)
    S.line(-10, -12, -5, -21, 1)
    S.line(5, -21, 10, -12, 1)
    S.line(-9, -5, -8, -9, 1)
    S.line(9, -4, 8, -8, 1)
    return _c3done(S, outline=True)


C10_PL_DIR = os.path.join(ROOT, "assets", "art", "objects_c10")
# Heights in px (the hero is 31 px tall, chapter 9's props 30 .. 44): the pedestals a bit under the hero (the lit one shares the empty one's scale, so its shard rises above), the stele and the door taller.
C10_H = {"ped": 25, "stele": 40, "fcol": 34, "altar": 36, "door": 44}
C10_CUT = {
    "lit": 40
}  # the shard's dark cut (its shaded facet is ~100: the default 70 would be near it)


def c10_pl_img(png):
    return Image.open(os.path.join(C10_PL_DIR, png + ".png")).convert("RGBA")


def c10_pl_box(im):
    return im.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()


def c10_pl_conv(im, s, box, tones=None, flip=False, cut=70):
    """One PixelLab RGBA image -> (levels, mask, ax, ay) of its scaled canvas: crop to `box`, (mirror), BOX-resample by `s`, pad 1 and the adaptive 4-level typecast of chapter 9's `battle_mock.load`
    (dark cut `cut` = 70 as there, 3 k-means tones; `tones` pools them over several images). The anchor is the BOTTOM CENTRE of the canvas content, the base of the prop.
    """
    import lvlib

    im = im.crop(box)
    if flip:
        im = im.transpose(Image.FLIP_LEFT_RIGHT)
    w, h = max(1, round(im.width * s)), max(1, round(im.height * s))
    if s != 1.0:
        im = im.resize((w, h), Image.BOX)
    pad = Image.new("RGBA", (im.width + 2, im.height + 2), (0, 0, 0, 0))
    pad.paste(im, (1, 1))
    if tones is None:
        tones = lvlib.adaptive_tones(pad, cut)
    lv, mk = lvlib.adaptive_levels(pad, cut, tones=tones)
    return lv, mk, 1 + im.width // 2, im.height


def c10_pl_crop(lv, mk, ax, ay):
    """Crop a canvas to its opaque box: (levels, mask, ax, ay) with the anchor moved with it."""
    import lvlib

    x0, y0, x1, y1 = lvlib.mask_bbox(mk)
    return (
        lvlib.crop(lv, x0, y0, x1, y1),
        lvlib.crop(mk, x0, y0, x1, y1),
        ax - x0,
        ay - y0,
    )


def c10_pl_object(name, lv, mk, ax, ay, dy=4):
    """Write a catalog OBJECT (anchor = the base centre, `dy` rows above the bottom: chapter 9's `c9_pl_piece`)."""
    import lvlib

    lv, mk, ax, ay = c10_pl_crop(lv, mk, ax, ay)
    lvlib.write_lv_png(os.path.join(OUT_DIR, name + ".lv.png"), lv, mk, ax, ay - dy)
    meta = os.path.join(OUT_DIR, name + ".json")
    d = json.load(open(meta))
    d.update({"fw": 1, "fh": 1, "kind": "object"})
    with open(meta, "w") as f:
        json.dump(d, f)
    RESULTS.append((name, "object", len(lv[0]), len(lv), ax, ay - dy, 1, 1))


def c10_pl_actor(name, lv, mk, ax, ay, dy=4):
    """Write an ACTOR that lands on the screen exactly where the object written with the same (ax, ay, dy) does: an actor's anchor stands 6 rows above the vertex an object's anchor is on (`_actor_write`)."""
    import lvlib

    lv, mk, ax, ay = c10_pl_crop(lv, mk, ax, ay)
    lvlib.write_lv_png(
        os.path.join(ACTORS_DIR, name, "south", "f0.lv.png"), lv, mk, ax, ay - dy - 6
    )


C10_ALTAR_CIRCLE = (
    46.5,
    19.8,
    12.4,
)  # the Piedra Eclipse (black disc + white corona) in eclipse_altar.png: centre x, y, radius
C10_ALTAR_TOP = [
    (27, 32),
    (46.5, 20),
    (66, 32),
    (46.5, 41),
]  # the top face of the altar's small block (the stone floats over its back half)


def _c10_in_poly(x, y, poly):
    inside = False
    for k in range(len(poly)):
        (x0, y0), (x1, y1) = poly[k], poly[(k + 1) % len(poly)]
        if (y0 > y) != (y1 > y) and x < x0 + (y - y0) * (x1 - x0) / (y1 - y0):
            inside = not inside
    return inside


def c10_altar_split(im):
    """eclipse_altar.png -> (the EMPTY altar, the STONE alone), both RGBA of the same 96 x 96 canvas, by code (no new art). The stone = the disc + corona (a circle) and the sparkle specks above the block;
    the empty altar = the rest, with the stone's circle replaced by the block's top face (light gray) where it overlapped it and by nothing above it.
    """
    cx, cy, r = C10_ALTAR_CIRCLE
    empty, stone = im.copy(), Image.new("RGBA", im.size, (0, 0, 0, 0))
    pe, ps, po = empty.load(), stone.load(), im.load()
    for y in range(im.height):
        for x in range(im.width):
            if po[x, y][3] < 128:
                continue
            in_circle = (x - cx) ** 2 + (y - cy) ** 2 <= r * r
            on_top = _c10_in_poly(x, y, C10_ALTAR_TOP)
            speck = (
                not in_circle
                and y < 31
                and not any(
                    _c10_in_poly(x + dx, y + dy, C10_ALTAR_TOP)
                    for dx in (-2, 0, 2)
                    for dy in (-2, 0, 2)
                )
            )
            if in_circle or speck:
                ps[x, y] = po[x, y]
                pe[x, y] = (
                    (192, 192, 191, 255) if (in_circle and on_top) else (0, 0, 0, 0)
                )
    return empty, stone


C10_SHARD_K = 1.3  # the floating shard is drawn at 1.3 x the pedestal's scale (a gem of 8 px would not read)
C10_SHARD_TONES = (
    140,
    212,
)  # lum < 140 -> level 2, < 212 -> 1, else 0 (the crystal's three facets: 233 / 179 / 100)
C10_SHARD_Y = (
    24  # pedestal_empty.png: the row above the socket where the shard's tip floats
)
C10_SHARD_DIAMOND = (
    47.5,
    26,
    14,
    16.5,
)  # pedestal_lit.png: the crystal's diamond (centre x, y, half width, half height); above row 30 everything is the shard (rays, sparks)


def c10_lit_shard(im):
    """pedestal_lit.png -> the glowing fragment alone (the crystal, its rays and sparks), same canvas. WHY not the whole picture: the lit pedestal is an ACTOR laid over the empty pedestal OBJECT (a flag
    shows it), and the generated lit platform is a different, lower and wider shape than the drum: the drum's rim would show around it like a cage. So the lit state is the SAME drum with the shard floating above its
    socket (the old code-drawn design too); the green platform of the PNG is not used.
    """
    cx, cy, hw, hh = C10_SHARD_DIAMOND
    out = Image.new("RGBA", im.size, (0, 0, 0, 0))
    po, pd = im.load(), out.load()
    for y in range(im.height):
        for x in range(im.width):
            if po[x, y][3] >= 128 and (
                y <= 30 or abs(x - cx) / hw + abs(y - cy) / hh <= 1.0
            ):
                pd[x, y] = po[x, y]
    return out


def c10_pl_pieces():
    import lvlib

    # pedestals: the empty drum (object ped0) and, over it, the shard (actor ped_lit, shown by a flag) floating above the socket
    pe, pl = c10_pl_img("pedestal_empty"), c10_pl_img("pedestal_lit")
    be = c10_pl_box(pe)
    s = C10_H["ped"] / (be[3] - be[1])
    c10_pl_object("ped0", *c10_pl_conv(pe, s, be))
    sh = c10_lit_shard(pl)
    bs = c10_pl_box(sh)
    lift = round(
        (be[3] - 1 - C10_SHARD_Y) * s
    )  # rows from the drum's bottom to the shard's tip
    lv, mk, ax, ay = c10_pl_conv(
        sh,
        s * C10_SHARD_K,
        bs,
        lvlib.QuantileTones(C10_SHARD_TONES),
        cut=C10_CUT["lit"],
    )  # fixed cuts: lit facet white, middle light gray, shaded facet dark gray
    c10_pl_actor("ped_lit", lv, mk, ax, ay + lift)
    # the mural stele, the fallen columns (the other orientation = the mirror image), the sealed door (the Meseta's `seal` actor: the 1-tile NPC the engine already places)
    im = c10_pl_img("mural_stele")
    b = c10_pl_box(im)
    c10_pl_object("ostele", *c10_pl_conv(im, C10_H["stele"] / (b[3] - b[1]), b))
    im = c10_pl_img("fallen_column")
    b = c10_pl_box(im)
    s = C10_H["fcol"] / (b[3] - b[1])
    tn = lvlib.adaptive_tones(im.crop(b))
    c10_pl_object("fcol_i", *c10_pl_conv(im, s, b, tn))
    c10_pl_object("fcol_j", *c10_pl_conv(im, s, b, tn, flip=True))
    im = c10_pl_img("sealed_door")
    b = c10_pl_box(im)
    c10_pl_actor("seal", *c10_pl_conv(im, C10_H["door"] / (b[3] - b[1]), b), dy=2)
    # the altar: the empty altar (object) + the stone (actor estone, shown by cave_open over it) cut from the SAME png on the same canvas, tones pooled from the whole picture; altar_stone = the mock's composite
    im = c10_pl_img("eclipse_altar")
    b = c10_pl_box(im)
    s = C10_H["altar"] / (b[3] - b[1])
    tn = lvlib.adaptive_tones(im.crop(b))
    empty, stone = c10_altar_split(im)
    ev, em, eax, eay = c10_pl_conv(empty, s, b, tn)
    sv, sm, sax, say = c10_pl_conv(stone, s, b, tn)
    assert (eax, eay) == (sax, say) and len(ev) == len(sv)
    c10_pl_object("altar", ev, em, eax, eay)
    cv = [row[:] for row in ev]
    cm = [row[:] for row in em]
    for y in range(len(sv)):  # altar_stone = the empty altar with the stone over it
        for x in range(len(sv[0])):
            if sm[y][x]:
                cv[y][x], cm[y][x] = sv[y][x], 1
    c10_pl_object("altar_stone", cv, cm, eax, eay)
    c10_pl_actor("estone", sv, sm, sax, say)


def c10_pieces():
    for a in (0, 1):
        for b in (0, 1):
            S, kind, fw, fh, tile, outline = c10_ancient_slab(a, b)
            finish("aslab_%d%d" % (a, b), S, kind, fw, fh, tile, outline)
    for s in range(3):
        S, kind, fw, fh, tile, outline = c10_vein_rock(s)
        finish("vrock_%d" % s, S, kind, fw, fh, tile, outline)
    S, kind, fw, fh, tile, outline = c10_cave_mouth()
    finish("gmouth", S, kind, fw, fh, tile, outline)
    c10_pl_pieces()
    _actor_write("lshaft", {"south": c10_shaft_spr()})


def build_all():
    grass("grass_a", [(9, 6), (18, 5), (14, 10)])
    grass("grass_b", [(7, 8), (20, 8), (13, 3), (17, 11)])
    grass("grass_c", [(10, 4), (22, 7), (5, 7)], tuft=(15, 10))
    grass("flowers", [(15, 7), (5, 7)], flowers=[(10, 5), (19, 10)])
    tall_grass()
    for d_ in ("sw", "se", "ne", "nw"):
        ledge("ledge_" + d_, d_)
    path_tile("path_a", 1, 0)
    path_tile("path_b", 2, 2)
    floor_wood()
    floor_lab()
    mat()
    void_dark()
    wall("wall_j")
    wall("wall_i")
    wall("wall_corner")
    wall("wall_win_j", "j")
    wall("wall_win_i", "i")
    fence("fence_j", "i")  # front row j=max: runs along i, face +j
    fence("fence_i", "j")  # right column i=max: runs along j, face +i
    sign()
    skirt("skirt_i", "i")
    skirt("skirt_j", "j")
    fb_house()
    fb_house_rival()
    fb_lab()
    fb_bed()
    fb_table()
    fb_lab_table()
    fb_lab_table_crystals()
    crystal_actor("crystal_fire", "fire")
    crystal_actor("crystal_water", "water")
    crystal_actor("crystal_plant", "plant")
    flitter_actor()
    emote_excl()
    item_bag()
    fb_bookshelf()
    fb_tv()
    fb_pc()
    fb_machine()
    fb_center()
    fb_shop()
    fb_gym()
    fb_counter()
    fb_shelf()
    fb_heal()
    tree_code()
    rock_code()
    zone_pieces()
    cantera_pieces()
    c2_pieces()
    c3_pieces()
    c4_pieces()
    c4b_pieces()
    c5_pieces()
    c6_pieces()
    c8_pieces()
    c7_pieces()
    c9_pieces()
    c10_pieces()


# ---------------------------------------------------------------- preview
BG = (255, 139, 41)
INK = (24, 14, 6)


def lcd(lv):
    f = lv / 3.0
    return tuple(int(round(BG[k] + (INK[k] - BG[k]) * f)) for k in range(3))


def load_lv(name):
    im = Image.open(os.path.join(OUT_DIR, name + ".lv.png")).convert("RGBA")
    meta = json.load(open(os.path.join(OUT_DIR, name + ".json")))
    w, h = im.size
    p = im.load()
    lv = [[0] * w for _ in range(h)]
    mk = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = p[x, y]
            if a == 255:
                mk[y][x] = True
                lv[y][x] = {255: 0, 170: 1, 85: 2, 0: 3}[g]
            else:
                assert a == 0
    return lv, mk, meta


class Scene:
    def __init__(self, W, H):
        self.W, self.H = W, H
        self.lv = [[0] * W for _ in range(H)]

    def blit(self, spr, x0, y0):
        lv, mk, meta = spr
        for y, row in enumerate(lv):
            for x, v in enumerate(row):
                if mk[y][x]:
                    X, Y = x0 + x, y0 + y
                    if 0 <= X < self.W and 0 <= Y < self.H:
                        self.lv[Y][X] = v

    def image(self, scale):
        im = Image.new("RGB", (self.W, self.H))
        p = im.load()
        for y in range(self.H):
            for x in range(self.W):
                p[x, y] = lcd(self.lv[y][x])
        return im.resize((self.W * scale, self.H * scale), Image.NEAREST)


def hero_sprite():
    im = Image.open(HERO_PNG).convert("RGBA")
    im = im.crop(im.getchannel("A").getbbox())
    w, h = im.size
    p = im.load()
    lv = [[0] * w for _ in range(h)]
    mk = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = p[x, y]
            if a > 128:
                lum = 0.299 * r + 0.587 * g + 0.114 * b
                lv[y][x] = 3 - min(3, int(lum / 64))
                mk[y][x] = True
    return lv, mk, {"w": w, "h": h, "ax": w // 2, "ay": h - 1}


def compose_scene(scale=6):
    SPR = {}

    def spr(n):
        if n not in SPR:
            SPR[n] = load_lv(n)
        return SPR[n]

    N = 8
    sc = Scene(260, 190)
    ox, oy = 130, 50

    def centre(i, j):
        return ox + (i - j) * 14, oy + (i + j) * 7

    path = {(2, 2), (2, 3), (2, 4), (2, 5), (3, 2), (4, 2)}
    flowers = {(0, 4), (6, 3), (7, 2)}
    rnd = random.Random(4)
    for s in range(2 * N - 1):
        for j in range(N):
            i = s - j
            if not 0 <= i < N:
                continue
            cx, cy = centre(i, j)
            if (i, j) in path:
                n = "path_a" if (i + j) % 2 == 0 else "path_b"
            elif (i, j) in flowers:
                n = "flowers"
            else:
                n = rnd.choice(["grass_a", "grass_b", "grass_c"])
            m = spr(n)[2]
            sc.blit(spr(n), cx - m["ax"], cy - m["ay"])
            if i == N - 1:  # front-right edge cliff
                m = spr("skirt_i")[2]
                sc.blit(spr("skirt_i"), cx - m["ax"], cy + 7 - m["ay"])
            if j == N - 1:
                m = spr("skirt_j")[2]
                sc.blit(spr("skirt_j"), cx - m["ax"], cy + 7 - m["ay"])
    # objects: (name, i0, j0, fw, fh)
    objs = [
        ("fb_house", 1, 0, 2, 2),
        ("fb_house_rival", 5, 0, 2, 2),
        ("tree_code", 0, 0, 1, 1),
        ("tree_code", 7, 1, 1, 1),
        ("fb_lab_table", 3, 5, 3, 1),
        ("sign", 3, 3, 1, 1),
        ("rock_code", 0, 6, 1, 1),
        ("fence_i", 7, 4, 1, 1),
        ("fence_i", 7, 5, 1, 1),
        ("fence_i", 7, 6, 1, 1),
    ]
    objs += [("fence_j", i, 7, 1, 1) for i in range(2, 8)]
    items = [
        (i0 + fw - 1 + j0 + fh - 1, 0, n, i0, j0, fw, fh) for n, i0, j0, fw, fh in objs
    ]
    items.append((1 + 4, 1, "hero", 1, 4, 1, 1))
    items.sort(key=lambda t: (t[0], t[1]))
    for _, _, n, i0, j0, fw, fh in items:
        cx, cy = centre(i0 + fw - 1, j0 + fh - 1)
        if n == "hero":
            h = hero_sprite()
            m = h[2]
            sc.blit(h, cx - m["ax"], cy + 4 - m["ay"])
        else:
            m = spr(n)[2]
            sc.blit(spr(n), cx - m["ax"], cy + 7 - m["ay"])
    return sc.image(scale)


def font(sz):
    try:
        return ImageFont.load_default(sz)
    except Exception:
        return ImageFont.load_default()


def compose_sheet(scale=6, width=1900):
    items = []
    for name, kind, w, h, ax, ay, fw, fh in RESULTS:
        items.append((name, kind, w, h, ax, ay, fw, fh))
    f = font(15)
    pad, label_h = 18, 40
    # shelf packing
    x = y = 0
    rowh = 0
    place = []
    for it in items:
        cw = max(it[2] * scale, 130) + pad
        ch = it[3] * scale + label_h + pad
        if x + cw > width:
            x, y, rowh = 0, y + rowh, 0
        place.append((it, x, y))
        x += cw
        rowh = max(rowh, ch)
    H = y + rowh + pad
    im = Image.new("RGB", (width, H), (60, 52, 46))
    d = ImageDraw.Draw(im)
    for it, px_, py_ in place:
        name, kind, w, h, ax, ay, fw, fh = it
        lv, mk, meta = load_lv(name)
        ox, oy = px_ + pad // 2, py_ + pad // 2
        d.rectangle(
            [ox - 1, oy - 1, ox + w * scale, oy + h * scale],
            fill=BG,
            outline=(120, 100, 80),
        )
        for yy in range(h):
            for xx in range(w):
                if mk[yy][xx]:
                    c = lcd(lv[yy][xx])
                    if lv[yy][xx] == 0:
                        c = (
                            255,
                            200,
                            120,
                        )  # opaque white: tinted in the sheet so the mask is visible
                    d.rectangle(
                        [
                            ox + xx * scale,
                            oy + yy * scale,
                            ox + xx * scale + scale - 1,
                            oy + yy * scale + scale - 1,
                        ],
                        fill=c,
                    )
        cx, cy = ox + ax * scale + scale // 2, oy + ay * scale + scale // 2
        d.line([cx - 6, cy, cx + 6, cy], fill=(0, 200, 255))
        d.line([cx, cy - 6, cx, cy + 6], fill=(0, 200, 255))
        d.text((ox, oy + h * scale + 4), f"{name}", fill=(255, 255, 255), font=f)
        d.text(
            (ox, oy + h * scale + 21),
            f"{w}x{h} a({ax},{ay}) {fw}x{fh} {kind[0]}",
            fill=(200, 200, 200),
            font=f,
        )
    return im


def make_preview():
    os.makedirs(PREV_DIR, exist_ok=True)
    scene = compose_scene(6)
    scene.save(os.path.join(PREV_DIR, "tiles_scene.png"))
    sheet = compose_sheet(6)
    f = font(20)
    W = max(sheet.width, scene.width)
    out = Image.new("RGB", (W, sheet.height + scene.height + 50), (60, 52, 46))
    out.paste(sheet, (0, 0))
    ImageDraw.Draw(out).text(
        (12, sheet.height + 10),
        "scene: 8x8 patch (houses, lab bench, tree, fence, sign, rock, hero for scale)",
        fill=(255, 255, 255),
        font=f,
    )
    out.paste(scene, (0, sheet.height + 40))
    out.save(os.path.join(PREV_DIR, "tiles.png"))
    sheet.save(os.path.join(PREV_DIR, "tiles_sheet.png"))


if __name__ == "__main__":
    build_all()
    if os.path.exists(HERO_PNG):
        make_preview()
    else:  # models_dl/ is local-only: a fresh clone / CI builds the sprites but skips the contact-sheet preview
        print("preview skipped (needs", HERO_PNG, ")")
    print("wrote", len(RESULTS), "sprites to", OUT_DIR)
    for r in RESULTS:
        print("  %-14s %-6s %3dx%-3d anchor(%d,%d) fp %dx%d" % r)
