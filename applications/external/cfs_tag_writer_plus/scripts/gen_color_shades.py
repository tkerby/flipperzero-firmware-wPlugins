"""Generate src/data/cfs_shades.c: named color shades grouped into families.

Source: meodai/color-names (MIT). Only plain, descriptive names are kept
(e.g. "Brick Red", "Avocado Green"): every word must be a known color word or
modifier, and the name must contain a word typical for its family.
Each family keeps a spread-out sample (farthest-point in CIELAB), sorted
light -> dark, so the Flipper picker can browse by hue family, then shade.

usage: python3 scripts/gen_color_shades.py colornames.csv out.c
"""
import colorsys
import csv
import io
import math
import sys
import zipfile

PER_FAMILY = 24
MAX_NAME = 18

FAMILIES = [
    "Whites & Grays",
    "Reds",
    "Oranges",
    "Yellows",
    "Greens",
    "Teals",
    "Blues",
    "Purples",
    "Pinks",
    "Browns & Beiges",
]


def family_of(r, g, b):
    h, l, s = colorsys.rgb_to_hls(r / 255, g / 255, b / 255)
    h *= 360
    if s < 0.10 or l < 0.07 or l > 0.96:
        return 0
    # Browns / beiges: warm hues that are dark or desaturated-light.
    if 10 <= h < 55 and (l < 0.42 or (l > 0.65 and s < 0.55)):
        return 9
    if h >= 345 or h < 12:
        return 8 if l > 0.72 else 1
    if h < 40:
        return 2
    if h < 68:
        return 3
    if h < 160:
        return 4
    if h < 195:
        return 5
    if h < 255:
        return 6
    if h < 295:
        return 7
    return 8


MODIFIERS = set("""light dark pale deep bright pastel neon dusty soft warm cool medium
vivid muted royal baby hot electric burnt off true pure classic metallic matte""".split())

FAMILY_WORDS = [
    set("""white gray grey black silver charcoal graphite smoke ash snow ivory pearl
    platinum titanium stone slate jet onyx ink obsidian gunmetal steel raven bone""".split()),
    set("""red crimson scarlet cherry ruby maroon burgundy wine blood brick tomato fire
    raspberry strawberry""".split()),
    set("""orange tangerine apricot peach pumpkin carrot amber rust terracotta copper
    coral salmon flame""".split()),
    set("""yellow lemon canary mustard gold golden banana honey sunshine sand cream""".split()),
    set("""green lime mint emerald jade sage moss olive forest pine fern grass kiwi
    pistachio avocado chartreuse jungle hunter kelly army leaf bottle apple""".split()),
    set("""teal turquoise aqua cyan petrol lagoon sea marine""".split()),
    set("""blue navy cobalt sapphire azure sky denim ocean indigo ultramarine cerulean
    ice arctic glacier steel midnight periwinkle""".split()),
    set("""purple violet lavender lilac plum grape eggplant amethyst orchid heather
    indigo mauve""".split()),
    set("""pink rose magenta fuchsia blush flamingo bubblegum candy watermelon berry
    salmon coral""".split()),
    set("""brown beige tan khaki chocolate coffee mocha caramel walnut cinnamon espresso
    chestnut mahogany sepia umber sienna camel taupe nude wood oak earth clay mud
    bronze linen sand ochre hazel""".split()),
]
VOCAB = MODIFIERS.union(*FAMILY_WORDS)


def words(name):
    return name.lower().replace("-", " ").split()


def plain_in_family(name, fam):
    w = words(name)
    return bool(w) and all(x in VOCAB for x in w) and any(x in FAMILY_WORDS[fam] for x in w)


def lab(r, g, b):
    def lin(c):
        c /= 255
        return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4

    rl, gl, bl = lin(r), lin(g), lin(b)
    x = (rl * 0.4124 + gl * 0.3576 + bl * 0.1805) / 0.95047
    y = rl * 0.2126 + gl * 0.7152 + bl * 0.0722
    z = (rl * 0.0193 + gl * 0.1192 + bl * 0.9505) / 1.08883

    def f(t):
        return t ** (1 / 3) if t > 0.008856 else 7.787 * t + 16 / 116

    fx, fy, fz = f(x), f(y), f(z)
    return 116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz)


def load(path):
    if zipfile.is_zipfile(path):
        with zipfile.ZipFile(path) as z:
            text = z.read(z.namelist()[0]).decode("utf-8")
    else:
        with open(path, encoding="utf-8") as fh:
            text = fh.read()
    rows = []
    for row in csv.reader(io.StringIO(text)):
        if len(row) < 2:
            continue
        best = len(row) > 2 and row[2].strip() == "x"
        name, hexv = row[0].strip(), row[1].strip()
        if not (hexv.startswith("#") and len(hexv) == 7):
            continue
        if not name.isascii() or len(name) > MAX_NAME or '"' in name or "\\" in name:
            continue
        v = int(hexv[1:], 16)
        rows.append((name, (v >> 16) & 255, (v >> 8) & 255, v & 255, best))
    return rows


def spread(cands, k):
    if len(cands) <= k:
        return cands
    labs = [lab(*c[1:]) for c in cands]
    # Seed with the lightest, then repeatedly take the farthest from the picked set.
    picked = [max(range(len(cands)), key=lambda i: labs[i][0])]
    dist = [math.dist(labs[i], labs[picked[0]]) for i in range(len(cands))]
    while len(picked) < k:
        nxt = max(range(len(cands)), key=lambda i: dist[i])
        picked.append(nxt)
        for i in range(len(cands)):
            dist[i] = min(dist[i], math.dist(labs[i], labs[nxt]))
    return [cands[i] for i in picked]


def main(src, out):
    rows = load(src)
    fams = [[] for _ in FAMILIES]
    spare = [[] for _ in FAMILIES]
    seen = set()
    for name, r, g, b, best in rows:
        if name.lower() in seen:
            continue
        seen.add(name.lower())
        f = family_of(r, g, b)
        if plain_in_family(name, f):
            fams[f].append((name, r, g, b))
        elif best:
            spare[f].append((name, r, g, b))
    for f in range(len(FAMILIES)):
        if len(fams[f]) < PER_FAMILY:
            fams[f] += spread(spare[f], PER_FAMILY - len(fams[f]))

    lines = [
        "// GENERATED by scripts/gen_color_shades.py - do not edit by hand.",
        "// Color names: meodai/color-names (MIT), https://github.com/meodai/color-names",
        "",
        '#include "src/data/cfs_shades.h"',
        "",
        "static const CfsColorPreset shades[] = {",
    ]
    starts = []
    idx = 0
    for fi, fam in enumerate(fams):
        pick = spread(fam, PER_FAMILY)
        pick.sort(key=lambda c: -lab(*c[1:])[0])
        starts.append((idx, len(pick)))
        lines.append(f"    // {FAMILIES[fi]}")
        for name, r, g, b in pick:
            lines.append(f'    {{"{name}", 0x{r:02X}, 0x{g:02X}, 0x{b:02X}}},')
        idx += len(pick)
    lines += ["};", "", "const CfsShadeFamily cfs_shade_families[CFS_SHADE_FAMILY_COUNT] = {"]
    for fi, (s, n) in enumerate(starts):
        lines.append(f'    {{"{FAMILIES[fi]}", &shades[{s}], {n}}},')
    lines += ["};", ""]
    with open(out, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))
    for fi, (s, n) in enumerate(starts):
        print(f"{FAMILIES[fi]:16} {n:3} of {len(fams[fi])}")


if __name__ == "__main__":
    main(sys.argv[1], sys.argv[2])
