#!/usr/bin/env python3
"""Test of the chapter 9 CONTENT (contract 56): the two mapc additions (`setv VAR sp:<species>` = a species id by name, `caught_ge VAR full` = every species but the mythical gift), the zone banner keys and their
consecutive texts (`TXT_GL_AREA_LAST`), the named story flags 904..923 (none in the test fixtures' 900..903 / 940 / 950, none twice), the League's door idiom on the real maps (members' beaten flags = league_eN,
doors with show / hide flags, `oneway` warps, no warp back), Ruta 23's eight badge guards (hide flags 80..87), the Senda's gates (a boulder, a pit, a step trigger in front of the gate), the legends, the shop and the
music lines. The world cases copy maps/ to a temporary root, change a line and run the whole load + link + validate.
  python3 -I tools/mapc_c9content_test.py        (exit status 1 on a failure; part of tools/build_assets.sh)"""
import shutil
import sys
import tempfile
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mapc  # noqa: E402

fails = 0


def check(what, cond):
    global fails
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        fails += 1


def world(edit=None):
    td = Path(tempfile.mkdtemp())
    try:
        (td / "assets").symlink_to(ROOT / "assets")
        shutil.copytree(ROOT / "maps", td / "maps")
        for nm, fn in (edit or {}).items():
            p = td / f"maps/{nm}.map"
            p.write_text(fn(p.read_text(encoding="utf-8")), encoding="utf-8")
        rep = mapc.Report()
        w = mapc.load_world(td, rep)
        return w, rep
    finally:
        shutil.rmtree(td, ignore_errors=True)


w, rep = world()
check(f"the real world loads without errors ({rep.errors[:2]})", not rep.errors)
by = {m.name: m for m in w.maps}
NAMES = [
    "ruta_22",
    "puerta_liga",
    "ruta_23",
    "senda_1",
    "senda_2",
    "senda_3",
    "meseta_cumbre",
    "liga_lobby",
    "liga_hielo",
    "liga_lucha",
    "liga_fantasma",
    "liga_dragon",
    "liga_campeon",
    "cueva_secreta_1",
    "cueva_secreta_2",
    "cueva_secreta_3",
    "casa_lengua",
]
check(
    "the 17 maps have the ids 160 .. 176 in this order (the chapter's reserved range is 160 .. 189)",
    [by[n].id for n in NAMES] == list(range(160, 177)),
)

# ---- the two additions
sp = mapc.species_ids()
lab = by["villa_lab"]
# D1 (contract 68): Lin no longer gives Genesa (her genesa_gift script is gone), so the `sp:` idiom is tested on a script added to villa_lab for the occasion
SP_TEST = "\n@script sp_test called\n    setv 0 sp:genesa\n@end\n"
wsp, repsp = world({"villa_lab": lambda t: t + SP_TEST})
labsp = next(m for m in wsp.maps if m.name == "villa_lab")
sc = next(s for s in labsp.scripts if s.name == "sp_test")
setv = [i for i in sc.ins if i.op == "setv"]
check(
    "a script with `setv 0 sp:genesa` compiles (the world loads without errors)",
    len(setv) == 1 and setv[0].args == ["0", "sp:genesa"] and not repsp.errors,
)
check(
    "villa_lab no longer has Lin's genesa_gift (contract 68: she gives the first Origin Fragment, `frag_gift`)",
    not any(s.name == "genesa_gift" for s in lab.scripts)
    and any(s.name == "frag_gift" for s in lab.scripts),
)
ctx = mapc.ScriptCtx(w, lab)
check(
    "`sp:genesa` resolves to Genesa's species id by name (so the script survives the species ids moving)",
    ctx.num("sp:genesa", 0, 255, "operand") == sp["genesa"]
    and sp["genesa"] + 1 == sp["arkegen"]
    and sp["arkegen"] + 1 == sp["quillia"],
)
try:
    ctx.num("sp:nope", 0, 255, "operand")
    bad = False
except ValueError:
    bad = True
check("an unknown species name after `sp:` is an error (ValueError)", bad)
lin = next(s for s in lab.scripts if s.name == "lin")
cg = [
    i
    for i in lin.ins
    if i.op == "call_native" and i.args[0] == "23" and i.args[2] != "0"
]
check(
    "`caught_ge 0 full` compiles to native 23 with N = the species count - 1 (everything but Genesa)",
    len(cg) == 1 and int(cg[0].args[2]) == len(sp) - 1,
)
w2, rep2 = world(
    {"villa_lab": lambda t: t.replace("caught_ge 0 full", "caught_ge 0 0")}
)
check(
    "`caught_ge VAR 0` is still refused (N = 1..255 or full)",
    any("caught_ge" in e for e in rep2.errors),
)
w3, rep3 = world(
    {"villa_lab": lambda t: (t + SP_TEST).replace("setv 0 sp:genesa", "setv 0 sp:nope")}
)
check(
    "an unknown `sp:` species in a real script is an error of the world load",
    bool(rep3.errors),
)

