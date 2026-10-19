#!/usr/bin/env python3
"""Battle plate mockups (normal view): the current compact plates vs readable redesigns, in the game's layout
(enemy front picture top-right, the player's back picture bottom-left, text / menu box at the bottom).

  python3 -I tools/plate_mock.py      -> build/plate_gate/sheet.png + appends the scenes to build/plate_gate/anim.bin
                                         (same format as tools/title_mock.py, one frame per scene)
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image, ImageDraw  # noqa: E402
import battle_mock as bm  # noqa: E402
import lvlib  # noqa: E402

SW, SH = 128, 64
OUT = os.path.join(ROOT, "build", "plate_gate")
FONT = bm.FONT

TINY = {  # hud_gfx.c 3x5
    "0": ["111", "101", "101", "101", "111"], "1": ["010", "110", "010", "010", "111"],
    "2": ["111", "001", "111", "100", "111"], "3": ["111", "001", "111", "001", "111"],
    "4": ["101", "101", "111", "001", "001"], "5": ["111", "100", "111", "001", "111"],
    "6": ["111", "100", "111", "101", "111"], "7": ["111", "001", "001", "010", "010"],
    "8": ["111", "101", "111", "101", "111"], "9": ["111", "101", "111", "001", "111"],
    "/": ["001", "001", "010", "100", "100"], "L": ["100", "100", "100", "100", "111"],
    "P": ["111", "101", "111", "100", "100"], "S": ["111", "100", "111", "001", "111"],
    "N": ["1001", "1101", "1011", "1001", "1001"], "v": ["000", "000", "101", "101", "010"],
}
TINY3 = dict(TINY)
TINY3["3"] = ["110", "001", "010", "001", "110"]  # rounded 3: the full middle bar made 'L3' read as 'LE'
TINY3["1"] = ["110", "110", "010", "010", "111"]  # contract 78: 2 px thick serif, a lone 1 next to the bar's black end / a "/" was lost



def tiny(c, s, x, y, g=None):
    g = g or TINY
    for ch in s:
        for r, row in enumerate(g[ch]):
            for k, b in enumerate(row):
                if b == "1":
                    c.put(x + k, y + r, 3)
        x += len(g[ch][0]) + 1
    return x - 1


def tiny_w(s):
    return sum(len(TINY3[ch][0]) + 1 for ch in s) - 1 if s else 0


def ftext(c, s, x, y):
    """game font; y = top of the CAPITALS (the 11-row cell starts 2 rows higher)."""
    return c.text(s, x, y - 2)


def fw(s):
    return 6 * len(s) - 1


def fill(c, x, y, w, h, v):
    c.rect(x, y, x + w - 1, y + h - 1, v)


def frame(c, x, y, w, h, v=3):
    for xx in range(x, x + w):
        c.put(xx, y, v)
        c.put(xx, y + h - 1, v)
    for yy in range(y, y + h):
        c.put(x, yy, v)
        c.put(x + w - 1, yy, v)


def bar(c, x, y, w, h, frac, rounded=False):
    """framed HP bar, inner fill black; h = total height (frame included)."""
    fill(c, x, y, w, h, 0)
    frame(c, x, y, w, h)
    inner = w - 2
    n = max(1 if frac > 0 else 0, round(inner * frac))
    fill(c, x + 1, y + 1, n, h - 2, 3)
    if rounded:
        for xx, yy in ((x, y), (x + w - 1, y), (x, y + h - 1), (x + w - 1, y + h - 1)):
            c.put(xx, yy, 0)


# ---------------------------------------------------------------- plate styles: (canvas, fighter, enemy?, bottom y)
def p_current(c, f, enemy, ui_top):
    lv = "L%d" % f["lv"]
    if enemy:
        w, h = 2 + tiny_w(lv) + 2 + 28 + 3, 7
        x, y = 0, 0
        fill(c, x, y, w, h, 0)
        fill(c, x, y + h - 1, w, 1, 3)
        fill(c, x + w - 1, y, 1, h, 3)
        ix, by = x + 2, y + 1
    else:
        nums = "%d/%d" % (f["hp"], f["max"])
        w, h = 3 + tiny_w(lv) + 2 + 28 + 2 + tiny_w(nums) + 1, 8
        x, y = SW - w, ui_top - h + 1
        fill(c, x, y, w, h, 0)
        fill(c, x, y, w, 1, 3)
        fill(c, x, y, 1, h, 3)
        ix, by = x + 3, y + 2
    tiny(c, lv, ix, by)
    bx = ix + tiny_w(lv) + 2
    bar(c, bx, by, 28, 5, f["hp"] / f["max"])
    if not enemy:
        tiny(c, "%d/%d" % (f["hp"], f["max"]), bx + 30, by)


def p_a(c, f, enemy, ui_top):
    """The classic plate: name + level in the game font, then 'PS' + a thick bar; the player's adds the numbers."""
    name, lv = f["name"], "Nv%d" % f["lv"]
    bw = 40
    w = max(fw(name) + 4 + fw(lv), 2 + tiny_w("PS") + 2 + bw) + 5
    h = 19 if enemy else 19
    if enemy:
        x, y = 0, 0
        fill(c, x, y, w, h, 0)
        fill(c, x, y + h - 1, w, 1, 3)
        fill(c, x + w - 1, y, 1, h, 3)
    else:
        x, y = SW - w, ui_top - h + 1
        fill(c, x, y, w, h, 0)
        fill(c, x, y, w, 1, 3)
        fill(c, x, y, 1, h, 3)
    tx = x + 2 if enemy else x + 3
    ty = y + 2 if enemy else y + 3
    ftext(c, name, tx, ty)
    ftext(c, lv, x + w - (2 if not enemy else 3) - fw(lv) - (0 if enemy else 0), ty)
    by = ty + 9
    tiny(c, "PS", tx, by)
    bar(c, tx + tiny_w("PS") + 2, by - 1, w - (tx - x) - tiny_w("PS") - 2 - 3, 7, f["hp"] / f["max"], rounded=True)


