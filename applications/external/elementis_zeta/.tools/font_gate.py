#!/usr/bin/env python3
"""FONT GATE: render style dialogue boxes / the pause menu at true device size (128x64,
1-bit) for several candidate fonts, write LCD-look PNGs (build/fontgate/scene_<n>.png, all.png)
and the Flipper viewer header (style_gate/scenes.h, same layout the style_gate app expects:
3 planes of 1024 bytes per scene, LSB-first, bit set = black; every plane is identical here
because the scenes are pure 1-bit).

  python3 tools/font_gate.py

Self-contained on purpose (own BDF reader / txt-font reader, independent of mkfont.py).
Fonts:
  v1 assets/font/font.txt 5x10 cell, 6 px advance
  v2 assets/font/font_v2.txt 5x11 cell, 6 px advance
  Fusion Pixel Font (TakWolf, OFL-1.1) BDFs under models_dl/fonts/fusion/<name>/
"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FUSION = os.path.join(ROOT, "models_dl", "fonts", "fusion")
OUT = os.path.join(ROOT, "build", "fontgate")
SW, SH = 128, 64
LCD_BG = (255, 139, 41)
LCD_INK = (24, 14, 6)

# text box geometry: 120 px of text, 1 px white gap, 1 px frame -> x 2..125
BOX_X0, BOX_X1 = 2, 125
TEXT_X = 4
BOX_BOTTOM = 62  # last row of the frame (row 63 stays background)


# --------------------------------------------------------------------------- fonts
class TxtFont:
    """ASCII-art font file ('== <char|U+XXXX>' + H rows of 5 chars), advance 6."""

    def __init__(self, path, base_off, name):
        self.name, self.base_off, self.adv = name, base_off, 6
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
                self.g[ch] = rows
                i = j
            else:
                i += 1

    def glyph(self, ch):
        rows = self.g.get(ch)
        if rows is None:
            raise KeyError("%s: no glyph %r" % (self.name, ch))
        px = [
            (x, y - self.base_off)
            for y, r in enumerate(rows)
            for x, c in enumerate(r)
            if c == "#"
        ]
        return self.adv, px


class BdfFont:
    """Minimal BDF reader: loads only the codepoints that are asked for (files are 3-5 MB)."""

    def __init__(self, path, chars, name, base_off):
        self.name, self.base_off = name, base_off
        want = {ord(c) for c in chars}
        self.g = {}
        cur, d, inbmp = None, None, False
        with open(path, encoding="utf-8") as f:
            for ln in f:
                if ln.startswith("ENCODING"):
                    cur, d, inbmp = int(ln.split()[1]), {"bm": []}, False
                elif cur is None:
                    continue
                elif ln.startswith("DWIDTH"):
                    d["dw"] = int(ln.split()[1])
                elif ln.startswith("BBX"):
                    d["bbx"] = tuple(int(v) for v in ln.split()[1:5])
                elif ln.startswith("BITMAP"):
                    inbmp = True
                elif ln.startswith("ENDCHAR"):
                    if cur in want:
                        self.g[chr(cur)] = d
                    cur, inbmp = None, False
                elif inbmp:
                    d["bm"].append(ln.strip())

    def glyph(self, ch):
        d = self.g.get(ch)
        if d is None:
            raise KeyError("%s: no glyph %r" % (self.name, ch))
        w, h, xo, yo = d["bbx"]
        px = []
        for r, hexrow in enumerate(d["bm"]):
            bits = int(hexrow, 16)
            nbits = len(hexrow) * 4
            for x in range(w):
                if (bits >> (nbits - 1 - x)) & 1:
                    # row r of h (0 = top); baseline row index = h + yo below the top
                    px.append((xo + x, r - (h + yo)))
        return d["dw"], px

    def top_ext(self, text):
        """rows above the baseline used by `text`."""
        m = 0
        for ch in set(text):
            if ch in self.g:
                _, _, _, yo = self.g[ch]["bbx"]
                m = max(m, self.g[ch]["bbx"][1] + yo)
        return m


def text_width(font, s):
    return sum(font.glyph(c)[0] for c in s)


# --------------------------------------------------------------------------- drawing
def new_screen():
    return [[0] * SW for _ in range(SH)]


def put(img, x, y, v=1):
    if 0 <= x < SW and 0 <= y < SH:
        img[y][x] = v


def draw_text(img, font, s, x, base_y):
    pen = x
    for ch in s:
        adv, px = font.glyph(ch)
        for dx, dy in px:
            put(img, pen + dx, base_y + dy)
        pen += adv
    return pen - x


def floor(img):
    """light iso floor: dotted 2:1 diamond grid (tile 28x14), every other pixel."""
    for i in range(-8, 9):
        for j in range(-8, 9):
            cx, cy = 64 + (i - j) * 14, 32 + (i + j) * 7
            for t in range(0, 14):
                y = cy - 7 + t // 2
                for x in (cx - t, cx + t):
                    if (x + y) % 3 == 0:
                        put(img, x, y)
            for t in range(0, 14):
                y = cy + (t // 2)
                for x in (cx - 14 + t, cx + 14 - t):
                    if (x + y) % 3 == 0:
                        put(img, x, y)


def frame_box(img, x0, y0, x1, y1):
    for y in range(y0, y1 + 1):
        for x in range(x0, x1 + 1):
            img[y][x] = 0
    for x in range(x0 + 1, x1):
        put(img, x, y0)
        put(img, x, y1)
    for y in range(y0 + 1, y1):
        put(img, x0, y)
        put(img, x1, y)


def dialogue(font, lines, pitch, base_off=None, bg=floor):
    img = new_screen()
    if bg:
        bg(img)
    h = 2 * pitch + 4
    y0 = BOX_BOTTOM - h + 1
    frame_box(img, BOX_X0, y0, BOX_X1, BOX_BOTTOM)
    bo = font.base_off if base_off is None else base_off
    for k, s in enumerate(lines):
        w = text_width(font, s)
        if w > 120:
            print("  WARNING %s: line %r is %d px wide (> 120)" % (font.name, s, w))
        draw_text(img, font, s, TEXT_X, y0 + 2 + k * pitch + bo)
    return img


def menu(font, items, sel, pitch, cursor_font=None):
    """pause menu: framed box top-right, one line per item, cursor on `sel`."""
    img = new_screen()
    floor(img)
    cf = cursor_font or font
    cw = cf.glyph("▶")[0]
    textw = max(text_width(font, s) for s in items)
    w = cw + textw + 6
    h = len(items) * pitch + 4
    x1 = 126
    x0 = x1 - w + 1
    y0 = 1
    frame_box(img, x0, y0, x1, y0 + h - 1)
    for k, s in enumerate(items):
        by = y0 + 2 + k * pitch + font.base_off
        if k == sel:
            draw_text(img, cf, "▶", x0 + 2, by)
        draw_text(img, font, s, x0 + 2 + cw, by)
    return img


# --------------------------------------------------------------------------- output
def planes(img):
    buf = bytearray(SW * SH // 8)
    for y in range(SH):
        for x in range(SW):
            if img[y][x]:
                buf[y * (SW // 8) + (x >> 3)] |= 1 << (x & 7)
    return bytes(buf)


def lcd(img, scale):
    im = Image.new("RGB", (SW * scale, SH * scale), LCD_BG)
    p = im.load()
    for y in range(SH):
        for x in range(SW):
            if img[y][x]:
                for dy in range(scale - 1):
                    for dx in range(scale - 1):
                        p[x * scale + dx, y * scale + dy] = LCD_INK
    return im


def write_header(scenes, path):
    with open(path, "w") as f:
        f.write(
            "// generated by tools/font_gate.py -- do not edit\n#pragma once\n#include <stdint.h>\n"
        )
        f.write("#define SG_N %d\n" % len(scenes))
        f.write(
            "static const char* const sg_names[SG_N] = {%s};\n"
            % ", ".join('"%s"' % s["name"] for s in scenes)
        )
        f.write(
            "// [scene][sub-frame][1024] 1-bit planes, LSB-first (canvas_draw_xbm), bit set = black\n"
        )
        f.write("static const uint8_t sg_planes[SG_N][3][1024] = {\n")
        for s in scenes:
            pl = ",".join(str(b) for b in planes(s["img"]))
            f.write(" {\n")
            for _ in range(3):
                f.write("  {" + pl + "},\n")
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


# --------------------------------------------------------------------------- scenes
ES1 = ["¡Hola, cariño!", "La profesora Lin"]
ES4 = ["Esta máquina analiza", "datos de criaturas."]
EN5 = ["Hi, honey! Professor", "Lin was looking for"]
JA6 = ["やあ こんにちは！", "わたしは リン です。"]
ZH7 = ["你好！我是林教授。", "欢迎来到生物的世界！"]
MENU = ["Equipo", "Mochila", "Guardar", "Opciones", "Salir"]
SPECIAL = "áéíóúÁÉÍÓÚñÑüÜ¿¡▼▶…"
LATIN = "".join(chr(c) for c in range(0x20, 0x7F)) + SPECIAL


def fpath(folder, fname):
    return os.path.join(FUSION, folder, fname)


def build():
    v1 = TxtFont(os.path.join(ROOT, "assets/font/font.txt"), 8, "v1")
    v2 = TxtFont(os.path.join(ROOT, "assets/font/font_v2.txt"), 9, "v2")
    f10 = BdfFont(
        fpath("10px-proportional", "fusion-pixel-10px-proportional-latin.bdf"),
        LATIN,
        "fusion10p",
        9,
    )
    f10m = BdfFont(
        fpath("10px-monospaced", "fusion-pixel-10px-monospaced-latin.bdf"),
        LATIN,
        "fusion10m",
        9,
    )
    ja8 = BdfFont(
        fpath("8px-proportional", "fusion-pixel-8px-proportional-ja.bdf"),
        "".join(JA6),
        "fusion8ja",
        8,
    )
    zh12 = BdfFont(
        fpath("12px-proportional", "fusion-pixel-12px-proportional-zh_hans.bdf"),
        "".join(ZH7),
        "fusion12zh",
        10,
    )
    zh10 = BdfFont(
        fpath("10px-proportional", "fusion-pixel-10px-proportional-zh_hans.bdf"),
        "".join(ZH7),
        "fusion10zh",
        9,
    )
    # CJK baseline offsets follow the real glyph extents
    ja8.base_off = ja8.top_ext("".join(JA6))
    zh12.base_off = zh12.top_ext("".join(ZH7))
    zh10.base_off = zh10.top_ext("".join(ZH7))
    P = 12
    S = []

    def add(name, title, img):
        S.append({"name": name, "title": title, "img": img})

    add("S1 v1 actual 5x10", "S1  v1 (font.txt 5x10)  ES", dialogue(v1, ES1, P))
    add("S2 v2 5x11", "S2  v2 (font_v2 5x11)  ES", dialogue(v2, ES1, P))
    add("S3 Fusion10 prop", "S3  Fusion 10px proportional  ES", dialogue(f10, ES1, P))
    add("S3b Fusion10 mono", "S3b Fusion 10px monospaced  ES", dialogue(f10m, ES1, P))
    add("S4a v1 stress", "S4a v1  'Esta máquina analiza'", dialogue(v1, ES4, P))
    add("S4b v2 stress", "S4b v2  'Esta máquina analiza'", dialogue(v2, ES4, P))
    add("S4c Fusion10 stress", "S4c Fusion 10px prop  stress", dialogue(f10, ES4, P))
    add("S4d Fusion10m stress", "S4d Fusion 10px mono  stress", dialogue(f10m, ES4, P))
    add("S5a v2 English", "S5a v2  EN", dialogue(v2, EN5, P))
    add("S5b Fusion10 English", "S5b Fusion 10px prop  EN", dialogue(f10, EN5, P))
    add("S6 Fusion8 kana", "S6  Fusion 8px ja  kana", dialogue(ja8, JA6, 10))
    add("S7 Fusion12 zh", "S7  Fusion 12px zh_hans", dialogue(zh12, ZH7, P))
    add("S8 Fusion10 zh", "S8  Fusion 10px zh_hans", dialogue(zh10, ZH7, P))
    add("S9a menu v2", "S9a menu v2", menu(v2, MENU, 1, 11))
    add("S9b menu Fusion10", "S9b menu Fusion 10px prop", menu(f10, MENU, 1, 11))
    return S


def glyph_sheet(font, path, chars=LATIN, cols=32, scale=8):
    """every glyph of a txt font on a 6x(2*11) grid, LCD look (design review aid)."""
    cw, chh = 7, 13
    rows = (len(chars) + cols - 1) // cols
    W, H = cols * cw, rows * chh
    im = Image.new("RGB", (W * scale, H * scale), LCD_BG)
    p = im.load()
    for k, ch in enumerate(chars):
        gx, gy = (k % cols) * cw, (k // cols) * chh
        _, px = font.glyph(ch)
        for dx, dy in px:
            for yy in range(scale - 1):
                for xx in range(scale - 1):
                    p[
                        (gx + dx) * scale + xx, (gy + dy + font.base_off) * scale + yy
                    ] = LCD_INK
    im.save(path)


def main():
    os.makedirs(OUT, exist_ok=True)
    glyph_sheet(
        TxtFont(os.path.join(ROOT, "assets/font/font_v2.txt"), 9, "v2"),
        os.path.join(OUT, "v2_glyphs.png"),
        cols=24,
    )
    scenes = build()
    for n, s in enumerate(scenes, 1):
        lcd(s["img"], 6).save(os.path.join(OUT, "scene_%d.png" % n))
    scale, pad, th, cols = 3, 10, 22, 3
    cw, ch = SW * scale, SH * scale
    rows = (len(scenes) + cols - 1) // cols
    sheet = Image.new(
        "RGB", (cols * (cw + pad) + pad, rows * (ch + th + pad) + pad), (40, 40, 40)
    )
    d = ImageDraw.Draw(sheet)
    f = label_font(14)
    for k, s in enumerate(scenes):
        cx = pad + (k % cols) * (cw + pad)
        cy = pad + (k // cols) * (ch + th + pad)
        d.text((cx, cy + 2), s["title"], fill=(255, 255, 255), font=f)
        sheet.paste(lcd(s["img"], scale), (cx, cy + th))
    sheet.save(os.path.join(OUT, "all.png"))
    write_header(scenes, os.path.join(ROOT, "style_gate", "scenes.h"))
    print(
        "wrote %d scenes: build/fontgate/scene_*.png, all.png, style_gate/scenes.h"
        % len(scenes)
    )


if __name__ == "__main__":
    sys.exit(main())