# ---- zone banners
keys = mapc.AREA_KEYS
i0 = keys.index(
    "central_electrica"
)  # (C7's seven zone banners come first, merge of chapters 7 and 9)
check(
    "AREA_KEYS: ruta_22, ruta_23, senda_campeones, meseta_cumbre, cueva_secreta follow C7's last (central_electrica) (in this order)",
    list(keys[i0 + 1 : i0 + 6])
    == ["ruta_22", "ruta_23", "senda_campeones", "meseta_cumbre", "cueva_secreta"],
)
h = mapc.emit_header_h(w)
check(
    "maps_gen.h defines TXT_GL_AREA_LAST (the last area key's text), which world.c's static assert uses",
    f"#define TXT_GL_AREA_LAST TXT_GL_AREA_{keys[-1].upper()}" in h,
)
src = (ROOT / "nucleo/world.c").read_text()
check(
    "world.c asserts the banner texts are consecutive up to TXT_GL_AREA_LAST",
    "TXT_GL_AREA_LAST - TXT_GL_AREA_VILLA_ALISO == AREA_COUNT - 1" in src,
)
for lang in ("es", "en", "ja", "zh"):
    t = (ROOT / f"assets/text/{lang}.txt").read_text(encoding="utf-8")
    order = [ln[1:-1] for ln in t.split("\n") if ln.startswith("[gl.area.")]
    check(
        f"{lang}.txt: the gl.area.* keys are in AREA_KEYS order, consecutive",
        order == [f"gl.area.{k}" for k in keys],
    )

# ---- flags
allflags = {}
for m in w.maps:
    for name, num in m.flag_defs.items():
        allflags.setdefault(m.name, {})[name] = num
mine = {}
for n in NAMES + ["villa_lab", "ciudad_marea", "ciudad_nodo", "ruta_18"]:
    for name, num in allflags.get(n, {}).items():
        if 904 <= num <= 939:
            mine[name] = num
check(
    "the chapter's named flags sit in 904 .. 923 (the reserved range is 900 .. 939; 900 .. 903, 940 and 950 are the test_flags fixture's) and none is used twice",
    sorted(mine.values()) == list(range(904, 924))
    and len(set(mine.values())) == len(mine),
)
check(
    "the flags are what the contract says (r22_done 904, liga_checked 905, pits 906..908, gates 909..911, the legends 912 / 913, the trade 914, Genesa 915, trainers 916..923)",
    mine.get("r22_done") == 904
    and mine.get("liga_checked") == 905
    and [mine[f"senda_pit{k}"] for k in (1, 2, 3)] == [906, 907, 908]
    and [mine[f"senda{k}_open"] for k in (1, 2, 3)] == [909, 910, 911]
    and mine.get("ignicondor_gone") == 912
    and mine.get("arkegen_gone") == 913
    and mine.get("tonguard_done") == 914
    and mine.get("genesa_given") == 915,
)

# ---- Ruta 23: eight guards
r23 = by["ruta_23"]
guards = sorted((n.j, n.hide_flag) for n in r23.npcs if n.name == "guardia")
check(
    "Ruta 23: eight guards on the centre column (7), each a gate NPC with hide_flag = the badge 80 .. 87 in order (rows 40, 36, 32, 28, 24, 13, 9, 5)",
    len(guards) == 8
    and [f for _, f in sorted(guards, reverse=True)] == list(range(80, 88))
    and all(
        n.gate and n.flags & 1 and n.i == 7 for n in r23.npcs if n.name == "guardia"
    ),
)
check(
    "Ruta 23 is surfable (`@surf yes`) and has a water table and a grass table",
    r23.surf and r23.tables.get("water") and r23.enc,
)

# ---- the Senda
for k, nm in enumerate(("senda_1", "senda_2", "senda_3"), 1):
    m = by[nm]
    boulders = [n for n in m.npcs if n.gen == "boulder"]
    holes = [n for n in m.npcs if n.gen == "hole"]
    gates = [n for n in m.npcs if n.name == "rock_gate"]
    check(
        f"{nm}: one boulder, one pit (hide flag senda_pit{k} = {905 + k}), one gate NPC (hide flag senda{k}_open = {908 + k}), a conditional step trigger in front of the gate",
        len(boulders) == 1
        and len(holes) == 1
        and holes[0].hide_flag == 905 + k
        and len(gates) == 1
        and gates[0].hide_flag == 908 + k
        and gates[0].gate
        and any(it.mode == 1 and it.script == f"gate{k}_check" for it in m.interacts)
        and next(s for s in m.scripts if s.name == f"gate{k}_check").cond,
    )
    check(
        f"{nm}: a cave (the Exit Wisp works) whose floor is the encounter ground: a grass table and most floor cells have the grass flag",
        m.cave and m.enc and sum(1 for c in m.cflags if c & mapc.C_GRASS) > 100,
    )
