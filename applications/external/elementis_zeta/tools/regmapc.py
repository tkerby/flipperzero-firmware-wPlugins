#!/usr/bin/env python3
"""Region map (the Atlas, contract 74): assets/data/regmap.toml (+ the map list and its warps) -> nucleo/files/regmap.bin.

  python3 tools/regmapc.py            build regmap.bin and a preview PNG (build/regmap/regmap_preview.png, x4)
  python3 tools/regmapc.py check      verify regmap.bin is up to date and the data is complete (exit 1 otherwise)

regmap.bin (little-endian), read on demand by nucleo/scene_f2.c (read_at; nothing is kept in RAM but the screen's own level buffer):
  0   "NRMP"            4  u8 version = 1     5  u8 n_area (len(AREA_KEYS))     6  u16 n_maps (highest map id + 1)    8  u32 0     12  u32 0
  16  the picture: 128 x 64, 2 bits a pixel, 4 pixels a byte (pixel x at bits (x & 3) * 2), 32 bytes a row = the level buffer's packing (2,048 B); the bottom 13 rows are the name bar
  2064  area table: n_area x (x, y): the centre of the mark of zone banner `area` (1-based; 255, 255 = not placed)
  2064 + 2 * n_area  map table: n_maps bytes, the area (1-based) the map belongs to (0 = none): its own @area when that area is placed, else the nearest placed area through the warps
Colours: 0 white (land), 1 light grey (sea), 2 dark grey (details), 3 black (outline, roads, marks).
"""
import os
import struct
import sys
import tomllib
from collections import deque
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import mapc  # noqa: E402

W, H = 128, 64
BAR = 13                       # the name bar's rows (the last 13 rows)
MAP_H = H - BAR                # rows 0 .. 50: the map; row 51 is the bar's rule
SRC = ROOT / "assets" / "data" / "regmap.toml"
OUT = ROOT / "nucleo" / "files" / "regmap.bin"
PREVIEW = ROOT / "build" / "regmap" / "regmap_preview.png"
TAB = 16 + W * H // 4


def load():
    with open(SRC, "rb") as f:
        return tomllib.load(f)


def resolve_maps(world, places, alias):
    """{map id: area index (1-based)}: the map's own area when placed, else the nearest placed one through the warps (both directions), else the alias."""
    maps = {m.name: m for m in world.maps if not m.name.startswith("test_")}
    adj = {n: set() for n in maps}
    for m in maps.values():
        for wp in m.warps:
            if wp.target_map in maps:
                adj[m.name].add(wp.target_map)
                adj[wp.target_map].add(m.name)
        for t in mapc.script_warp_targets(m):
            if t in maps:
                adj[m.name].add(t)
                adj[t].add(m.name)
    out, lost = {}, []
    for name, m in maps.items():
        a = m.area
        if a and AREA_KEYS[a - 1] in places:
            out[m.id] = a
            continue
        seen, q, found = {name}, deque([name]), None
        while q and found is None:
            cur = q.popleft()
            for nb in sorted(adj[cur]):
                if nb in seen:
                    continue
                seen.add(nb)
                na = maps[nb].area
                if na and AREA_KEYS[na - 1] in places:
                    found = na
                    break
                q.append(nb)
        if found is None:
            lost.append(name)
        else:
            out[m.id] = found
    for name in list(lost):    # [alias]: a map the warps cannot reach takes the area of the named map
        t = maps.get(alias.get(name, ""))
        if t is not None and t.id in out:
            out[maps[name].id] = out[t.id]
            lost.remove(name)
    return out, lost


AREA_KEYS = mapc.AREA_KEYS


