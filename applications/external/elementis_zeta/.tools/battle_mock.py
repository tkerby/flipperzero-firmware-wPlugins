#!/usr/bin/env python3
"""G-ELEMENTI device gate: true-size 128x64 battle mockups (style C) of the PixelLab creatures.

Writes build/elementi_gate/scene_<n>.png (6x LCD look) + all.png and style_gate/scenes.h (the
format the style_gate viewer app expects: 3 temporal-dither planes of 1024 bytes per scene).

  python3 -I tools/battle_mock.py (inputs are downloaded PNGs -> -I)

Creatures use tools/elementi_preview.adaptive_levels (the approved 4-level + mask + black
silhouette typecast); a scaled variant resamples the RGBA first, then converts.
"""
import math
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image, ImageDraw, ImageFont  # noqa: E402
import lvlib  # noqa: E402
from elementi_preview import adaptive_levels  # noqa: E402

SW, SH = 128, 64
OUT = os.path.join(ROOT, "build", "elementi_gate")
ELE = os.path.join(ROOT, "assets/art/elementis")
HERO = os.path.join(ROOT, "models_dl/pixellab/hero_b_std")
RIVAL = os.path.join(ROOT, "assets/art/npcs/rival")


# ---------------------------------------------------------------- font (font_v2.txt, 5x11, adv 6)
class Font:
    def __init__(self, path):
        self.g = {}
        lines = open(path, encoding="utf-8").read().split("\n")
        i = 0
        while i < len(lines):
            if lines[i].startswith("== "):
                tok = lines[i][3:].strip()
                ch = (
                    chr(int(tok[2:], 16))
                    if tok.startswith("U+") and len(tok) > 2
                    else tok
                )
                rows = []
                j = i + 1
                while j < len(lines) and lines[j] and set(lines[j]) <= set(".#"):
                    rows.append(lines[j])
                    j += 1
                self.g[ch] = [
                    (x, y)
                    for y, r in enumerate(rows)
                    for x, c in enumerate(r)
                    if c == "#"
                ]
                i = j
            else:
                i += 1

    def width(self, s):
        return 6 * len(s)


FONT = Font(os.path.join(ROOT, "assets/font/font_v2.txt"))


# ---------------------------------------------------------------- sprites
class Spr:
    def __init__(self, lv, mk):
        self.lv, self.mk = lv, mk
        self.h, self.w = len(lv), len(lv[0])
        self.name = ""


def to_spr(im):
    lv, mk = adaptive_levels(im)
    b = lvlib.mask_bbox(mk)
    return Spr(lvlib.crop(lv, *b), lvlib.crop(mk, *b))


def load(path, scale=1.0, max_h=None, max_w=None):
    """PixelLab PNG -> Spr.  scale / max_h / max_w resample the cropped RGBA first."""
    im = Image.open(path).convert("RGBA")
    bb = im.getchannel("A").point(lambda a: 255 if a >= 128 else 0).getbbox()
    im = im.crop(bb)
    s = scale
    if max_h:
        s = min(s, max_h / im.height)
    if max_w:
        s = min(s, max_w / im.width)
    if s != 1.0:
        im = im.resize(
            (max(1, round(im.width * s)), max(1, round(im.height * s))), Image.BOX
        )
    # adaptive_levels expects a canvas (edge pixels count as silhouette edge): pad by 1
    pad = Image.new("RGBA", (im.width + 2, im.height + 2), (0, 0, 0, 0))
    pad.paste(im, (1, 1))
    return to_spr(pad)


def creature(name, view, **kw):
    sp = load(os.path.join(ELE, name + "_c", view + ".png"), **kw)
    sp.name = "%s %s" % (name, view)
    return sp