s3 = by["senda_3"]
lg = [n for n in s3.npcs if n.gen == "legend"]
check(
    "senda_3: Ignicondor is a legend NPC (ignicondor_ow) with hide flag ignicondor_gone (912)",
    len(lg) == 1 and lg[0].name == "ignicondor_ow" and lg[0].hide_flag == 912,
)
c3 = by["cueva_secreta_3"]
lg = [n for n in c3.npcs if n.gen == "legend"]
check(
    "cueva_secreta_3: Arkegen is a legend NPC (arkegen_ow) with hide flag arkegen_gone (913); the map is surfable",
    len(lg) == 1 and lg[0].name == "arkegen_ow" and lg[0].hide_flag == 913 and c3.surf,
)

# ---- the League
lobby = by["liga_lobby"]
check(
    "the lobby's doorway warp is `oneway` into liga_hielo; the lobby has the respawn on_enter script and the League door NPC (hide flag liga_checked)",
    lobby.warps
    and any(wp.oneway and wp.target_map == "liga_hielo" for wp in lobby.warps)
    and any(
        s.name == "lobby" and s.trigger == mapc.SCRT_NAMES["on_enter"]
        for s in lobby.scripts
    )
    and any(n.ident == "door" and n.hide_flag == 905 and n.gate for n in lobby.npcs),
)
chain = ["liga_hielo", "liga_lucha", "liga_fantasma", "liga_dragon", "liga_campeon"]
for k, nm in enumerate(chain):
    m = by[nm]
    npc = {n.ident: n for n in m.npcs}
    if k < 4:
        check(
            f"{nm}: the member's beaten flag is league_e{k + 1} ({961 + k}), the door ahead hides with it, the doorway warp is `oneway` into {chain[k + 1]}",
            npc["member"].talk_flag == 961 + k
            and npc["door_a"].hide_flag == 961 + k
            and npc["door_a"].gate
            and len(m.warps) == 1
            and m.warps[0].oneway
            and m.warps[0].target_map == chain[k + 1],
        )
    else:
        check(
            "liga_campeon: no warp, the champion's script does `ending run` and `ending title`",
            not m.warps
            and sum(
                1
                for s in m.scripts
                if s.name == "champ"
                for i in s.ins
                if i.op == "call_native" and i.args[0] == "22"
            )
            == 2,
        )
    check(
        f"{nm}: the door behind (5,11) shows with league_run (960) and is solid",
        npc["door_x"].show_flag == 960
        and npc["door_x"].flags & 1
        and (npc["door_x"].i, npc["door_x"].j) == (5, 11),
    )
check(
    "only room 1 has the `league_in` on_enter script",
    [nm for nm in chain if any(s.name == "league_in" for s in by[nm].scripts)]
    == ["liga_hielo"],
)
check(
    'no "has no warp back" warning for the League\'s one-way chain',
    not any(("liga_" in e and "has no warp back" in e) for e in rep.warnings),
)

# ---- data
sh = tomllib.load(open(ROOT / "assets/data/shops.toml", "rb"))
it_kind = lambda k: tomllib.load(open(ROOT / "assets/data/items.toml", "rb"))[k]["kind"]
check(
    "shops.toml has the Meseta's mart `meseta_cumbre` (15 items since contract 73: the 11 of 58 + Vital Draught, Life Spark, Grand Ward (64) + Cure-All; no key item)",
    "meseta_cumbre" in sh
    and len(sh["meseta_cumbre"]["items"]) == 15
    and all(it_kind(i) != "key" for i in sh["meseta_cumbre"]["items"]),
)
idx = (ROOT / "assets/music/index.txt").read_text()
check(
    "index.txt maps every new map (Ruta 22 `route`, the road and the Senda `victoria`, the lobby `centro`, the five rooms `alto_mando`, the caves `eco`) and gives the champion `campeon`, the members `battle_gym`",
    all(f"map {n} " in idx for n in NAMES)
    and "map liga_campeon alto_mando" in idx
    and "trainer rival_liga campeon" in idx
    and all(
        f"trainer {t} battle_gym" in idx for t in ("iver", "tanko", "velo", "arkan")
    ),
)
sw = ROOT / "tools/c9_c7_swap.py"
check(
    "tools/c9_c7_swap.py exists (the stand-ins for C7's species in Iver's and Arkan's teams)",
    sw.is_file(),
)
print("mapc_c9content_test:", "all ok" if not fails else f"FAILED {fails}")
sys.exit(1 if fails else 0)
