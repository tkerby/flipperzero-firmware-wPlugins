#!/usr/bin/env python3
"""Title-screen mockups (Elementis Zeta): animated full-screen scenes in 4 grays at the final size.

Pieces: the logo (tools/logo_px.py), the spinning Cristal (tools/crystal3d.py, real 3D render), the hero and a
style parade of Elementis (medium front pictures, flipped to face the hero).

  python3 -I tools/title_mock.py
writes build/title_gate/<scene>.gif (LCD look, for the Mac), build/title_gate/sheet.png (first frames) and
build/title_gate/anim.bin for the device viewer (anim_gate/: copy to /ext/apps_data/anim_gate/anim.bin).

anim.bin: "AGT1", u8 scene count, then per scene: name (24 bytes, NUL padded), u16 frame count, u16 ms per frame,
frames of 2048 bytes (2 bpp, 4 px per byte, pixel x at bits (x & 3) * 2, levels 0 white .. 3 black).
"""
import os
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image  # noqa: E402
import battle_mock as bm  # noqa: E402
import crystal3d  # noqa: E402
import logo_px  # noqa: E402
import lvlib  # noqa: E402

SW, SH = 128, 64
OUT = os.path.join(ROOT, "build", "title_gate")
TICK = 60  # ms per animation frame
PARADE = ["pyrewolf", "tidalgard", "grovestag", "voltgecko"]


class Spr:
    def __init__(self, lv, mk):
        self.lv, self.mk = lv, mk
        self.h, self.w = len(lv), len(lv[0])


def flip(s):
    return Spr([r[::-1] for r in s.lv], [r[::-1] for r in s.mk])


def blit(p, s, x0, y0):
    for y in range(s.h):
        for x in range(s.w):
            if s.mk[y][x]:
                X, Y = x0 + x, y0 + y
                if 0 <= X < SW and 0 <= Y < SH:
                    p[Y][X] = s.lv[y][x]


def text(p, s, x, y, v=3):
    for ch in s:
        for gx, gy in bm.FONT.g.get(ch, []):
            X, Y = x + gx, y + gy
            if 0 <= X < SW and 0 <= Y < SH:
                p[Y][X] = v
        x += 6


def hero():
    s = bm.load(os.path.join(ROOT, "models_dl/pixellab/heroV3txt/south-west.png"))
    return Spr(s.lv, s.mk)


def mon(name, max_h):
    s = bm.creature(name, "south-west", max_h=max_h)
    return flip(Spr(s.lv, s.mk))


def ground(p, y, x0=0, x1=SW - 1, v=2):
    for x in range(x0, x1 + 1):
        if 0 <= y < SH:
            p[y][x] = v


def spark(p, cx, cy, size):
    """4-point twinkle in black (reads on white and on the crystal's light faces)."""
    for d in range(-size, size + 1):
        for X, Y in ((cx + d, cy), (cx, cy + d)):
            if 0 <= X < SW and 0 <= Y < SH:
                p[Y][X] = 3 if abs(d) < size else 2
    if size >= 2:
        p[cy][cx] = 0


class Parade:
    """parade: each Elementi slides in from the left edge, holds, slides out; the next one follows."""

    def __init__(self, names, max_h, x_rest, base_y, slide=6, hold=40):
        self.sprs = [mon(n, max_h) for n in names]
        self.x_rest, self.base_y, self.slide, self.hold = x_rest, base_y, slide, hold
        self.per = 2 * slide + hold

    def frames(self):
        return self.per * len(self.sprs)

    def draw(self, p, f):
        k, t = divmod(f % self.frames(), self.per)
        s = self.sprs[k]
        if t < self.slide:
            x = -s.w + (self.x_rest + s.w) * (t + 1) // self.slide
        elif t < self.slide + self.hold:
            x = self.x_rest
        else:
            x = (
                self.x_rest
                - (self.x_rest + s.w) * (t - self.slide - self.hold + 1) // self.slide
            )
        blit(p, s, x, self.base_y - s.h)


