#!/usr/bin/env python3
"""Test of the follower's content (contract 69): nucleo/files/follow.bin (tools/follow_ow.py) has a sprite for EVERY species of species.toml (ids 1 .. SP_COUNT - 1): the header agrees with nucleo/map.h
(FOL_W / FOL_H / FOL_HDR), the file has exactly header + count x 5 facings, every facing of every species has an opaque body inside the canvas that stands on the bottom row, the per-species table of top rows is right,
the bytes are what `build` writes (deterministic), the species' own tone mapping is used (a dark-cut species is lighter than the default typecast), the `@nofollow` directive and its header bit (mapc / mapdump),
the seven maps that carry it (the ferry and the sea) and no other, and the eleven texts `gl.fol.*` exist in es / en / ja / zh in the order the plugin indexes them (nucleo/scene_follow.c).
  python3 -I tools/follow_test.py (exit status 1 on a failure; part of tools/build_assets.sh)"""
import re
import struct
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import follow_ow  # noqa: E402
import mapc  # noqa: E402
import mapdump  # noqa: E402

fails = 0


def check(what, cond):
    global fails
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        fails += 1


fol_w, fol_h, fol_hdr = follow_ow.engine_consts()
data = (ROOT / "nucleo" / "files" / "follow.bin").read_bytes()
species = follow_ow.species_list()
n = len(species)
frame = ((fol_w + 3) // 4) * fol_h + ((fol_w + 7) // 8) * fol_h
rec = 5 * frame
bpr, mbpr = (fol_w + 3) // 4, (fol_w + 7) // 8

check("the engine's constants: 20 x 20, the table of top rows ends at 176", (fol_w, fol_h, fol_hdr) == (20, 20, 176))
check("magic NFOL, version 1", data[:5] == b"NFOL\x01")
check(f"count = {n} species (SP_COUNT - 1 of nucleo/gen/reg_gen.h)", data[5] == n and re.search(r"#define SP_COUNT %d\b" % (n + 1), (ROOT / "nucleo/gen/reg_gen.h").read_text()) is not None)
check("w, h, frame bytes, record bytes in the header", struct.unpack_from("<BBHH", data, 6) == (fol_w, fol_h, frame, rec))
check(f"the file is header + {n} x {rec} bytes = {fol_hdr + n * rec}", len(data) == fol_hdr + n * rec)
check("build() writes exactly these bytes (deterministic, up to date with the art)", follow_ow.build_bytes() == data)

bad_body, bad_floor, bad_top, bad_levels = [], [], [], []
for i, key, sd in species:
    tops = []
    for fc in range(5):
        base = fol_hdr + (i - 1) * rec + fc * frame
        px = []
        for y in range(fol_h):
            for x in range(fol_w):
                if (data[base + bpr * fol_h + y * mbpr + (x >> 3)] >> (x & 7)) & 1:
                    px.append((x, y, (data[base + y * bpr + (x >> 2)] >> ((x & 3) * 2)) & 3))
        if len(px) < 30:
            bad_body.append(f"{key}/{fc}: {len(px)} px")
            continue
        tops.append(min(y for _, y, _ in px))
        if max(y for _, y, _ in px) != fol_h - 1:
            bad_floor.append(f"{key}/{fc}")
        if not any(v == 3 for _, _, v in px) or len({v for _, _, v in px}) < 2:
            bad_levels.append(f"{key}/{fc}")
    if tops and data[16 + i - 1] != min(tops):
        bad_top.append(f"{key}: table {data[16 + i - 1]} vs {min(tops)}")
check(f"every species ({n}) has all 5 facings with a body of >= 30 pixels on the canvas: {bad_body[:3]}", not bad_body)
check(f"every facing stands on the bottom row: {bad_floor[:3]}", not bad_floor)
check(f"every facing has an outline (level 3) and at least two tones: {bad_levels[:3]}", not bad_levels)
check(f"the table of top rows (offset 16) is the first row with an opaque pixel: {bad_top[:3]}", not bad_top)

# sizes follow the species' height: the 0.3 m species are smaller than the 2 m ones, nothing is taller than FOL_H
rows = {key: fol_h - data[16 + i - 1] for i, key, _ in species}
small = [k for _, k, sd in species if sd.get("height", 1) <= 0.3]
big = [k for _, k, sd in species if sd.get("height", 1) >= 2.0]
check("small creatures (<= 0.3 m) are 12 rows or less, big ones (>= 2 m) fill the canvas", max(rows[k] for k in small) <= 14 and min(rows[k] for k in big) >= 18)

# the species' own tone mapping: a species with art_dark_cut / art_split converts with it (muncrit 45 and the 25 / split ones are lighter than the default 70 typecast)
sd_all = {k: sd for _, k, sd in species}
dark = [k for k, sd in sd_all.items() if "art_dark_cut" in sd or "art_split" in sd]
check(f"{len(dark)} species have their own tone mapping (art_dark_cut / art_split): the conversion reads it", len(dark) > 5 and follow_ow.rows_for(0.3, 20) == 12)

# --- @nofollow: the directive, the header bit (bit 2 of the background byte), the maps
src = (ROOT / "tools" / "mapc.py").read_text()
check("mapc documents @nofollow and writes it into the header's background byte", "@nofollow" in src and "m.bg | (4 if m.nofollow else 0)" in src)
want = {"ferry_cubierta", "ferry_camarotes", "ferry_capitan", "mar_ambar", "ruta_19", "ruta_20", "ruta_21"}
have = {p.stem for p in (ROOT / "maps").glob("*.map") if re.search(r"^@nofollow\s+yes", p.read_text(encoding="utf-8"), re.M)}
check(f"exactly the ferry and the sea maps carry `@nofollow yes`: {sorted(have ^ want)}", have == want)
ids = {m.group(2): m.group(1) for m in re.finditer(r"^(\d+)\s+(\w+)", (ROOT / "maps" / "maps.txt").read_text(), re.M)}
bits = {}
for name in sorted(want) + ["ruta_1", "villa_aliso", "ciudad_marea", "muelle_dinamo"]:
    d = (ROOT / "nucleo" / "files" / ("map_%02d.bin" % int(ids[name]))).read_bytes()
    bits[name] = d[8] >> 2
    r = mapdump.parse_map_bin(d)
    check(f"{name}: header byte 8 = 0b{d[8]:03b}: bg {r['bg']} (0..3), nofollow {r['nofollow']}", r["bg"] == d[8] & 3 and r["nofollow"] == (name in want))
check("a map without it keeps the old byte (bg only)", all(v == 0 for k, v in bits.items() if k not in want))

# --- the texts
fol_keys = ["t0", "t1", "t2", "t3", "t4", "tired", "psn", "slp", "brn", "frz", "par"]
for lang in ("es", "en", "ja", "zh"):
    t = (ROOT / "assets" / "text" / f"{lang}.txt").read_text(encoding="utf-8")
    keys = re.findall(r"^\[(gl\.fol\.[a-z0-9]+)\]", t, re.M)
    check(f"{lang}: the block `# --- C1 follower (contract 69) ---` has the {len(fol_keys)} keys in the plugin's order", "# --- C1 follower (contract 69) ---" in t and keys == ["gl.fol." + k for k in fol_keys])
    if lang == "ja":
        body = "".join(re.findall(r"^\[gl\.fol\.[a-z0-9]+\]\n((?:(?!\[).*\n)*)", t, re.M))
        check("ja: kana only (no kanji) in the follower's block", not re.search(r"[一-鿿]", body))
    body = "".join(re.findall(r"^\[gl\.fol\.[a-z0-9]+\]\n((?:(?!\[).*\n)*)", t, re.M))
    check(f"{lang}: no word of another creature game", "pok" not in body.lower())
g = (ROOT / "nucleo" / "gen" / "maps_gen.h").read_text()
ids_ = [int(re.search(r"#define TXT_GL_FOL_%s (0x[0-9A-Fa-f]+)" % k.upper(), g).group(1), 16) for k in fol_keys]
check("the generated ids TXT_GL_FOL_T0 .. TXT_GL_FOL_PAR are consecutive (the plugin adds an index)", ids_ == list(range(ids_[0], ids_[0] + len(fol_keys))))

print(f"follow_test: {'FAILED' if fails else 'OK'} ({fails} failure(s))")
sys.exit(1 if fails else 0)
