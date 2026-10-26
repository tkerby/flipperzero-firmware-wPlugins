#!/usr/bin/env python3
"""Overworld sprites of the ROAMING wild creatures (contract 55): the creature art (assets/art/elementis/<species>_c, 64 px pro, 8 facings) scaled down to a map actor,
by the recipe of tools/legend_ow.py (no PixelLab, no drawn art).

  python3 tools/roam_ow.py [build|check|sheet] [species ...]

The species are the ones named by the `@encounters` table of every map that has a `@roam` directive (maps/*.map; `table=` picks the table, default grass), plus the ones
given on the command line.
build (default) writes assets/art/npcs/<species>_roam/<facing>.png (south, south-east, east, north-east, north; the catalog's `mirror_west = true` folds the three west facings)
       and assets/catalog_roam.toml (one `roam_<species>` actor per species, GENERATED: do not edit; mapc and spritec read it next to maps/test_catalog.toml).
       Size: the tallest facing is scaled to TARGET rows by the species' `height` in species.toml (24 rows at 0.3 m, +5 per doubling, at most 34), every facing of a species by the SAME
       factor, alpha cut at 128 (a premultiplied-alpha Lanczos resample: no black bleeding into the edge), padded to one canvas standing on the bottom row. One frame: the engine's hop
       (`hop = true`) bobs it while it walks. The typecast is the species' own: `levels = "adaptive"`, `dark_cut` = its art_dark_cut (default 70, as tools/monc.py) and
       `adaptive_split` = its art_split on every stored facing: the light typecast for the dark creatures (muncrit 45, the 25 / split ones).
check exits 1 when a file is missing or older than its source, or assets/catalog_roam.toml is not what `build` would write (the build step of tools/build_assets.sh runs it).
sheet writes build/roam/ow_sheet.png: every species, every facing, converted with the real catalog entry (tools/spritec.py), on a light and a dark floor swatch.
"""
import math
import os
import re
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from PIL import Image  # noqa: E402
from legend_ow import bbox, premul_resize  # noqa: E402

FACINGS = ["south", "south-east", "east", "north-east", "north"]
MTIME_SLACK = 60  # seconds: a fresh clone / CI checkout writes source and derived files in arbitrary order within a few seconds
MIN_H, MAX_H, BASE_M = (
    24,
    34,
    0.3,
)  # rows of the tallest facing: 24 at 0.3 m (the smallest creatures), +5 per doubling of the height, at most 34 (the hero is ~32)
SRC = os.path.join(ROOT, "assets", "art", "elementis")
DST = os.path.join(ROOT, "assets", "art", "npcs")
CATALOG = os.path.join(ROOT, "assets", "catalog_roam.toml")
SPECIES_TOML = os.path.join(ROOT, "assets", "data", "species.toml")
DARK_CUT = 70  # tools/monc.py DARK_CUT: the species default


def species_data():
    with open(SPECIES_TOML, "rb") as f:
        return tomllib.load(f)


def target_h(sd):
    h = float(sd.get("height", BASE_M))
    return max(MIN_H, min(MAX_H, round(MIN_H + 5 * math.log2(max(h, 0.01) / BASE_M))))


def affinity_solar():
    """The Solar forms of the wild affinity pairs (assets/data/affinity.toml, contract 66): they never roam (the roamer would show one species and a Lunar player would fight its counterpart), so
    they get no actor and mapc writes ROAM_NONE for their slots."""
    with open(os.path.join(ROOT, "assets", "data", "affinity.toml"), "rb") as f:
        return {
            p["solar"] for p in tomllib.load(f).get("pair", []) if p.get("wild", True)
        }


def map_species():
    """Species named by the @encounters table of every map with a @roam directive (source order, unique), except the Solar forms of the affinity pairs."""
    out = []
    skip = affinity_solar()
    mdir = os.path.join(ROOT, "maps")
    for fn in sorted(os.listdir(mdir)):
        if not fn.endswith(".map"):
            continue
        lines = [
            ln.split("#", 1)[0].strip()
            for ln in open(os.path.join(mdir, fn), encoding="utf-8")
        ]
        tabs = []
        for ln in lines:
            t = ln.split()
            if t and t[0] == "@roam":
                opt = dict(
                    x.split("=", 1)
                    for x in t[1:]
                    if "=" in x and not x.startswith("area=")
                )
                tabs.append(opt.get("table", "grass"))
        for tab in tabs:
            cur = False
            for ln in lines:
                t = ln.split()
                if not t:
                    continue
                if t[0] == "@encounters":
                    cur = len(t) > 1 and t[1] == tab
                elif t[0] == "@end":
                    cur = False
                elif cur and len(t) == 4 and t[0] not in out and t[0] not in skip:
                    out.append(t[0])
    return out


# Contract 63: two committed pictures are not images (tarbog_c/north.png and wombamma_c/south-east.png are a PixelLab "too_busy" reply of 116 bytes). A facing whose file cannot be
# read is taken from its nearest neighbour (a mirrored west picture or an adjacent facing); a warning is printed. Replace the picture and rebuild to get the real one.
SUBST = {
    "south": [("south-east", 0), ("south-west", 1)],
    "south-east": [("south-west", 1), ("south", 0)],
    "east": [("south-east", 0), ("north-east", 0), ("west", 1)],
    "north-east": [("north-west", 1), ("north", 0)],
    "north": [("north-east", 0), ("north-west", 1)],
}


def load_facing(art, f):
    try:
        return Image.open(os.path.join(SRC, art, f + ".png")).convert("RGBA")
    except Exception:
        for alt, flip in SUBST[f]:
            try:
                im = Image.open(os.path.join(SRC, art, alt + ".png")).convert("RGBA")
            except Exception:
                continue
            print(
                f"roam_ow: warning: {art}/{f}.png is not an image: using {'the mirrored ' if flip else ''}{alt}",
                file=sys.stderr,
            )
            return im.transpose(Image.FLIP_LEFT_RIGHT) if flip else im
        raise


