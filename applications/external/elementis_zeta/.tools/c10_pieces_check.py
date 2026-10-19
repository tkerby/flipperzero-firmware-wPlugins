"""C10 (contract 68): check the generated pieces of gen_tiles c10_pieces().
1. The FLOORS (`aslab_<i%2><j%2>`, `vrock_<s>`) are still drawn in code: pixel-identical (anchor-relative) to the OLD mock's in-memory drawing (tools/attic/c10_mock_before_pieces.py).
2. The PROPS are PixelLab objects (contract 68.9, assets/art/objects_c10/*.png): the sizes / anchors the maps depend on (heights in px, base-centre anchors), the mirrored fallen column, and the pairs that are
   laid over each other in the game: the shard actor `ped_lit` floats above the socket of the pedestal object `ped0`, the stone actor `estone` is exactly the disc the empty altar `altar` lacks
   (`altar_stone` = both), the empty altar keeps no black disc pixel above its top block."""

import os
import sys

from pathlib import Path

R = str(Path(__file__).resolve().parent.parent)
sys.path.insert(0, R + "/tools")
src = (
    open(R + "/tools/attic/c10_mock_before_pieces.py")
    .read()
    .replace(
        "ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))",
        'ROOT = "%s"' % R,
    )
)
ns = {"__name__": "c10_mock_orig", "__file__": R + "/tools/c10_mock.py"}
exec(compile(src, "c10_mock_orig", "exec"), ns)
import zone_mock as zm  # noqa: E402
import lvlib  # noqa: E402


def px(sp, dy=0):
    """{(x - ax, y - ay): level} of the opaque pixels."""
    out = {}
    for y, row in enumerate(sp.lv):
        for x, v in enumerate(row):
            if sp.mk[y][x]:
                out[(x - sp.ax, y - sp.ay - dy)] = v
    return out


def actor(name):
    lv, mk, m = lvlib.read_lv_png(R + "/assets/art/npcs/%s/south/f0.lv.png" % name)
    return zm.Sp(
        lv, mk, m["ax"], m["ay"] + 6
    )  # an actor's anchor stands 6 rows above the vertex an object's anchor is on


bad = 0
n = 0


def check(cond, what):
    global bad, n
    n += 1
    if not cond:
        bad += 1
        print("FAIL", what)


def cmp(name, old):
    check(
        px(old) == px(zm.old(name))
        and (old.fw, old.fh) == (zm.old(name).fw, zm.old(name).fh),
        "floor %s differs from the old drawing" % name,
    )


for pi in (0, 1):
    for pj in (0, 1):
        cmp("aslab_%d%d" % (pi, pj), ns["ancient_slab"](pi, pj))
for s in range(3):
    cmp("vrock_%d" % s, ns["vein_rock"](s))

# ---- sizes (heights in px: gen_tiles C10_H) and anchors (the base centre, `dy` rows above the bottom)
ped, stele, fi, fj, altar, comp = (
    zm.old(k) for k in ("ped0", "ostele", "fcol_i", "fcol_j", "altar", "altar_stone")
)
for sp, h, nm in (
    (ped, 25, "ped0"),
    (stele, 40, "ostele"),
    (fi, 34, "fcol_i"),
    (fj, 34, "fcol_j"),
    (comp, 36, "altar_stone"),
):
    check(len(sp.lv) == h, "%s is %d px tall (want %d)" % (nm, len(sp.lv), h))
    check(
        sp.ay == h - 1 - 4 and abs(sp.ax - len(sp.lv[0]) // 2) <= 1,
        "%s anchor (%d, %d): base centre, 4 rows up" % (nm, sp.ax, sp.ay),
    )
    check(sp.fw == 1 and sp.fh == 1, "%s footprint 1 x 1" % nm)
seal = actor("seal")
check(
    len(seal.lv) == 44 and len(seal.lv[0]) == 44 and seal.ay == 43 - 2,
    "seal (the Meseta's sealed door) 44 x 44, anchor 2 rows up",
)

# ---- the fallen column's other orientation is the mirror image
diff = sum(
    1
    for y in range(len(fi.lv))
    for x in range(len(fi.lv[0]))
    if fi.lv[y][x] != fj.lv[y][::-1][x] or fi.mk[y][x] != fj.mk[y][::-1][x]
)
check(
    (len(fi.lv), len(fi.lv[0])) == (len(fj.lv), len(fj.lv[0]))
    and abs(fi.ax - fj.ax) <= 1
    and diff <= 0.02 * len(fi.lv) * len(fi.lv[0]),
    "fcol_j is the mirror image of fcol_i (%d resampling-rounding pixels differ)"
    % diff,
)

# ---- the lit pedestal = a shard floating above the socket of the pedestal (it must not touch the drum's body and must not overhang below the drum's top face)
lit = actor("ped_lit")
pl, pe = px(lit), px(ped)
check(
    max(y for (_, y) in pl) < min(y for (_, y) in pe) + 12
    and min(y for (_, y) in pl) < min(y for (_, y) in pe),
    "ped_lit rises above the drum",
)
check(
    abs((min(x for (x, _) in pl) + max(x for (x, _) in pl)) / 2) <= 2,
    "ped_lit is centred over the pedestal",
)
check(
    len(lit.lv) <= 22 and len(lit.lv[0]) <= 30,
    "ped_lit stays small (a streamed actor): %d x %d" % (len(lit.lv[0]), len(lit.lv)),
)

# ---- the altar: altar_stone = altar + stone actor, the stone is a disc above the block, the empty altar has none of it
stone = actor("estone")
ps, pa, pc = px(stone), px(altar), px(comp)
merged = dict(pa)
merged.update(ps)
check(merged == pc, "altar_stone == altar + estone (same anchor)")
check(
    min(y for (_, y) in ps) < min(y for (_, y) in pa),
    "the stone floats above the empty altar's top block",
)
check(
    len(ps) >= 60
    and sum(1 for v in ps.values() if v == 3) >= 20
    and sum(1 for v in ps.values() if v == 0) >= 8,
    "the stone has its black disc and white corona",
)
check(
    len(altar.lv) <= len(comp.lv) - 5
    and sum(1 for v in pa.values() if v == 0)
    > sum(1 for v in pc.values() if v == 0) - 40,
    "the empty altar is the block without the stone (5+ rows shorter, the corona gone)",
)

print("checks:", n, "failures:", bad)
sys.exit(1 if bad else 0)