class Crystal:
    def __init__(self, height, n=12, step=2):
        self.fr = [Spr(*f) for f in crystal3d.frames(height, n)]
        self.step = step  # animation frames per crystal frame

    def draw(self, p, f, cx, cy, bob=True):
        s = self.fr[(f // self.step) % len(self.fr)]
        dy = 0
        if bob:
            dy = (0, 0, -1, -1, -1, 0, 0, 1, 1, 1)[(f // 4) % 10]
        blit(p, s, cx - s.w // 2, cy - s.h // 2 + dy)
        return s, dy


def blank(v=0):
    return [[v] * SW for _ in range(SH)]


def scene_a(nf):
    """Clasico: logo ELEMENTIS + ZETA centred on top, Elementi (left) and hero (right), the Cristal floating between."""
    logo = Spr(*logo_px.word("ELEMENTIS", "arialblack", 11, track=1))
    zero = Spr(*logo_px.word("ZETA", "arialblack", 7, track=1, depth=1))
    cr = Crystal(24)
    par = Parade(PARADE, 34, 4, 62)
    he = hero()
    out = []
    for f in range(nf):
        p = blank()
        ground(p, 62, 0, SW - 1, 1)
        par.draw(p, f)
        blit(p, he, SW - he.w - 3, 63 - he.h)
        s, dy = cr.draw(p, f, 70, 46)
        if (f % 48) < 6:
            spark(
                p, 70 - s.w // 2 + 3, 46 - s.h // 2 + 5 + dy, (1, 2, 3, 3, 2, 1)[f % 48]
            )
        blit(p, logo, (SW - logo.w) // 2, 1)
        blit(p, zero, (SW - zero.w) // 2 + 2, logo.h)
        out.append(p)
    return out


def scene_b(nf):
    """Emblema: ELEMENTIS on top, a big Cristal under it with ZETA across its waist; Elementi left, hero right."""
    logo = Spr(*logo_px.word("ELEMENTIS", "arialblack", 11, track=1))
    zero = Spr(*logo_px.word("ZETA", "arialblack", 8, track=1))
    cr = Crystal(42)
    par = Parade(PARADE, 30, 0, 63)
    he = hero()
    out = []
    for f in range(nf):
        p = blank()
        s, dy = cr.draw(p, f, 64, 42)
        par.draw(p, f)
        blit(p, he, SW - he.w, 64 - he.h)
        blit(p, logo, (SW - logo.w) // 2, 1)
        blit(p, zero, (SW - zero.w) // 2 + 1, 38)
        if (f % 48) < 6:
            spark(
                p,
                64 - s.w // 2 + 5,
                42 - s.h // 2 + 12 + dy,
                (1, 2, 3, 3, 2, 1)[f % 48],
            )
        out.append(p)
    return out


def scene_c(nf):
    """Cristal-O: ELEMENTIS on top, ZER + the spinning Cristal as the O; Elementi left, hero right."""
    logo = Spr(*logo_px.word("ELEMENTIS", "arialblack", 11, track=1))
    zer = Spr(*logo_px.word("ZER", "arialblack", 11, track=1))
    cr = Crystal(20)
    par = Parade(PARADE, 30, 0, 63)
    he = hero()
    out = []
    for f in range(nf):
        p = blank()
        par.draw(p, f)
        blit(p, he, SW - he.w, 64 - he.h)
        blit(p, logo, (SW - logo.w) // 2, 1)
        zx = (SW - zer.w - 16) // 2
        blit(p, zer, zx, logo.h + 1)
        s, dy = cr.draw(p, f, zx + zer.w + 6, logo.h + 1 + 8, bob=False)
        if (f % 48) < 6:
            spark(
                p,
                zx + zer.w + 6 - s.w // 2 + 3,
                logo.h + 1 + 8 - s.h // 2 + 4,
                (1, 2, 3, 3, 2, 1)[f % 48],
            )
        out.append(p)
    return out


def pack(p):
    b = bytearray(SW * SH // 4)
    for y in range(SH):
        for x in range(SW):
            b[y * (SW // 4) + (x >> 2)] |= (p[y][x] & 3) << ((x & 3) * 2)
    return bytes(b)


def gif(frames, path, scale=4):
    ims = [
        lvlib.lcd_image(f, None, scale=scale, grid=False).convert(
            "P", palette=Image.ADAPTIVE, colors=8
        )
        for f in frames
    ]
    ims[0].save(path, save_all=True, append_images=ims[1:], duration=TICK, loop=0)


def main():
    os.makedirs(OUT, exist_ok=True)
    n = Parade(PARADE, 34, 6, 62).frames()  # one full parade
    scenes = [
        ("A clasico", scene_a(n)),
        ("B emblema", scene_b(n)),
        ("C cristal-O", scene_c(n)),
    ]
    with open(os.path.join(OUT, "anim.bin"), "wb") as f:
        f.write(b"AGT1" + bytes([len(scenes)]))
        for name, frs in scenes:
            f.write(name.encode()[:23].ljust(24, b"\0"))
            f.write(struct.pack("<HH", len(frs), TICK))
            for p in frs:
                f.write(pack(p))
    sheet = Image.new("RGB", (SW * 4 * 3 + 40, SH * 4 * 2 + 30), (40, 40, 40))
    for k, (name, frs) in enumerate(scenes):
        gif(frs, os.path.join(OUT, name.split()[0] + ".gif"))
        for r, fi in enumerate((3, 10)):
            sheet.paste(
                lvlib.lcd_image(frs[fi], None, scale=4, grid=False),
                (10 + k * (SW * 4 + 10), 10 + r * (SH * 4 + 10)),
            )
    sheet.save(os.path.join(OUT, "sheet.png"))
    print(
        "frames per scene:",
        n,
        " anim.bin:",
        os.path.getsize(os.path.join(OUT, "anim.bin")),
        "bytes",
    )


if __name__ == "__main__":
    main()