def build(species, sd):
    files = {}
    for f in FACINGS:
        im = load_facing(sd["art"], f)
        files[f] = im.crop(bbox(im))
    top = max(i.size[1] for i in files.values())
    k = target_h(sd) / top
    out = {
        f: premul_resize(i, max(1, round(i.size[0] * k)), max(1, round(i.size[1] * k)))
        for f, i in files.items()
    }
    cw = max(i.size[0] for i in out.values())
    ch = max(i.size[1] for i in out.values())
    d = os.path.join(DST, species + "_roam")
    os.makedirs(d, exist_ok=True)
    for f, i in out.items():
        c = Image.new("RGBA", (cw, ch), (0, 0, 0, 0))
        c.alpha_composite(
            i, ((cw - i.size[0]) // 2, ch - i.size[1])
        )  # centred, standing on the bottom row
        c.save(os.path.join(d, f + ".png"))
    return cw, ch


def catalog_text(names, data):
    L = [
        "# GENERATED by tools/roam_ow.py (contract 55): the overworld actors of the roaming wild creatures, one per species named by the table of a map with `@roam`. Do not edit.",
        '# Read by tools/mapc.py and tools/spritec.py next to maps/test_catalog.toml. `stream = "always"`: streamed from the card even below the 1 KB threshold (a roamer must not cost resident RAM).',
        "",
    ]
    for n in names:
        sd = data[n]
        cut = int(sd.get("art_dark_cut", DARK_CUT))
        e = [
            'name = "roam_%s"' % n,
            'kind = "actor"',
            'src = "assets/art/npcs/%s_roam/"' % n,
            "ndirs = 8",
            "nframes = 1",
            'levels = "adaptive"',
            "dark_cut = %d" % cut,
        ]
        sp = sd.get("art_split")
        if sp:
            e.append(
                "adaptive_split = { %s }"
                % ", ".join("%s = [%s, %s]" % (f, sp[0], sp[1]) for f in FACINGS)
            )
        e += ["mirror_west = true", "hop = true", 'stream = "always"']
        L.append("[[sprite]]")
        L += e
        L.append("")
    return "\n".join(L)


def check(names, data):
    bad = []
    for n in names:
        for f in FACINGS:
            s = os.path.join(SRC, data[n]["art"], f + ".png")
            d = os.path.join(DST, n + "_roam", f + ".png")
            if not os.path.exists(d):
                bad.append(d + " missing")
            elif os.path.getmtime(d) + MTIME_SLACK < os.path.getmtime(s):
                bad.append(d + " older than its source")
    want = catalog_text(names, data)
    have = open(CATALOG, encoding="utf-8").read() if os.path.exists(CATALOG) else ""
    if have != want:
        bad.append(
            "assets/catalog_roam.toml is not what `python3 tools/roam_ow.py build` writes"
        )
    return bad


def sheet(names, data):
    import spritec

    cat = spritec.load_catalog(
        os.path.join(ROOT, "assets", "catalog.toml"), extra=CATALOG
    )
    cell_w, cell_h = 48, 44
    img = Image.new(
        "RGB", (cell_w * len(FACINGS), cell_h * len(names) * 2), (255, 255, 255)
    )
    ink = [(255, 255, 255), (170, 170, 170), (85, 85, 85), (0, 0, 0)]
    row = 0
    for n in names:
        sp = cat.sprite("roam_" + n)
        for fl in ("light", "dark"):
            for c, f in enumerate(FACINGS):
                idx = spritec.ORDER8.index(f)
                lv, mk = sp.frames[idx][0]
                x0, y0 = c * cell_w, row * cell_h
                for y in range(cell_h):
                    for x in range(cell_w):
                        v = (
                            (255 if (x // 2 + y) % 9 else 225)
                            if fl == "light"
                            else (130 if (x * 7 + y * 13) % 11 else 85)
                        )
                        img.putpixel((x0 + x, y0 + y), (v, v, v))
                ox, oy = x0 + (cell_w - sp.w) // 2, y0 + cell_h - 4 - sp.h
                for y in range(sp.h):
                    for x in range(sp.w):
                        if mk[y][x]:
                            img.putpixel((ox + x, oy + y), ink[lv[y][x]])
            row += 1
    out = os.path.join(ROOT, "build", "roam")
    os.makedirs(out, exist_ok=True)
    p = os.path.join(out, "ow_sheet.png")
    img.resize((img.size[0] * 4, img.size[1] * 4), Image.NEAREST).save(p)
    print("wrote", p)


def main():
    args = sys.argv[1:]
    cmd = args[0] if args and args[0] in ("build", "check", "sheet") else "build"
    data = species_data()
    extra = [a for a in args if a in data]
    names = []
    for n in map_species() + extra:
        if n not in names:
            names.append(n)
    names.sort()
    for n in names:
        if n not in data or not data[n].get("art"):
            print("roam_ow: species %r has no art in species.toml" % n)
            sys.exit(1)
    if cmd == "build":
        for n in names:
            print(n, "%dx%d" % build(n, data[n]))
        with open(CATALOG, "w", encoding="utf-8") as f:
            f.write(catalog_text(names, data))
        print("wrote", os.path.relpath(CATALOG, ROOT), "(%d species)" % len(names))
    elif cmd == "check":
        bad = check(names, data)
        for m in bad:
            print("roam_ow:", m)
        sys.exit(1 if bad else 0)
    else:
        sheet(names, data)


if __name__ == "__main__":
    main()
