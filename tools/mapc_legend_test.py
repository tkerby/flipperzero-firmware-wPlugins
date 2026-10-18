#!/usr/bin/env python3
"""Test of mapc's legendary-encounter idiom (contract 52): the legend attribute `legend=<species> level=<n> npc=<actor> [hide_flag=]` (its generated script, its refusals, the wild level with bit 7), the catalog actors
of the three birds (tools/legend_ow.py), and the music lines (the `legendario` track, the `legend_cry` effect).
  python3 -I tools/mapc_legend_test.py        (exit status 1 on a failure; part of tools/build_assets.sh)"""
import os
import re
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mapc  # noqa: E402
import spritec  # noqa: E402

fails = 0


def check(what, cond):
    global fails
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        fails += 1


HEAD = """@map t
@size 6 3
@interior no
@bg 0
@fill dirt_a
@spawn start {spawn} S
"""


def parse(extra_head="", legend="", grid=("......", "......", "......"), tail="", spawn="5 2"):
    src = HEAD.format(spawn=spawn) + extra_head + "@legend\n  .  tile=dirt_a\n" + legend + "@grid\n" + "".join(f"  {r}\n" for r in grid) + tail
    with tempfile.NamedTemporaryFile("w", suffix=".map", delete=False, dir=tempfile.gettempdir()) as f:
        f.write(src)
        path = f.name
    rep = mapc.Report()
    cat = mapc.Catalog(ROOT / "assets/catalog.toml", rep)
    try:
        m = mapc.parse_map(path, cat, rep, name_hint="t")
    finally:
        os.unlink(path)
    return m, rep


def errs(rep, needle):
    return any(needle in e for e in rep.errors)


G = ("L.....", "......", "......")
MUNCRIT = mapc.species_ids()["muncrit"]

# ---- the idiom ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
m, rep = parse(legend="  L  tile=dirt_a legend=muncrit level=40 npc=aurowl_ow face=SW\n", grid=G)
check(f"legend=SPECIES level=N npc=ACTOR compiles ({rep.errors})", m is not None and not rep.errors and len(m.npcs) == 1 and m.npcs[0].script == "legend_0")
if m and m.npcs:
    n = m.npcs[0]
    sc = next(s for s in m.scripts if s.name == "legend_0")
    ops = [(i.op, i.args[0]) if i.op == "call_native" else (i.op,) for i in sc.ins]
    check("the script: npc_face_hero, sfx (native 23), say, wild_set (native 9), wild_go (native 10), jv (ran), jv (not opened), setf",
          ops == [("npc_face_hero",), ("call_native", "23"), ("say",), ("call_native", "9"), ("call_native", "10"), ("jv",), ("jv",), ("setf",)])
    check("wild_set takes the species and the level WITH BIT 7 (40 + 128 = 168): the battle's legendary flag", [i.args for i in sc.ins if i.op == "call_native" and i.args[0] == "9"] == [["9", str(MUNCRIT), "168"]])
    check("the encounter's result goes to var 0 and `jv 0 2` (RAN) and `jv 0 4` (WILD_NOTOPEN, §72) skip the setf: it stays", [i.args for i in sc.ins if i.op == "call_native" and i.args[0] == "10"] == [["10", "0", "0"]]
          and [i.args[:2] for i in sc.ins if i.op == "jv"] == [["0", "2"], ["0", "4"]])
    check("the effect is the `legend_cry` one, the text `gl.c52.cry`", any(i.op == "call_native" and i.args[:2] == ["23", "144"] and i.args[2] == "legend_cry" for i in sc.ins)
          and [i.args[0] for i in sc.ins if i.op == "say"] == ["gl.c52.cry"])
    check("the NPC is solid, faces SW, gets an automatic hide flag, is a `legend`", n.flags & 1 and n.flag_auto and n.gen == "legend" and n.facing == mapc.parse_facing("SW"))
    check("it is a gate for the reachability checks (the cell behind it is reachable)", not m.solid_at(0, 0))
