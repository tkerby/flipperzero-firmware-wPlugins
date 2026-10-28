#!/usr/bin/env python3
"""Render the simulator's draw trace into PNGs that look like the device.

sim/run.c links the app's real draw and input code against a shim that emits this trace;
this turns the trace into pixels. Keeping rasterisation on the Python side means the sim does
not have to ship a 5x7 font table that would only approximate the device's glyphs anyway.

Usage:  ./render.py trace.txt out_dir
"""
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFont

W, H = 128, 64
SCALE = 6

FONT_FILES = {}


def font(size: int, bold: bool = False):
    """A small monospace face, close enough to the device's 5x7 to judge layout."""
    key = (size, bold)
    if key not in FONT_FILES:
        from matplotlib import font_manager

        name = "DejaVu Sans Mono" if not bold else "DejaVu Sans Mono"
        path = font_manager.findfont(font_manager.FontProperties(family=name, weight="bold" if bold else "normal"))
        FONT_FILES[key] = ImageFont.truetype(path, size)
    return FONT_FILES[key]


def text_mask(s: str, size: int, bold=False) -> np.ndarray:
    f = font(size, bold)
    w = max(1, int(round(f.getlength(s))))
    img = Image.new("L", (max(w, 1) + 4, size + 6), 0)
    d = ImageDraw.Draw(img)
    d.text((0, 0), s, fill=255, font=f)
    return np.array(img) > 96


class Canvas:
    def __init__(self):
        self.px = np.zeros((H, W), dtype=bool)

    def set(self, x0, y0, x1, y1, color):
        x0, x1 = max(0, min(x0, x1)), min(W - 1, max(x0, x1))
        y0, y1 = max(0, min(y0, y1)), min(H - 1, max(y0, y1))
        if x1 < 0 or y1 < 0 or x0 >= W or y0 >= H:
            return
        if color == 2:  # XOR: invert what is underneath, which is what the device does
            self.px[y0:y1 + 1, x0:x1 + 1] ^= True
        else:
            self.px[y0:y1 + 1, x0:x1 + 1] = (color == 0)

    def line(self, x0, y0, x1, y1, color):
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        err = dx + dy
        while True:
            self.set(x0, y0, x0, y0, color)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def text(self, x, y, s, color, ha=0, va=0, size=7, bold=False):
        m = text_mask(s, size, bold)
        h, w = m.shape
        if ha == 1:
            x -= w
        elif ha == 2:
            x -= w // 2
        if va == 1:
            y -= h
        elif va == 2:
            y -= h // 2
        for yy in range(h):
            for xx in range(w):
                if m[yy, xx]:
                    self.set(x + xx, y + yy, x + xx, y + yy, color)


def render(trace: str, out: Path):
    c = Canvas()
    color = 0
    size = 7
    bold = False
    scenes = []
    current = None
    for raw in trace.splitlines():
        parts = raw.split(" ", 1)
        op = parts[0]
        a = parts[1].split(" ") if len(parts) > 1 and parts[1] else []
        if op == "CANVAS":
            continue
        if op == "SCENE":
            # a scene marker opens a picture: the pixels belong to the scene that is ENDING
            if current:
                scenes.append((current, c.px.copy()))
            current = a[0]
            continue
        if op == "CLEAR":
            c.px[:] = False
        elif op == "COLOR":
            color = int(a[0])
        elif op == "FONT":
            f = int(a[0])
            size, bold = (7, False) if f == 1 else ((8, True) if f == 0 else (14, True))
        elif op == "BOX":
            x, y, w, h = map(int, a)
            c.set(x, y, x + w - 1, y + h - 1, color)
        elif op == "FRAME":
            x, y, w, h = map(int, a)
            c.set(x, y, x + w - 1, y, color)
            c.set(x, y + h - 1, x + w - 1, y + h - 1, color)
            c.set(x, y, x, y + h - 1, color)
            c.set(x + w - 1, y, x + w - 1, y + h - 1, color)
        elif op == "DOT":
            c.set(int(a[0]), int(a[1]), int(a[0]), int(a[1]), color)
        elif op == "LINE":
            c.line(*map(int, a), color)
        elif op == "DISC":
            x, y, r = map(int, a)
            yy, xx = np.ogrid[-r:r + 1, -r:r + 1]
            m = xx * xx + yy * yy <= r * r
            for j in range(m.shape[0]):
                for i in range(m.shape[1]):
                    if m[j, i]:
                        c.set(x - r + i, y - r + j, x - r + i, y - r + j, color)
        elif op == "STR":
            x, y, col, s = int(a[0]), int(a[1]), int(a[2]), " ".join(a[3:])
            c.text(x, y, s, col, size=size, bold=bold)
        elif op == "STRA":
            x, y, ha, va, col, s = int(a[0]), int(a[1]), int(a[2]), int(a[3]), int(a[4]), " ".join(a[5:])
            c.text(x, y, s, col, ha=ha, va=va, size=size, bold=bold)
        elif op == "PILL":
            idx, label = int(a[0]), " ".join(a[1:])
            x0 = {0: 0, 1: 44, 2: 88}[idx]
            c.set(x0, 51, 127 if idx == 2 else x0 + 43, 63, 0)
            c.text(x0 + 3, 57, label, 1, size=7)
    if current:
        scenes.append((current, c.px.copy()))

    out.mkdir(parents=True, exist_ok=True)
    paths = []
    for name, px in scenes:
        img = np.where(px, 0, 255).astype(np.uint8)
        im = Image.fromarray(img).resize((W * SCALE, H * SCALE), Image.NEAREST)
        p = out / f"{name}.png"
        im.save(p)
        paths.append(p)
    return paths


def main(argv):
    trace = Path(argv[1]).read_text()
    out = Path(argv[2]) if len(argv) > 2 else Path("sim/out")
    for p in render(trace, out):
        print(p)


if __name__ == "__main__":
    main(sys.argv)
