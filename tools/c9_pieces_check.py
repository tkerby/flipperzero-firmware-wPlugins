"""C9 (contract 56): compare every piece of the OLD c9 mock (the in-memory drawing, tools/attic/c9_mock_before_pieces.py) with the generated files of gen_tiles c9_pieces(): pixel-identical (anchor-relative)."""
import os
import sys

from pathlib import Path
R = str(Path(__file__).resolve().parent.parent)
sys.path.insert(0, R + "/tools")
src = open(R + "/tools/attic/c9_mock_before_pieces.py").read().replace("ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))", 'ROOT = "%s"' % R)
ns = {"__name__": "c9_mock_orig", "__file__": R + "/tools/c9_mock.py"}
exec(compile(src, "c9_mock_orig", "exec"), ns)
import zone_mock as zm  # noqa: E402
import c7_mock as c7  # noqa: E402


def px(sp):
    """{(x - ax, y - ay): level} of the opaque pixels."""
    out = {}
    for y, row in enumerate(sp.lv):
        for x, v in enumerate(row):
            if sp.mk[y][x]:
                out[(x - sp.ax, y - sp.ay)] = v
    return out


bad = 0
n = 0


def cmp(name, old):
    global bad, n
    n += 1
    new = zm.old(name)
    a, b = px(old), px(new)
    if a != b or (old.fw, old.fh) != (new.fw, new.fh):
        bad += 1
        print("DIFF", name, len(a), len(b), "anchor", (old.ax, old.ay), (new.ax, new.ay))


for pi in (0, 1):
    for pj in (0, 1):
        cmp("marb_%d%d" % (pi, pj), ns["marble"](pi, pj))
        cmp("ringm_%d%d" % (pi, pj), ns["tatami_ring"](pi, pj))
        cmp("ghf_%d%d" % (pi, pj), ns["ghost_floor"](pi, pj))
        cmp("iceslab_%d%d" % (pi, pj), c7.ice_slab(pi, pj))
for s in range(3):
    cmp("rockf_%d" % s, ns["rock_floor"](s))
    cmp("crys_%d" % s, ns["crystal_floor"](s))
cmp("bgate", ns["badge_gate"]())
cmp("snowdrift", c7.snow_drift())
cmp("lstatue", ns["statue"]())
cmp("ringpost", ns["ring_post"]())
for k in (0, 1):
    cmp("torch_%d" % k, ns["torch"](k))
    cmp("cand_%d" % k, ns["candelabra"](k))
    cmp("gcry_%d" % k, ns["glow_crystal"](k))
cmp("ban_i", ns["banner"]("i"))
cmp("ban_j", ns["banner"]("j"))
cmp("icecr", ns["pl_object"]("ice_crystals", 34))
cmp("dstat", ns["pl_object"]("dragon_statue", 44))
cmp("cpil", ns["pl_object"]("champion_column", 44))
cmp("troph", ns["pl_object"]("champion_trophy", 30))
print("pieces compared:", n, "differences:", bad)
sys.exit(1 if bad else 0)