m, rep = parse(legend="  L  tile=dirt_a legend=muncrit level=100 npc=fulgurion_ow\n", grid=G)
check("level 100 is the top (+ 128 = 228 still one byte)", m is not None and not rep.errors and any(i.op == "call_native" and i.args == ["9", str(MUNCRIT), "228"] for i in m.scripts[0].ins))
m, rep = parse(legend="  L  tile=dirt_a legend=muncrit npc=aurowl_ow\n", grid=G)
check("legend= without level= is refused", errs(rep, "level="))
for bad in ("0", "101", "200"):
    m, rep = parse(legend=f"  L  tile=dirt_a legend=muncrit level={bad} npc=aurowl_ow\n", grid=G)
    check(f"level {bad} is refused (bit 7 is the engine's, not the map's)", errs(rep, "level="))
m, rep = parse(legend="  L  tile=dirt_a legend=nosuchbird level=5 npc=aurowl_ow\n", grid=G)
check("an unknown species is refused", errs(rep, "unknown species"))
m, rep = parse(legend="  L  tile=dirt_a legend=muncrit level=5\n", grid=G)
check("legend= without npc= is refused", errs(rep, "need npc="))
m, rep = parse(legend="  L  tile=dirt_a legend=muncrit level=5 npc=aurowl_ow text=a.b\n", grid=G)
check("legend= with text= is refused (the script is generated)", errs(rep, "generates the NPC"))
m, rep = parse(legend="  L  tile=dirt_a legend=muncrit level=5 npc=aurowl_ow script=x\n", grid=G)
check("legend= with script= is refused", errs(rep, "generates the NPC"))
m, rep = parse(legend="  L  tile=dirt_a legend=muncrit level=5 npc=nosuchactor\n", grid=G)
check("an unknown actor is refused", errs(rep, "unknown actor"))
m, rep = parse("@flag gone 792\n", legend="  L  tile=dirt_a legend=muncrit level=5 npc=aurowl_ow hide_flag=gone\n", grid=G)
check(f"an explicit hide_flag= (an @flag name) is the bird's flag ({rep.errors})", m is not None and not rep.errors and m.npcs[0].hide_flag == 792 and not m.npcs[0].flag_auto)
m, rep = parse(legend="  t  tile=dirt_a npc=aurowl_ow text=a.a level=3\n", grid=("t.....", "......", "......"))
check("level= without sleeper= / legend= / gift= is refused", errs(rep, "level= needs"))
m, rep = parse(legend="  L  tile=dirt_a legend=muncrit level=5 npc=aurowl_ow\n  M  tile=dirt_a legend=muncrit level=5 npc=ignicondor_ow\n", grid=("L.M...", "......", "......"))
check("two legendary NPCs on one map each get their own script and flag", m is not None and not rep.errors and [n.script for n in m.npcs] == ["legend_0", "legend_1"] and m.npcs[0].flag_tok != m.npcs[1].flag_tok)

