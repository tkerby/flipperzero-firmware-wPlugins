#!/usr/bin/env python3
"""Static worst-case stack depth of OUR code (contract 5, Memory: thread stacks).

Compile the app sources with the ufbt flags plus `-fcallgraph-info=su -fstack-usage` (see tools/stack_depth.sh), then:
  stack_depth.py CI_DIR ROOT [--via A>B ...] [--top N]
ROOT is a function name (e.g. game_thread). The compiler's graph has no indirect calls (scene ops, natives table), so give
those edges with --via CALLER>CALLEE (names without the file path). Firmware frames below our calls (storage, printf, the
plugin loader's ELF code) are NOT known here: the number is OUR share; add the measured firmware depth (debug overlay S / T).
Frame = the compiler's static stack usage of the function (registers saved included) -- calls add nothing else."""
import re, sys, glob, os, argparse

ap = argparse.ArgumentParser()
ap.add_argument("dir")
ap.add_argument("root")
ap.add_argument("--via", nargs="*", default=[])
ap.add_argument("--top", type=int, default=1)
ap.add_argument("--auto", action="store_true", help="add the indirect edges of our code: ROOT -> every scene op (x_enter / x_input / x_update / x_draw_world / x_draw_overlay / x_leave), script_run -> nat_*, world_update -> wild_battle")
ap.add_argument("--to", help="only paths that END in this function (deepest frame counted)")
a = ap.parse_args()

size, edges, name_of = {}, {}, {}
def short(t):
    return t.rsplit(":", 1)[-1] if t.startswith("/") else t
for f in glob.glob(os.path.join(a.dir, "*.ci")):
    for line in open(f):
        m = re.match(r'node: \{ title: "([^"]+)" label: "([^\\]+)\\n[^\\]*\\n(\d+) bytes', line)
        if m:
            size[m[1]] = int(m[3])
            name_of.setdefault(short(m[1]), []).append(m[1])
            continue
        m = re.match(r'edge: \{ sourcename: "([^"]+)" targetname: "([^"]+)"', line)
        if m:
            edges.setdefault(m[1], set()).add(m[2])

def resolve(n):  # a bare (extern) name -> its defining node(s)
    if n in size:
        return [n]
    return name_of.get(short(n), [])

if a.auto:
    for sn, nodes in list(name_of.items()):
        if re.match(r"^[a-z]+_(enter|leave|input|update|draw_world|draw_overlay)$", sn) or sn.startswith("scene_battle_") and sn.endswith(("trainer", "wild")):
            a.via.append(f"{a.root}>{sn}")
        if sn.startswith("nat_"):
            a.via.append(f"script_run>{sn}")
    a.via.append("world_update>wild_battle")
for v in a.via:
    s, t = v.split(">", 1)
    for sn in name_of.get(s, []):
        for tn in name_of.get(t, []):
            edges.setdefault(sn, set()).add(tn)

best = {}
def depth(n, stack=()):
    if n in best:
        return best[n]
    if n in stack:
        return (-1, [])
    own = size.get(n, 0)
    if a.to and short(n) == a.to:
        best[n] = (own, [n])
        return best[n]
    top = (-1 if a.to else 0, [])
    for c in edges.get(n, ()):
        for cn in resolve(c):
            d = depth(cn, stack + (n,))
            if d[0] > top[0]:
                top = d
    best[n] = (own + top[0], [n] + top[1]) if top[0] >= 0 else (-1, [])
    return best[n]

roots = name_of.get(a.root, [])
if not roots:
    sys.exit(f"no function {a.root}")
res = sorted((depth(r) for r in roots), reverse=True)[: a.top]
for tot, path in res:
    print(f"{tot} B of our frames:")
    for n in path:
        print(f"  {size.get(n, 0):5d}  {short(n)}")