def p_b(c, f, enemy, ui_top):
    """B 'compacta legible': one strip like today, level in the game font, a 7 px bar; the player's numbers in the
    game font too."""
    lv = "Nv%d" % f["lv"]
    bw = 34
    nums = "%d/%d" % (f["hp"], f["max"])
    w = 2 + fw(lv) + 3 + bw + 3 + ((fw(nums) + 2) if not enemy else 0)
    h = 11
    if enemy:
        x, y = 0, 0
        fill(c, x, y, w, h, 0)
        fill(c, x, y + h - 1, w, 1, 3)
        fill(c, x + w - 1, y, 1, h, 3)
        ty = y + 2
    else:
        x, y = SW - w, ui_top - h + 1
        fill(c, x, y, w, h, 0)
        fill(c, x, y, w, 1, 3)
        fill(c, x, y, 1, h, 3)
        ty = y + 2
    ix = x + 2 if enemy else x + 3
    ftext(c, lv, ix, ty)
    bx = ix + fw(lv) + 3
    bar(c, bx, ty, bw, 7, f["hp"] / f["max"], rounded=True)
    if not enemy:
        ftext(c, nums, bx + bw + 3, ty)


def p_c(c, f, enemy, ui_top):
    """C 'nombre arriba': name + level on one line, a full-width 6 px bar under it; the player's numbers sit in the
    bar's line on the right (no PS label: the bar under a name reads as life)."""
    name, lv = f["name"], "Nv%d" % f["lv"]
    nums = "%d/%d" % (f["hp"], f["max"])
    w = fw(name) + 4 + fw(lv) + 5
    if not enemy:
        w = max(w, 30 + 3 + fw(nums) + 5)
    h = 18
    if enemy:
        x, y = 0, 0
        fill(c, x, y, w, h, 0)
        fill(c, x, y + h - 1, w, 1, 3)
        fill(c, x + w - 1, y, 1, h, 3)
        tx = x + 2
    else:
        x, y = SW - w, ui_top - h + 1
        fill(c, x, y, w, h, 0)
        fill(c, x, y, w, 1, 3)
        fill(c, x, y, 1, h, 3)
        tx = x + 3
    ty = y + 2
    ftext(c, name, tx, ty)
    ftext(c, lv, x + w - 3 - fw(lv), ty)
    by = ty + 9
    if enemy:
        bar(c, tx, by, w - (tx - x) - 3, 6, f["hp"] / f["max"], rounded=True)
    else:
        bw = w - (tx - x) - 3 - fw(nums) - 3
        bar(c, tx, by, bw, 6, f["hp"] / f["max"], rounded=True)
        ftext(c, nums, tx + bw + 3, by - 1)