# ---- the actors of the three birds -------------------------------------------------------------------------------------------------------------------------------------------------------
cat = spritec.load_catalog(str(ROOT / "assets/catalog.toml"))
for sp in ("aurowl", "fulgurion", "ignicondor"):
    name = sp + "_ow"
    try:
        s = cat.sprite(name)
        ok = s.entry.kind == "actor" and s.ndirs == 8 and 32 <= s.h <= 40 and s.w <= 48
        check(f"{name}: an 8-facing actor of {s.w} x {s.h} px (32 .. 40 rows, 1 frame), the west facings folded", ok and len(spritec.stored_dirs(s)) == 5)
        check(f"{name}: the light conversion of its art (dark_cut 25, split 0.35 / 0.75 on every stored facing)", s.entry.raw.get("dark_cut") == 25 and set(s.entry.raw.get("adaptive_split", {})) == {"south", "south-east", "east", "north-east", "north"}
              and all(v == [0.35, 0.75] for v in s.entry.raw["adaptive_split"].values()))
        # the art is a scaled copy of the creature picture: the 5 files exist and are newer than their sources
        files = [ROOT / "assets/art/npcs" / name / (f + ".png") for f in spritec.ORDER8[:5]]
        check(f"{name}: south / south-east / east / north-east / north files exist", all(f.is_file() for f in files))
        # readable on a light floor: the outline (level 3) and a body of at least two tones
        lv, mk = s.frames[0][0]
        tones = {lv[y][x] for y in range(s.h) for x in range(s.w) if mk[y][x]}
        edge = sum(1 for y in range(s.h) for x in range(s.w) if mk[y][x] and lv[y][x] == 3)
        total = sum(1 for y in range(s.h) for x in range(s.w) if mk[y][x])
        check(f"{name}: the south view has an outline and at least three tones ({sorted(tones)}, {edge} / {total} px black)", 3 in tones and len(tones) >= 3 and 0.08 < edge / total < 0.55)
    except Exception as e:  # noqa: BLE001
        check(f"{name}: loads from the catalog ({e})", False)
import subprocess  # noqa: E402
r = subprocess.run([sys.executable, "-I", str(ROOT / "tools/legend_ow.py"), "check"], capture_output=True, text=True)
check(f"`legend_ow.py check`: the actor art is newer than the creature pictures ({r.stdout.strip()[:80]})", r.returncode == 0)

# ---- the music ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
idx = (ROOT / "assets/music/index.txt").read_text().splitlines()
tracks = [re.split(r"[\s#]+", l.strip())[1] for l in idx if re.match(r"(music|fx)\s", l.strip())]
kinds = [l.split()[0] for l in idx if re.match(r"(music|fx)\s", l.strip())]
check("index.txt has `music legendario` among the music lines (before the first fx line)", "legendario" in tracks and kinds[tracks.index("legendario")] == "music" and tracks.index("legendario") < kinds.index("fx"))
check("index.txt has `fx legend_cry`", "legend_cry" in tracks and kinds[tracks.index("legend_cry")] == "fx")
gen = (ROOT / "nucleo/gen/music_gen.h").read_text()
mm = re.search(r"#define SND_LEGENDARIO (\d+)", gen)
fx0 = re.search(r"#define SND_FX0 (\d+)", gen)
fx = re.search(r"#define SND_FX_LEGEND_CRY (\d+)", gen)
check("music_gen.h: SND_LEGENDARIO is a music id (below SND_FX0), SND_FX_LEGEND_CRY an effect id", bool(mm and fx0 and fx) and int(mm.group(1)) < int(fx0.group(1)) <= int(fx.group(1)))
info = subprocess.run([sys.executable, "-I", str(ROOT / "tools/musc.py"), "info"], capture_output=True, text=True).stdout
row = next((l.split() for l in info.splitlines() if len(l.split()) > 6 and l.split()[1] == "legendario"), None)
check(f"`legendario`: a looping music record of at most 400 bytes with a 45 - 60 s loop ({row})", bool(row) and row[-2] == "loop" or bool(row) and row[-3] == "loop")
if row:
    rec, loop_s = int(row[5]), float(row[7])
    check(f"... {rec} bytes, intro {row[6]} s, loop {loop_s} s", rec <= 400 and 45 <= loop_s <= 60 and row[8] == "loop")
rowf = next((l.split() for l in info.splitlines() if len(l.split()) > 6 and l.split()[1] == "legend_cry"), None)
check(f"`legend_cry`: an effect under a second and a half ({rowf})", bool(rowf) and rowf[8] == "fx" and float(rowf[7]) < 1.5)
print(f"mapc_legend_test: {'FAILED ' + str(fails) if fails else 'all ok'}")
sys.exit(1 if fails else 0)