# ---------------------------------------------------------------- canvas
class Canvas:
    def __init__(self):
        self.p = [[0] * SW for _ in range(SH)]

    def put(self, x, y, v):
        if 0 <= x < SW and 0 <= y < SH:
            self.p[y][x] = v

    def blit(self, spr, x0, y0, invert=False):
        for y in range(spr.h):
            for x in range(spr.w):
                if spr.mk[y][x]:
                    v = spr.lv[y][x]
                    self.put(x0 + x, y0 + y, 3 - v if invert else v)

    def rect(self, x0, y0, x1, y1, v):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                self.put(x, y, v)

    def box(self, x0, y0, x1, y1, round_corners=True):
        """white-filled 1 px black frame (corners trimmed)."""
        self.rect(x0, y0, x1, y1, 0)
        for x in range(x0 + 1, x1):
            self.put(x, y0, 3)
            self.put(x, y1, 3)
        for y in range(y0 + 1, y1):
            self.put(x0, y, 3)
            self.put(x1, y, 3)
        if round_corners:
            for x, y in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
                self.put(x, y, 0)
            for x, y in (
                (x0 + 1, y0 + 1),
                (x1 - 1, y0 + 1),
                (x0 + 1, y1 - 1),
                (x1 - 1, y1 - 1),
            ):
                self.put(x, y, 3)
        else:
            for x, y in ((x0, y0), (x1, y0), (x0, y1), (x1, y1)):
                self.put(x, y, 3)

    def text(self, s, x, y, v=3):
        """y = top of the 11-row cell (capitals occupy rows 2..8)."""
        for ch in s:
            for gx, gy in FONT.g[ch]:
                self.put(x + gx, y + gy, v)
            x += 6
        return x


# ---------------------------------------------------------------- platforms / plates
def platform(c, cx, cy, rx, ry, thick=2):
    """Iso ellipse slab: light top (centre white, rim level 1), 2-level side band, black outline."""

    def inside(x, y, ox, oy, k=1.0):
        return ((x - cx) / (rx * k)) ** 2 + ((y - cy - oy) / (ry * k)) ** 2 <= 1.0

    for y in range(cy - ry - 1, cy + ry + thick + 2):
        for x in range(cx - rx - 1, cx + rx + 2):
            top = inside(x, y, 0, 0)
            low = any(inside(x, y, 0, t) for t in range(1, thick + 1))
            if top:
                v = 0 if inside(x, y, 0, 0, 0.62) else 1
                c.put(x, y, v)
            elif low:
                c.put(x, y, 2)
    # outline: black where a filled pixel touches empty
    filled = lambda x, y: inside(x, y, 0, 0) or any(
        inside(x, y, 0, t) for t in range(1, thick + 1)
    )
    for y in range(cy - ry - 1, cy + ry + thick + 2):
        for x in range(cx - rx - 1, cx + rx + 2):
            if filled(x, y) and any(
                not filled(x + dx, y + dy)
                for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
            ):
                c.put(x, y, 3)
    # rim line separating the top face from the side band on the front
    for x in range(cx - rx, cx + rx + 1):
        for y in range(cy, cy + ry + 2):
            if (
                inside(x, y, 0, 0)
                and not inside(x, y + 1, 0, 0)
                and 0 <= y + 1 < SH
                and 0 <= x < SW
                and c.p[y + 1][x] == 2
            ):
                c.put(x, y, 3)


def shadow(c, cx, cy, rx, ry):
    """soft contact shadow on the platform: level 2 ellipse, level 1 halo."""
    for k, v in ((1.35, 1), (1.0, 2)):
        for y in range(int(cy - ry * k) - 1, int(cy + ry * k) + 2):
            for x in range(int(cx - rx * k) - 1, int(cx + rx * k) + 2):
                if (
                    ((x - cx) / (rx * k)) ** 2 + ((y - cy) / (ry * k)) ** 2 <= 1.0
                    and 0 <= x < SW
                    and 0 <= y < SH
                ):
                    if c.p[y][x] < v:
                        c.p[y][x] = v


def plate(c, x, y, name, lv, hp, w=None):
    """style name plate: framed, 'NAME' / 'Lv5 [bar]'. Returns (w, h)."""
    nw = FONT.width(name)
    w = w or max(nw, 18 + 2 + 18) + 6
    h = 19
    c.box(x, y, x + w - 1, y + h - 1)
    c.text(name, x + 3, y)
    lvs = "Lv%d" % lv
    c.text(lvs, x + 3, y + 8)
    bx0 = x + 3 + FONT.width(lvs) + 2
    bx1 = x + w - 4
    by0 = y + 11
    c.rect(bx0, by0, bx1, by0 + 4, 0)
    for xx in range(bx0, bx1 + 1):
        c.put(xx, by0, 3)
        c.put(xx, by0 + 4, 3)
    for yy in range(by0, by0 + 5):
        c.put(bx0, yy, 3)
        c.put(bx1, yy, 3)
    fill = int(round((bx1 - bx0 - 3) * hp))
    for xx in range(fill):
        for yy in (by0 + 1, by0 + 2, by0 + 3):
            c.put(bx0 + 2 + xx, yy, 3)
    return w, h