def p_tweak(c, f, enemy, ui_top, label=False):
    """today's plate, small fixes: rounded '3', 'Nv' instead of 'L', 1 px more air at the screen edge, a 6 px bar
    (4 px of black), optional 'PS' label."""
    lv = "Nv%d" % f["lv"]
    lab = "PS" if label else ""
    lw = tiny_w(lv) + 3 + ((tiny_w(lab) + 2) if label else 0)
    if enemy:
        nums = "%d/%d" % (f["hp"], f["max"])
        w, h = 2 + lw + 28 + 3 + tiny_w(nums) + 3, 9
        x, y = 0, 0
        fill(c, x, y, w, h, 0)
        fill(c, x, y + h - 1, w, 1, 3)
        fill(c, x + w - 1, y, 1, h, 3)
        ix, by = x + 2, y + 2
    else:
        nums = "%d/%d" % (f["hp"], f["max"])
        w, h = 3 + lw + 28 + 3 + tiny_w(nums) + 1, 9
        x, y = SW - w, ui_top - h + 1
        fill(c, x, y, w, h, 0)
        fill(c, x, y, w, 1, 3)
        fill(c, x, y, 1, h, 3)
        ix, by = x + 3, y + 2
    tiny(c, lv, ix, by, TINY3)
    bx = ix + tiny_w(lv) + 3
    if label:
        tiny(c, lab, bx, by, TINY3)
        bx += tiny_w(lab) + 2
    bar(c, bx, by - 0, 28, 6, f["hp"] / f["max"])
    tiny(c, "%d/%d" % (f["hp"], f["max"]), bx + 31, by, TINY3)


def p_tweak_ps(c, f, enemy, ui_top):
    p_tweak(c, f, enemy, ui_top, label=True)


STYLES = [("actual", p_current), ("ajuste 1", p_tweak), ("ajuste 2 PS", p_tweak_ps)]


def scene(style, menu, en_hp, pl_hp):
    c = bm.Canvas()
    en = bm.creature("puddlet", "south-west", max_h=42)
    pl = bm.creature("embercub", "north-east", max_h=46)
    bm.stand(c, en, right=3, top=1)
    bm.stand(c, pl, left=0, bottom=SH - 1)
    box_h = 26
    if menu:
        y0 = bm.bottom_box(c, menu=True, h=box_h)
    else:
        y0 = bm.bottom_box(c, lines=["¡Puddlet usó", "Burbuja!"], h=box_h)
    efi = {"name": "Puddlet", "lv": 3, "hp": en_hp, "max": 20}
    pfi = {"name": "Embercub", "lv": 13, "hp": pl_hp, "max": 33}
    style(c, efi, True, None)
    style(c, pfi, False, y0)
    return c.p


def pack(p):
    b = bytearray(SW * SH // 4)
    for y in range(SH):
        for x in range(SW):
            b[y * (SW // 4) + (x >> 2)] |= (p[y][x] & 3) << ((x & 3) * 2)
    return bytes(b)


def main():
    os.makedirs(OUT, exist_ok=True)
    scenes = []
    for name, st in STYLES:
        scenes.append(("%s menu" % name, scene(st, True, 20, 33)))
        scenes.append(("%s texto baja" % name, scene(st, False, 6, 13)))
    with open(os.path.join(OUT, "anim.bin"), "wb") as f:
        f.write(b"AGT1" + bytes([len(scenes)]))
        for name, p in scenes:
            f.write(name.encode()[:23].ljust(24, b"\0"))
            f.write(struct.pack("<HH", 1, 500))
            f.write(pack(p))
    scale, pad, th = 4, 10, 20
    cols = 2
    rows = (len(scenes) + 1) // 2
    sheet = Image.new("RGB", (cols * (SW * scale + pad) + pad, rows * (SH * scale + th + pad) + pad), (40, 40, 40))
    d = ImageDraw.Draw(sheet)
    fnt = bm.label_font(14)
    for k, (name, p) in enumerate(scenes):
        x = pad + (k % cols) * (SW * scale + pad)
        y = pad + (k // cols) * (SH * scale + th + pad)
        d.text((x, y + 2), name, fill=(255, 255, 255), font=fnt)
        sheet.paste(lvlib.lcd_image(p, None, scale=scale, grid=False), (x, y + th))
    sheet.save(os.path.join(OUT, "sheet.png"))
    print("wrote", OUT)


if __name__ == "__main__":
    main()