# ---------------------------------------------------------------- drawing
class Canvas:
    def __init__(self):
        self.p = [[1] * W for _ in range(H)]

    def put(self, x, y, v):
        if 0 <= x < W and 0 <= y < H:
            self.p[y][x] = v

    def get(self, x, y):
        return self.p[y][x] if 0 <= x < W and 0 <= y < H else 3

    def line(self, x0, y0, x1, y1, v, dash=0):
        dx, dy = abs(x1 - x0), abs(y1 - y0)
        sx, sy = (1 if x0 < x1 else -1), (1 if y0 < y1 else -1)
        err, n = dx - dy, 0
        while True:
            if not dash or (n // dash) % 2 == 0:
                self.put(x0, y0, v)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 > -dy:
                err -= dy
                x0 += sx
            if e2 < dx:
                err += dx
                y0 += sy
            n += 1

    def rect(self, x, y, w, h, v):
        for j in range(h):
            for i in range(w):
                self.put(x + i, y + j, v)

    def frame(self, x, y, w, h, v):
        for i in range(w):
            self.put(x + i, y, v)
            self.put(x + i, y + h - 1, v)
        for j in range(h):
            self.put(x, y + j, v)
            self.put(x + w - 1, y + j, v)


def _h(x, y):
    n = (x * 374761393 + y * 668265263) & 0xFFFFFFFF
    n = ((n ^ (n >> 13)) * 1274126177) & 0xFFFFFFFF
    return ((n ^ (n >> 16)) & 255) / 255.0


def noise(x, y, cell=6):
    """Smooth value noise in 0..1 (a fixed hash: the picture is deterministic)."""
    gx, gy, fx, fy = x // cell, y // cell, (x % cell) / cell, (y % cell) / cell
    fx, fy = fx * fx * (3 - 2 * fx), fy * fy * (3 - 2 * fy)
    a, b = _h(gx, gy) * (1 - fx) + _h(gx + 1, gy) * fx, _h(gx, gy + 1) * (1 - fx) + _h(gx + 1, gy + 1) * fx
    return a * (1 - fy) + b * fy


def seg_dist(px, py, x0, y0, x1, y1):
    dx, dy = x1 - x0, y1 - y0
    t = 0 if dx == dy == 0 else max(0.0, min(1.0, ((px - x0) * dx + (py - y0) * dy) / (dx * dx + dy * dy)))
    return ((px - x0 - t * dx) ** 2 + (py - y0 - t * dy) ** 2) ** 0.5


def morph(mask, grow):
    out = [row[:] for row in mask]
    for y in range(MAP_H):
        for x in range(W):
            nb = [mask[yy][xx] for yy in range(max(0, y - 1), min(MAP_H, y + 2)) for xx in range(max(0, x - 1), min(W, x + 2))]
            out[y][x] = 1 if (any(nb) if grow else all(nb)) else 0
    return out


R_KIND = {"town": 6, "city": 7, "route": 5, "cave": 5, "wood": 6, "plateau": 7, "ruin": 4, "power": 5, "reserva": 8, "island": 4, "sea": 0}


def render(data):
    c = Canvas()
    places = data["place"]
    land = [[0] * W for _ in range(MAP_H)]
    discs = [(p["x"], p["y"], R_KIND[p["kind"]]) for p in places.values() if R_KIND[p["kind"]]]
    segs = [(x0, y0, x1, y1) for rd in data["road"] if rd["style"] == "road" for (x0, y0), (x1, y1) in zip(rd["pts"], rd["pts"][1:])]
    for y in range(MAP_H):
        for x in range(W):
            wob = (noise(x + 17, y + 5) - 0.5) * 5.0 + (noise(x + 91, y + 33, 3) - 0.5) * 2.0   # +- 3 px of coast wobble
            d = min([((x - cx) ** 2 + (y - cy) ** 2) ** 0.5 - r for cx, cy, r in discs] + [seg_dist(x, y, *sg) - 5 for sg in segs])
            land[y][x] = 1 if d + wob < 1.0 else 0
    land = morph(morph(land, True), False)      # close the small gaps
    land = morph(morph(land, False), True)      # and drop the single-pixel spurs
    sea, q = [[0] * W for _ in range(MAP_H)], deque()   # no lakes inland: whatever the sea does not reach from the picture's edge is land
    for y in range(MAP_H):
        for x in range(W):
            if (x in (0, W - 1) or y in (0, MAP_H - 1)) and not land[y][x] and not sea[y][x]:
                sea[y][x] = 1
                q.append((x, y))
    while q:
        x, y = q.popleft()
        for xx, yy in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
            if 0 <= xx < W and 0 <= yy < MAP_H and not land[yy][xx] and not sea[yy][xx]:
                sea[yy][xx] = 1
                q.append((xx, yy))
    land = [[0 if sea[y][x] else 1 for x in range(W)] for y in range(MAP_H)]
    for y in range(MAP_H):
        for x in range(W):
            c.p[y][x] = 0 if land[y][x] else 1
    # the coast
    for y in range(MAP_H):
        for x in range(W):
            if land[y][x]:
                for xx, yy in ((x - 1, y), (x + 1, y), (x, y - 1), (x, y + 1)):
                    if 0 <= xx < W and 0 <= yy < MAP_H and not land[yy][xx]:
                        c.p[y][x] = 3
                        break
    # a few waves on the open sea (dark dashes, a fixed pattern)
    for (x, y) in ((100, 4), (112, 10), (120, 20), (108, 36), (118, 46), (72, 49), (96, 49), (6, 36), (24, 36), (3, 49)):
        if all(not land[y + dy][x + dx] if 0 <= y + dy < MAP_H and 0 <= x + dx < W else False for dx in range(-3, 6) for dy in (-2, 0, 2)):
            c.line(x, y, x + 2, y, 2)
            c.line(x + 3, y + 1, x + 5, y + 1, 2)
    # roads
    for rd in data["road"]:
        pts = rd["pts"]
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            if rd["style"] == "sea":
                c.line(x0, y0, x1, y1, 3, dash=2)
            else:
                c.line(x0, y0, x1, y1, 3)
    # marks
    for key, p in places.items():
        x, y, k = p["x"], p["y"], p["kind"]
        if k in ("town", "city"):
            s = 5 if k == "town" else 7
            c.rect(x - s // 2, y - s // 2, s, s, 0)
            c.frame(x - s // 2, y - s // 2, s, s, 3)
            if k == "city":
                c.rect(x - 1, y - 1, 3, 3, 3)
        elif k == "cave":
            for j, wd in enumerate((3, 5, 7)):
                c.rect(x - wd // 2, y - 1 + j, wd, 1, 3)
            c.rect(x, y + 1, 1, 1, 0)
        elif k == "wood":
            for dx, dy in ((-4, 1), (0, -2), (4, 1)):
                c.put(x + dx, y + dy - 1, 3)
                c.rect(x + dx - 1, y + dy, 3, 1, 3)
                c.put(x + dx, y + dy + 1, 3)
        elif k == "plateau":
            for j in range(5):
                c.rect(x - j - 1, y + 1 + j - 2, 2 * j + 3, 1, 3)
            c.line(x, y - 5, x, y - 1, 3)
            c.rect(x + 1, y - 5, 3, 2, 3)
        elif k == "ruin":
            c.rect(x - 2, y, 2, 4, 3)
            c.rect(x + 1, y - 2, 2, 6, 3)
            c.rect(x - 3, y + 4, 7, 1, 3)
        elif k == "power":
            for (a, b, d, e) in ((x + 1, y - 4, x - 1, y), (x - 1, y, x + 1, y), (x + 1, y, x - 1, y + 4)):
                c.line(a, b, d, e, 3)
        elif k == "reserva":
            c.rect(x - 5, y - 3, 11, 7, 0)
            for j in range(7):
                for i in range(11):
                    if (i + j) % 3 == 0:
                        c.put(x - 5 + i, y - 3 + j, 2)
            c.frame(x - 5, y - 3, 11, 7, 3)
        elif k == "island":
            c.rect(x - 1, y - 1, 3, 3, 3)
    for d in data.get("deco", []):
        x, y, k = d["x"], d["y"], d["kind"]
        if k in ("mount", "range"):
            for ox, oy in (((0, 0),) if k == "mount" else ((-5, 1), (0, -1), (5, 1))):
                for (dx, dy) in ((0, 0), (-1, 1), (1, 1), (-2, 2), (2, 2)):
                    c.put(x + ox + dx, y + oy + dy, 3)
                c.put(x + ox, y + oy + 1, 0)
        elif k == "lake":
            rx, ry = d["rx"], d["ry"]
            for j in range(-ry - 1, ry + 2):
                for i in range(-rx - 1, rx + 2):
                    e = (i / (rx + 0.5)) ** 2 + (j / (ry + 0.5)) ** 2
                    if e <= 1.0:
                        c.put(x + i, y + j, 1)
                    elif e <= 1.6:
                        c.put(x + i, y + j, 2) if c.get(x + i, y + j) == 0 else None
        elif k == "tree":
            c.put(x, y - 1, 3)
            c.rect(x - 1, y, 3, 1, 3)
            c.put(x, y + 1, 3)
        elif k == "islet":
            for j in range(-2, 3):
                for i in range(-3, 4):
                    if (i / 3.5) ** 2 + (j / 2.5) ** 2 <= 1.0:
                        c.put(x + i, y + j, 0)
                    elif (i / 3.5) ** 2 + (j / 2.5) ** 2 <= 1.9:
                        c.put(x + i, y + j, 3) if c.get(x + i, y + j) == 1 else None
    # the frame and the name bar's rule
    c.frame(0, 0, W, H, 3)
    c.rect(1, MAP_H, W - 2, 1, 3)
    c.rect(1, MAP_H + 1, W - 2, BAR - 2, 0)
    return c


def pack(c):
    b = bytearray()
    for y in range(H):
        for x in range(0, W, 4):
            b.append(sum(c.p[y][x + i] << (2 * i) for i in range(4)))
    return bytes(b)


def build():
    data = load()
    world = mapc.load_world(ROOT)
    if world.report.errors:
        raise SystemExit("regmapc: the map sources have errors (run tools/mapc.py check)")
    places = data["place"]
    bad = [k for k in places if k not in AREA_KEYS]
    if bad:
        raise SystemExit("regmapc: unknown area keys in regmap.toml: %s" % bad)
    for k, p in places.items():
        if p["kind"] not in R_KIND or not (3 <= p["x"] < W - 3 and 3 <= p["y"] < MAP_H - 1):
            raise SystemExit("regmapc: place %s: bad kind or position %r" % (k, p))
    maps, lost = resolve_maps(world, places, data.get("alias", {}))
    if lost:
        raise SystemExit("regmapc: maps with no place on the region map (add them to [alias] or place their area): %s" % lost)
    # every outdoor area must be placed (a zone banner with no mark would leave the cursor nowhere)
    outdoor = sorted({AREA_KEYS[m.area - 1] for m in world.maps if m.area and not m.interior and not m.name.startswith("test_")})
    miss = [k for k in outdoor if k not in places]
    if miss:
        raise SystemExit("regmapc: outdoor areas without a place: %s" % miss)
    c = render(data)
    n_maps = max(maps) + 1
    blob = bytearray(b"NRMP" + struct.pack("<BBHII", 1, len(AREA_KEYS), n_maps, 0, 0))
    blob += pack(c)
    for k in AREA_KEYS:
        p = places.get(k)
        blob += bytes((p["x"], p["y"]) if p else (255, 255))
    blob += bytes(maps.get(i, 0) for i in range(n_maps))
    assert len(blob) == TAB + 2 * len(AREA_KEYS) + n_maps
    return bytes(blob), c


def preview(c):
    try:
        from PIL import Image
    except ImportError:
        return
    img = Image.new("L", (W, H))
    img.putdata([(255, 170, 85, 0)[c.p[y][x]] for y in range(H) for x in range(W)])
    PREVIEW.parent.mkdir(parents=True, exist_ok=True)
    img.resize((W * 5, H * 5), Image.NEAREST).save(PREVIEW)


def main():
    blob, c = build()
    if len(sys.argv) > 1 and sys.argv[1] == "check":
        if not OUT.is_file() or OUT.read_bytes() != blob:
            print("regmapc: regmap.bin is out of date (run tools/regmapc.py)")
            raise SystemExit(1)
        print("regmapc: regmap.bin OK (%d bytes)" % len(blob))
        return
    OUT.write_bytes(blob)
    preview(c)
    print("regmap.bin: %d bytes, %d areas, %d maps" % (len(blob), len(AREA_KEYS), blob[6] | blob[7] << 8))


if __name__ == "__main__":
    main()