def bottom_box(c, lines=None, menu=False, h=26, cursor=(0, 0)):
    y0 = SH - h
    c.box(0, y0, SW - 1, SH - 1)
    if menu:
        items = [["LUCHAR", "MOCHILA"], ["EQUIPO", "HUIR"]]
        for r in range(2):
            for k in range(2):
                tx = 12 + k * 56
                ty = y0 + 1 + r * 11
                if cursor == (r, k):
                    c.text("▶", tx - 8, ty)
                c.text(items[r][k], tx, ty)
    else:
        for i, s in enumerate(lines):
            c.text(s, 4, y0 + 1 + i * 11)
    return y0


# ---------------------------------------------------------------- placement helpers
def stand(
    c,
    spr,
    right=None,
    left=None,
    bottom=None,
    top=None,
    plat=True,
    invert=False,
    plat_w=None,
    plat_ry=None,
    sh_k=0.55,
):
    """place a creature by one horizontal and one vertical edge; draw platform + shadow below."""
    x = (SW - spr.w - right) if right is not None else left
    y = (bottom - spr.h) if bottom is not None else top
    cx = x + spr.w // 2
    foot = y + spr.h - 3  # platform centre: feet overlap the slab by ~3 px
    rx = plat_w or max(16, min(30, int(spr.w * 0.62)))
    ry = plat_ry or max(4, rx // 3)
    if plat:
        platform(c, cx, foot, rx, ry)
        shadow(c, cx, foot, int(rx * sh_k * 1.15), max(2, int(ry * sh_k)))
    c.blit(spr, x, y, invert=invert)
    return x, y


def flame(c, cx, base, h, w, seed, lean=0.0):
    """one flame (teardrop with a curling tip): black outline, level-2 body, white-hot core, with a
    white halo so it reads on top of any creature art."""
    shape = {}
    for t in range(h):
        u = t / max(1, h - 1)
        half = (
            (w / 2.0)
            * math.sin(min(1.0, u * 3 + 0.35) * math.pi / 2)
            * (1 - u) ** 0.85
            * (0.8 + 0.2 * math.sin(u * 6 + seed))
        )
        sway = (math.sin(u * 2.4 + seed) * 0.35 + lean) * w * u**1.4
        for x in range(int(round(cx + sway - half)), int(round(cx + sway + half)) + 1):
            d = abs(x - (cx + sway)) / max(0.6, half)
            core = d < 0.45 and 0.08 < u < 0.62
            shape[(x, base - t)] = 1 if core else (3 if u > 0.72 else 2)
    for x, y in list(shape):
        for dx in (-2, -1, 0, 1, 2):
            for dy in (-2, -1, 0, 1, 2):
                if (x + dx, y + dy) not in shape and abs(dx) + abs(dy) <= 2:
                    c.put(x + dx, y + dy, 0)
    for (x, y), v in shape.items():
        c.put(x, y, v)
    for x, y in shape:
        if any(
            (x + dx, y + dy) not in shape
            for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))
        ):
            c.put(x, y, 3)


def fire(c, x0, y0, x1, y1):
    """fire over the target bbox (x0,y0)-(x1,y1): a big central flame, side tongues, embers."""
    W, H = x1 - x0, y1 - y0
    spec = [
        (0.50, 0.97, 26, 20, 0.3, 0.1),
        (0.16, 0.95, 20, 15, 1.7, -0.2),
        (0.84, 0.95, 20, 15, 2.9, 0.2),
        (0.33, 0.78, 16, 12, 4.1, -0.1),
        (0.68, 0.78, 16, 12, 5.3, 0.15),
    ]
    for fx, fy, h, w, seed, lean in spec:
        flame(c, int(x0 + W * fx), int(y0 + H * fy), h, w, seed, lean)
    for ex, ey in (
        (0.05, 0.25),
        (0.95, 0.30),
        (0.25, 0.05),
        (0.78, 0.08),
        (0.62, 0.0),
        (0.38, 0.15),
    ):
        x, y = int(x0 + W * ex), int(y0 + H * ey)
        c.rect(x - 1, y - 1, x + 2, y + 2, 0)
        c.rect(x, y, x + 1, y + 1, 3)


# ---------------------------------------------------------------- scenes
def hidden(spr, top_y, box_top):
    """fraction of the opaque pixels of a sprite placed at top_y that lie under a box starting at box_top"""
    tot = sum(map(sum, spr.mk))
    hid = sum(
        spr.mk[y][x] for y in range(spr.h) for x in range(spr.w) if top_y + y >= box_top
    )
    return hid / tot


def build():
    S = []
    info = {}

    def add(name, c):
        S.append((name, c.p))

    ember_f = creature("embercub", "south-west")
    puddle_b = creature("puddlet", "north-east")
    sprout_b = creature("sprouton", "north-east")
    for n, v in (
        ("embercub SW", ember_f),
        ("puddlet NE", puddle_b),
        ("sprouton NE", sprout_b),
        ("sprouton SW", creature("sprouton", "south-west")),
        ("puddlet SW", creature("puddlet", "south-west")),
        ("embercub NE", creature("embercub", "north-east")),
    ):
        info[n] = "%dx%d" % (v.w, v.h)
    BOX = 26
    BT = SH - BOX

    def full(en, pl, enn, pln, menu=False, lines=None, plates=True):
        c = Canvas()
        stand(c, en, right=3, top=1 if en.h < SH else 0)
        stand(c, pl, left=0, bottom=max(SH - 1, pl.h))
        if plates:
            plate(c, 1, 1, enn, 5, 1.0, w=54)
            plate(c, 71, BT - 19, pln, 5, 0.7, w=54)
        bottom_box(c, lines=lines, menu=menu, h=BOX)
        return c

    add("1 C-full menu", full(ember_f, puddle_b, "EMBERCUB", "PUDDLET", menu=True))
    add(
        "2 C-full texto",
        full(
            ember_f,
            puddle_b,
            "EMBERCUB",
            "PUDDLET",
            lines=["\u00a1EMBERCUB us\u00f3", "GARRA!"],
        ),
    )
    add("3 C-full brote", full(ember_f, sprout_b, "EMBERCUB", "SPROUTON", menu=True))

    c = Canvas()
    stand(c, ember_f, left=(SW - ember_f.w) // 2, top=1, plat_w=34, plat_ry=7)
    plate(c, 1, 1, "EMBERCUB", 5, 1.0, w=54)
    add("4 FOCUS rival", c)

    c = Canvas()
    stand(c, puddle_b, left=2, bottom=SH - 1, plat_w=33, plat_ry=7)
    plate(c, 71, 1, "EMBERCUB", 5, 1.0, w=54)
    plate(c, 71, 22, "PUDDLET", 5, 0.7, w=54)
    add("5 FOCUS jugador", c)

    rv = load(os.path.join(RIVAL, "south-west.png"))
    hb = load(os.path.join(HERO, "north-east.png"))
    info["rival SW"] = "%dx%d" % (rv.w, rv.h)
    info["hero NE"] = "%dx%d" % (hb.w, hb.h)
    c = Canvas()
    stand(c, rv, right=22, top=0, plat_w=21, plat_ry=5)
    stand(c, hb, left=22, bottom=BT - 5, plat_w=21, plat_ry=5)
    bottom_box(c, lines=["\u00a1GAEL quiere pelear!"], h=BOX)
    add("6 INTRO rival", c)

    # 7 COMPACT: one-line box; both creatures scaled just enough to be fully visible above it
    box_h = 14
    c = Canvas()
    es = creature("embercub", "south-west", max_h=SH - box_h - 8)
    ps = creature("puddlet", "north-east", max_h=SH - box_h - 4)
    stand(c, es, right=3, top=2)
    stand(c, ps, left=0, bottom=SH - box_h - 1)
    plate(c, 1, 1, "EMBERCUB", 5, 1.0, w=54)
    bottom_box(c, lines=["\u00a1EMBERCUB us\u00f3 GARRA!"], h=box_h)
    info["COMPACT embercub"] = "%dx%d" % (es.w, es.h)
    info["COMPACT puddlet"] = "%dx%d" % (ps.w, ps.h)
    add("7 COMPACT 1 linea", c)

    # 8 HIT: enemy inverted + fire over it (focus composition, native size)
    c = Canvas()
    x, y = stand(
        c, ember_f, left=(SW - ember_f.w) // 2, top=1, plat_w=34, plat_ry=7, invert=True
    )
    fire(c, x, y + 8, x + ember_f.w, y + ember_f.h - 2)
    plate(c, 1, 1, "EMBERCUB", 5, 0.45, w=54)
    add("8 HIT fuego", c)

    # 9 C-escala: layout that fits the 26 px box (creatures scaled to ~30/36 px)
    c = Canvas()
    es = creature("embercub", "south-west", max_h=30)
    ps = creature("puddlet", "north-east", max_h=36)
    stand(c, es, right=4, top=2)
    stand(c, ps, left=6, bottom=BT - 2)
    bottom_box(c, menu=True, h=BOX)
    info["C-escala embercub"] = "%dx%d" % (es.w, es.h)
    info["C-escala puddlet"] = "%dx%d" % (ps.w, ps.h)
    add("9 C-escala sin placas", c)

    # 10 C-full without plates (box only) to see the raw overlap
    add(
        "10 C-full sin placas", full(ember_f, puddle_b, "", "", menu=True, plates=False)
    )

    info["puddlet NE hidden by 26px box"] = "%.0f%% of pixels (rows %d..%d of 62)" % (
        100 * hidden(puddle_b, SH - puddle_b.h, BT),
        BT - (SH - puddle_b.h),
        puddle_b.h - 1,
    )
    info["embercub SW hidden by box"] = "%.0f%% (top y=1)" % (
        100 * hidden(ember_f, 1, BT)
    )
    return S, info


# ---------------------------------------------------------------- output
def lcd(levels, scale=6):
    return lvlib.lcd_image(levels, None, scale=scale, grid=True)


def planes(levels):
    out = []
    for s in range(3):
        buf = bytearray(SW * SH // 8)
        for y in range(SH):
            for x in range(SW):
                if ((s + (x + y) % 3) % 3) < levels[y][x]:
                    buf[y * (SW // 8) + (x >> 3)] |= 1 << (x & 7)
        out.append(bytes(buf))
    return out


def write_header(scenes, path):
    with open(path, "w") as f:
        f.write(
            "// generated by tools/battle_mock.py -- do not edit\n#pragma once\n#include <stdint.h>\n"
        )
        f.write("#define SG_N %d\n" % len(scenes))
        f.write(
            "static const char* const sg_names[SG_N] = {%s};\n"
            % ", ".join('"%s"' % n for n, _ in scenes)
        )
        f.write(
            "// [scene][sub-frame][1024] 1-bit planes, LSB-first (canvas_draw_xbm), bit set = black\n"
        )
        f.write("static const uint8_t sg_planes[SG_N][3][1024] = {\n")
        for _, lv in scenes:
            f.write(" {\n")
            for pl in planes(lv):
                f.write("  {" + ",".join(str(b) for b in pl) + "},\n")
            f.write(" },\n")
        f.write("};\n")


def label_font(sz):
    for p in (
        "/System/Library/Fonts/Helvetica.ttc",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
    ):
        try:
            return ImageFont.truetype(p, sz)
        except Exception:
            pass
    return ImageFont.load_default()


def main():
    os.makedirs(OUT, exist_ok=True)
    scenes, info = build()
    for n, (name, lv) in enumerate(scenes, 1):
        lcd(lv, 6).save(os.path.join(OUT, "scene_%d.png" % n))
    scale, pad, th, cols = 4, 10, 24, 2
    cw, ch = SW * scale, SH * scale
    rows = (len(scenes) + cols - 1) // cols
    sheet = Image.new(
        "RGB", (cols * (cw + pad) + pad, rows * (ch + th + pad) + pad), (40, 40, 40)
    )
    d = ImageDraw.Draw(sheet)
    f = label_font(16)
    for k, (name, lv) in enumerate(scenes):
        cx = pad + (k % cols) * (cw + pad)
        cy = pad + (k // cols) * (ch + th + pad)
        d.text((cx, cy + 3), name, fill=(255, 255, 255), font=f)
        sheet.paste(lcd(lv, scale), (cx, cy + th))
    sheet.save(os.path.join(OUT, "all.png"))
    write_header(scenes, os.path.join(ROOT, "style_gate", "scenes.h"))
    for k, v in info.items():
        print("%-32s %s" % (k, v))
    print("wrote %d scenes" % len(scenes))


if __name__ == "__main__":
    main()
