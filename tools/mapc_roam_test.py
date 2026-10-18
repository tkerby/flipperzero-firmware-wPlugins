#!/usr/bin/env python3
"""Test of mapc's roaming-creature directive (contract 55, the PILOT on Ruta 1): `@roam <n> [table=grass] [area=<chars>] [period=<ms>]` (the roam area mask, the NPC slots, the bank actors, every refusal), the extras
section's roam block and its round trip, and the generated actors of tools/roam_ow.py (assets/catalog_roam.toml).
  python3 -I tools/mapc_roam_test.py        (exit status 1 on a failure; part of tools/build_assets.sh)"""
import os
import subprocess
import sys
import tempfile
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mapc  # noqa: E402
import mapdump  # noqa: E402
import spritec  # noqa: E402

fails = 0


def check(what, cond):
    global fails
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        fails += 1


HEAD = """@map t
@size {w} {h}
@interior no
@bg 0
@fill grass
@spawn start 1 1 S
"""
TABLE = "@encounters grass rate=25\n  flitter 3 6 60\n  muncrit 3 6 40\n@end\n"


def parse(roam="@roam 2 area=.f\n", table=TABLE, legend="", grid=None, w=12, h=6):
    if grid is None:
        grid = ["BBBBBBBBBBBB",
                "B.f..gg....B",
                "B....gg..wwB",
                "B..x......rB",
                "B..........B",
                "BBBBBBBBBBBB"]
    src = HEAD.format(w=w, h=h) + table + roam + ("@legend\n  .  tile=grass\n  f  tile=flowers\n  =  tile=path\n  g  tile=tall_grass\n  w  tile=sea_o\n  B  object=bush\n  r  object=rock\n"
                                                    "  x  tile=grass npc=vecino id=talker face=S text=tr.test.after\n" + legend) + "@grid\n" + "".join(f"  {r}\n" for r in grid)
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


def cells(m):
    return {(i, j) for j in range(m.h) for i in range(m.w) if m.roam_mask[(j * m.w + i) >> 3] >> ((j * m.w + i) & 7) & 1}


# ---- the directive and what it makes ---------------------------------------------------------------------------------------------------------------------------------------------------
m, rep = parse()
check(f"`@roam 2 area=.f` compiles ({rep.errors})", m is not None and not rep.errors and m.roam is not None)
if m and m.roam:
    check("the directive is kept: 2 roamers, grass table, area '.f', default-less period", m.roam[:3] == (2, "grass", ".f") and m.roam[3] == mapc.ROAM_PERIOD)
    roam = [n for n in m.npcs if n.gen == "roam"]
    check("two NPC slots, the LAST ones (after the talker), behavior 6, solid, no text / script, wander param = period / 250, the show flag 1021 (never set)",
          len(roam) == 2 and m.npcs[-2:] == roam and len(m.npcs) == 3 and all(n.behavior == mapc.NPCB_ROAM == 6 and n.flags == 1 and not n.script and not n.text_key and n.param == mapc.ROAM_PERIOD // 250
                                                                               and n.show_flag == mapc.ROAM_OFF_FLAG == 1021 and not n.hide_flag and not n.talk_flag for n in roam))
    check("a roamer is not a wall for the reachability checks", all(not m.solid_at(n.i, n.j) for n in roam))
    ar = cells(m)
    want = {(i, j) for j in range(1, m.h - 1) for i in range(1, m.w - 1) if m.chars[j][i] in ".f" and (i, j) not in {(3, 3), (1, 1)}}   # not the talker's cell, not the spawn
    check(f"the area = the cells of '.f' off the border, minus the NPC and spawn cells ({len(ar)} cells, {len(ar ^ want)} different)", ar == want)
    check("no tall grass, water, rock, bush or border cell in it", not any(m.chars[j][i] in "gwrB" for i, j in ar) and all(0 < i < m.w - 1 and 0 < j < m.h - 1 for i, j in ar))
    check("the bank has an actor `roam_<species>` per grass slot and the map's table order is kept: roam_spr per slot, both actors in the bank",
          len(m.roam_spr) == 2 and m.roam_spr[0] != m.roam_spr[1] and {m.bank[k] for k in m.roam_spr} == {"roam_flitter", "roam_muncrit"})
    check("the roamer NpcDefs use the first slot's actor", all(n.name == "roam_flitter" for n in roam))
    check("the mask is ceil(w * h / 8) bytes", len(m.roam_mask) == (m.w * m.h + 7) // 8)

# default area: every walkable cell but the tall grass
m, rep = parse(roam="@roam 1\n")
check(f"`@roam 1` (no area) compiles ({rep.errors})", m is not None and not rep.errors and m.roam[:3] == (1, "grass", ""))
if m and m.roam:
    ar = cells(m)
    check("without area= the tall grass, the water, the rock and the bush are out, the flowers and plain grass in", not any(m.chars[j][i] in "gwrB" for i, j in ar) and (2, 1) in ar and (1, 2) in ar)
m, rep = parse(roam="@roam 1 area=.fg\n")
check("an explicit area may include the tall grass (a cave floor that is encounter ground): the author's call", m is not None and not rep.errors and (5, 1) in cells(m) and (9, 2) not in cells(m))
m, rep = parse(roam="@roam 3 period=800\n")
check("period= is ms; 800 -> param 3 (800 / 250)", m is not None and not rep.errors and [n.param for n in m.npcs if n.gen == "roam"] == [3, 3, 3])
m, rep = parse(roam="@roam 1 area==.\n")
check("`area==.`: a chars list may contain '=' (the path tile)", m is not None and not rep.errors and m.roam[2] == "=.")

# ---- refusals ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
for what, roam, needle in (("0 roamers", "@roam 0\n", "@roam <1..3>"), ("4 roamers (the cap is 3)", "@roam 4\n", "@roam <1..3>"), ("no count", "@roam\n", "@roam <1..3>"),
                           ("a period below 250 ms", "@roam 1 period=100\n", "period="), ("an unknown option", "@roam 1 speed=3\n", "@roam <1..3>"),
                           ("a table that is not the grass one", "@roam 1 table=water\n", "only `grass`"), ("two @roam", "@roam 1\n@roam 1\n", "at most one")):
    m, rep = parse(roam=roam)
    check(f"{what} is refused", errs(rep, needle))
m, rep = parse(table="")
check("@roam without `@encounters grass` is refused", errs(rep, "needs the map's `@encounters grass`"))
m, rep = parse(roam="@roam 3 area=x\n")
check("an area too small for the roamers (needs 4 cells each) is refused", errs(rep, "free walkable cell"))
m, rep = parse(roam="@roam 1 area=.\n", grid=["BBBBBBBBBBBB", "B..........B", "B..........B", "B..........B", "B..........B", "BBBBBBBBBBBB"], w=12, h=6)
check("a plain area compiles", m is not None and not rep.errors)
m, rep = parse(table="@encounters grass rate=25\n  embercub 3 6 60\n@end\n")
check("a species without a roam actor is refused with the fix (run tools/roam_ow.py build)", errs(rep, "roam_ow.py"))
# the 16-NPC cap
leg = "".join(f"  {c}  tile=grass npc=vecino id=n{k} face=S text=tr.test.after\n" for k, c in enumerate("CDEFGHIJKLMNOPQS"))


def grid_n(n):   # a 23 x 6 meadow with n talkers in the row j = 1 (the spawn is at (1, 1))
    return ["B" * 23, "B." + "CDEFGHIJKLMNOPQS"[:n] + "." * (20 - n) + "B"] + ["B" + "." * 21 + "B"] * 3 + ["B" * 23]


m, rep = parse(roam="@roam 1\n", legend=leg, grid=grid_n(16), w=23, h=6)
check(f"16 NPCs + @roam 1: refused, the message counts the free slots ({[e for e in rep.errors if 'cap' in e][:1]})", errs(rep, "the cap is 16") and errs(rep, "0 slot(s) are free"))
m, rep = parse(roam="@roam 3\n", legend=leg, grid=grid_n(13), w=23, h=6)
check(f"13 NPCs + @roam 3 = 16: fits ({rep.errors})", m is not None and not rep.errors and len(m.npcs) == 16)
m, rep = parse(roam="@roam 3\n", legend=leg, grid=grid_n(14), w=23, h=6)
check("14 NPCs + @roam 3: refused (2 slots free)", errs(rep, "2 slot(s) are free"))

# ---- the real maps and the file format ---------------------------------------------------------------------------------------------------------------------------------------------------
rep = mapc.Report()
world = mapc.load_world(ROOT, rep)
r1 = world.by_name("ruta_1")
tr = world.by_name("test_roam")
check(f"Ruta 1 (the pilot) has `@roam 2 table=grass area=.=f period=1800`: roamers 2, table 2 slots ({rep.errors[:2]})", r1 is not None and r1.roam == (2, "grass", ".=f", 1800, r1.roam[4]) and len(r1.enc) == 2 and not rep.errors)
check("Ruta 1 keeps its tall-grass table (flitter 2..5 w55, muncrit 2..5 w45) and the scripted NPCs before the roamers", r1 is not None and [(e[4], e[1], e[2], e[3]) for e in r1.enc] == [("flitter", 2, 5, 55), ("muncrit", 2, 5, 45)]
      and [n.gen for n in r1.npcs[-2:]] == ["roam", "roam"] and len(r1.npcs) == 3 and r1.enc_rate == 25 and r1.open_rate == 2)   # (contract 63: the one-off Flitter is gone: the clerk + the 2 roamers)
check("the test map (id 227) has 3 roamers and a trainer, a talker and a scripted NPC", tr is not None and tr.id == 227 and [n.gen for n in tr.npcs].count("roam") == 3 and len(tr.npcs) == 6)
for nm in ("ruta_1", "test_roam"):
    m = world.by_name(nm)
    ar = cells(m)
    area = m.roam[2]
    bad = [(i, j) for i, j in ar if m.chars[j][i] not in area or m.cflags[j * m.w + i] & (mapc.C_SOLID | mapc.C_WATER | mapc.C_WARP | mapc.C_TRIGGER | mapc.C_LEDGE | mapc.C_GRASS)]
    check(f"{nm}: {len(ar)} area cells, all on the area chars '{area}' and none solid / grass / water / door / trigger / ledge ({len(bad)} bad)", ar and not bad)
# ---- @open_rate (contract 63) ---------------------------------------------------------------------------------------------------------------------------------------------------------------
m, rep = parse(roam="@open_rate 4\n@roam 1\n")
check(f"`@open_rate 4` compiles ({rep.errors}) and is kept", m is not None and not rep.errors and m.open_rate == 4)
if m:
    x = mapc.extras_bytes(m)
    check("the extras header is 5 bytes: enc_rate 25, n_enc 2, n_ledges 0, byte 3 (roam block length / 8) << 1, open_rate 4, then the slots", x[:3] == bytes([25, 2, 0]) and x[3] >> 1 > 0 and x[4] == 4 and x[5] == mapc.species_ids()["flitter"])
m, rep = parse(roam="")
check("no @open_rate = 0 (default) and the header byte 4 is 0", m is not None and m.open_rate == 0 and mapc.extras_bytes(m)[4] == 0)
m, rep = parse(roam="@open_rate 255\n")
check("@open_rate 255 is the largest value", m is not None and not rep.errors and m.open_rate == 255)
m, rep = parse(roam="@open_rate 0\n")
check("@open_rate 0 is allowed (= none)", m is not None and not rep.errors and m.open_rate == 0)
for what, d in (("256", "@open_rate 256\n"), ("-1", "@open_rate -1\n"), ("a word", "@open_rate lots\n"), ("no number", "@open_rate\n"), ("two numbers", "@open_rate 2 3\n")):
    m, rep = parse(roam=d)
    check(f"`{d.strip()}` ({what}) is refused", errs(rep, "@open_rate <0..255>"))
m, rep = parse(roam="@open_rate 2\n@open_rate 3\n")
check("two @open_rate are refused", errs(rep, "at most one @open_rate"))
m, rep = parse(roam="@open_rate 2\n", table="")
check("@open_rate without `@encounters grass` is refused", errs(rep, "@open_rate needs"))
m, rep = parse(roam="@open_rate 2\n", table="@encounters grass rate=25\n  flitter 3 6 60\n@end\n@encounters water rate=10\n  muncrit 3 6 40\n@end\n", grid=["BBBBBBBBBBBB", "B.f..gg.ww.B", "B....gg.ww.B", "B..x......rB", "B..........B", "BBBBBBBBBBBB"])
check("an open rate next to a water table compiles (the water keeps its own table)", m is not None and not rep.errors and m.open_rate == 2 and "water" in m.tables)

# the zone table of contract 63: every map with a grass table, its open-ground rate and its roamers
OPEN = {}
for n in ("ruta_1", "ruta_2", "ruta_22"):
    OPEN[n] = (2, 2)                                           # transit roads
for n in (3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 18, 23, 24, 25):
    OPEN[f"ruta_{n}"] = (4, 3)                                 # normal routes
OPEN["bosque_umbrio"] = (4, 2)                                 # the forest (a route, laid out like a cave)
for n in ("reserva_1", "reserva_2", "reserva_3", "reserva_4", "senda_1", "senda_2", "senda_3"):
    OPEN[n] = (8, 3)                                           # wild zones
for n in ("cueva_excavadora", "cueva_secreta_1", "cueva_secreta_2", "cueva_secreta_3", "monte_eco_1", "tunel_1", "tunel_2",
          "islas_escarcha_1", "islas_escarcha_2", "islas_escarcha_3", "central_electrica", "central_electrica_2"):
    OPEN[n] = (0, 2)                                           # caves: their floor rolls the cave table already; 2 roamers
OPEN["monte_eco_2"] = (0, 1)                                   # (one: with 2 the map would cost more heap than Ciudad Marea, contract 63.4)
for n in ("torre_3", "torre_4", "mansion_calcinada_1", "mansion_calcinada_2", "mansion_calcinada_3", "mansion_calcinada_4"):
    OPEN[n] = (0, 0)                                           # the Torre de los Recuerdos and the Mansion Calcinada: interiors
bad = []
for mp in world.maps:
    if mp.id >= mapc.TEST_MAP_BASE:
        continue
    want = OPEN.get(mp.name, (0, 0))
    got = (mp.open_rate, sum(1 for n in mp.npcs if n.gen == "roam"))
    if got != want:
        bad.append(f"{mp.name}: {got} != {want}")
check(f"every real map has the open rate and the roamers of the zone table ({len(OPEN)} maps with a table, the rest 0 / 0; {bad[:3]})", not bad)
SOLAR = mapc.affinity_sets()[0]
rs = lambda mp: [k for k in mp.roam_spr if k != mapc.ROAM_NONE]
check("every map of the table has a grass table, every species of it has a roam actor except the Solar forms of the affinity pairs (contract 66: ROAM_NONE) and at least one slot roams",
      all(world.by_name(n) is not None and world.by_name(n).enc and len(set(rs(world.by_name(n)))) == len(rs(world.by_name(n))) and rs(world.by_name(n)) and
          len(rs(world.by_name(n))) == sum(1 for e in world.by_name(n).enc if e[0] not in SOLAR) for n in OPEN if OPEN[n][1]))
check("towns, interiors, gyms, the Autopista, the bridges and piers have rate 0 and no roamer (no real map outside the table has either)",
      all(mp.open_rate == 0 and not mp.roam for mp in world.maps if mp.name not in OPEN and mp.id < mapc.TEST_MAP_BASE))
check("an interior map has open_rate 0", all(mp.open_rate == 0 for mp in world.maps if mp.interior))
check("every roam NPC list leaves free slots: <= 16 NPCs on every map", all(len(mp.npcs) <= 16 for mp in world.maps))
for n in ("monte_eco_1", "cueva_secreta_1", "senda_1", "bosque_umbrio"):
    mp = world.by_name(n)
    check(f"{n}: an explicit area of floor chars ('{mp.roam[2]}'), {len(cells(mp))} cells", mp.roam[2] and len(cells(mp)) >= 4 * mp.roam[0])
for mid in (3, 17, 227, 173):
    r = mapdump.parse_map_bin((ROOT / "nucleo" / "files" / f"map_{mid:02d}.bin").read_bytes())
    mp = [x for x in world.maps if x.id == mid][0]
    check(f"map {mid} ({mp.name}): the file's extras header byte 4 = {r['open_rate']} = the compiler's open_rate {mp.open_rate}", r["open_rate"] == mp.open_rate)

r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "mapdump.py"), "--verify"], capture_output=True, text=True, cwd=ROOT)
check("mapdump --verify round-trips every map (the roam block included)", r.returncode == 0 and "FAIL" not in r.stdout and "differs" not in r.stdout)
for mid in (3, 227):
    d = (ROOT / "nucleo" / "files" / f"map_{mid:02d}.bin").read_bytes()
    r = mapdump.parse_map_bin(d)
    m = world.by_name("ruta_1" if mid == 3 else "test_roam")
    check(f"map {mid}: the file's roam block decodes to the compiler's mask ({len(r['roam_mask'])} B) and its {len(r['roam_spr'])} actor indices", r["roam_mask"] == m.roam_mask and r["roam_spr"] == list(m.roam_spr))
    # the extras header: byte 3 = the C5 tail bit | (roam block length / 8) << 1; the block is padded to 8 and follows the ledges
    o = r["off"]["extras"]
    x3 = d[o + 3]
    blk = (len(m.roam_mask) + len(m.roam_spr) + 7) // 8
    check(f"map {mid}: extras byte 3 = {x3} = (block {blk * 8} B / 8) << 1 (no C5 tail)", x3 == blk << 1 and not m.tables)
    sprs = r["bank"]
    st = [bool(sprs[k]["flags"] & spritec.SPRF_STREAM) for k in m.roam_spr]
    hop = [bool(sprs[k]["flags"] & spritec.SPRF_HOP) for k in m.roam_spr]
    check(f"map {mid}: both roam actors are STREAMED from the card (stream = \"always\") and hop (flags {[sprs[k]['flags'] for k in m.roam_spr]})", all(st) and all(hop))
    check(f"map {mid}: 8 directions, 1 frame, 24..34 rows, 5 stored + mirrored", all(sprs[k]["ndirs"] == 8 and sprs[k]["nframes"] == 1 and 24 <= sprs[k]["h"] <= 34 for k in m.roam_spr))
    check(f"map {mid}: no roam actor is resident (the cost of a species is one 16 B directory entry)", not any(not st_ for st_ in st))
# the map without @roam is byte-identical in its extras
d = (ROOT / "nucleo" / "files" / "map_04.bin").read_bytes()
rr = mapdump.parse_map_bin(d)
check("a map without @roam has no roam block (extras byte 3 bit 1..7 clear, empty mask)", rr["roam_mask"] == b"" and rr["enc_rate"] >= 0)

# ---- the generated actors (tools/roam_ow.py) ------------------------------------------------------------------------------------------------------------------------------------------------
r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "roam_ow.py"), "check"], capture_output=True, text=True, cwd=ROOT)
check(f"`roam_ow.py check`: the art is newer than its pictures and assets/catalog_roam.toml is what `build` writes ({r.stdout.strip()[:120]})", r.returncode == 0)
cat = tomllib.load(open(ROOT / "assets" / "catalog_roam.toml", "rb"))
sp = tomllib.load(open(ROOT / "assets" / "data" / "species.toml", "rb"))
ent = {e["name"]: e for e in cat["sprite"]}
check("an actor per species of the @roam maps' tables: roam_flitter and roam_muncrit", {"roam_flitter", "roam_muncrit"} <= set(ent))
for n in ("flitter", "muncrit"):
    e = ent["roam_" + n]
    check(f"roam_{n}: 8-direction actor, 1 frame, mirror_west, hop, stream = always, adaptive conversion with the species' own dark cut ({sp[n].get('art_dark_cut', 70)})",
          e["kind"] == "actor" and e["ndirs"] == 8 and e["nframes"] == 1 and e["mirror_west"] and e["hop"] and e["stream"] == "always" and e["levels"] == "adaptive" and e["dark_cut"] == sp[n].get("art_dark_cut", 70))
sc = spritec.load_catalog(str(ROOT / "assets" / "catalog.toml"), extra=str(ROOT / "assets" / "catalog_roam.toml"))
for n in ("flitter", "muncrit"):
    s = sc.sprite("roam_" + n)
    check(f"roam_{n}: {s.w} x {s.h} px, {s.ndirs} directions of {s.nframes} frame, anchored on the feet (ay {s.ay})", 24 <= s.h <= 34 and s.ndirs == 8 and s.nframes == 1 and s.ay == s.h - 1)
    check(f"roam_{n}: the species' `height` {sp[n]['height']} m sets the size (24 rows at 0.3 m)", s.h == 24)
a, b = sc.sprite("roam_flitter"), sc.sprite("roam_muncrit")
check("the art files exist for the five stored facings of each species", all((ROOT / "assets" / "art" / "npcs" / f"{n}_roam" / f"{f}.png").is_file() for n in ("flitter", "muncrit") for f in ("south", "south-east", "east", "north-east", "north")))
check("a roam actor of a bank-sized sprite folds its west facings (mirror_west)", spritec.can_fold(a) and spritec.can_fold(b))
# ---- the affinity (contract 66): the Solar form of a pair never roams, a Lunar form in a table is an error -----------------------------------------------------------------------------------
SOL, LUN = mapc.affinity_sets()
sp_ids = mapc.species_ids()
check("affinity_sets: 10 wild pairs (the two final-stage counterparts are `wild = false`), Coilisp / Dunedil and the emblems among them",
      len(SOL) == len(LUN) == 10 and {sp_ids["coilisp"], sp_ids["scarabeam"]} <= SOL and {sp_ids["dunedil"], sp_ids["lunashell"]} <= LUN and not SOL & LUN)
T_MIXED = "@encounters grass rate=25\n  flitter 3 6 40\n  coilisp 3 6 40\n  muncrit 3 6 20\n@end\n"
m, rep = parse(table=T_MIXED)
check(f"a table with a Solar form compiles ({rep.errors})", m is not None and not rep.errors and m.roam is not None)
if m and m.roam:
    check("the Solar form's slot has no actor (ROAM_NONE = 255), the others have theirs; the bank has no roam_coilisp",
          m.roam_spr[1] == mapc.ROAM_NONE == 0xFF and m.roam_spr[0] != 0xFF and m.roam_spr[2] != 0xFF and not any(b == "roam_coilisp" for b in m.bank))
    check("the roamers' NpcDefs use the first slot that roams", all(n.name == "roam_flitter" for n in m.npcs if n.gen == "roam"))
m, rep = parse(table="@encounters grass rate=25\n  coilisp 3 6 60\n  mossbud 3 6 40\n@end\n")
check("a table of Solar forms only cannot have roamers: an error that says so", errs(rep, "affinity-exclusive"))
m, rep = parse(table="@encounters grass rate=25\n  flitter 3 6 60\n  dunedil 3 6 40\n@end\n", roam="")
check("a Lunar form in a wild table is an error: list its Solar counterpart", errs(rep, "LUNAR form") and errs(rep, "dunedil"))
m, rep = parse(table="@encounters grass rate=25\n  flitter 3 6 60\n  lunashell 3 6 40\n@end\n", roam="")
check("... also for the Lunar emblem", errs(rep, "LUNAR form"))
r = subprocess.run([sys.executable, "-I", str(ROOT / "tools" / "roam_ow.py"), "check"], capture_output=True, text=True, cwd=ROOT)
cat2 = tomllib.load(open(ROOT / "assets" / "catalog_roam.toml", "rb"))
have = {e["name"] for e in cat2["sprite"]}
check("roam_ow.py builds no actor for a Solar form (no roam_coilisp / roam_mossbud / roam_razorfly ...), and no stale folder is left for them",
      r.returncode == 0 and not any("roam_" + n in have for n in ("coilisp", "mossbud", "razorfly", "ramplet", "cindermeer", "hoodcoil")) and
      not any((ROOT / "assets" / "art" / "npcs" / f"{n}_roam").exists() for n in ("coilisp", "mossbud", "razorfly", "ramplet", "cindermeer", "hoodcoil", "dunedil", "droselet", "pitlion", "glintail")))
bad_real = []
for mp in world.maps:
    if mp.id >= mapc.TEST_MAP_BASE:
        continue
    for e in mp.enc:
        if e[0] in LUN:
            bad_real.append(mp.name)
    if mp.roam:
        nr = [k for k in mp.roam_spr if k != mapc.ROAM_NONE]
        if not nr or len(nr) != sum(1 for e in mp.enc if e[0] not in SOL):
            bad_real.append(mp.name + " roam")
check(f"every real map: no Lunar form in a wild table, and every roam map keeps >= 1 roaming slot ({bad_real[:3]})", not bad_real)
wt = [mp.name for mp in world.maps if mp.id < mapc.TEST_MAP_BASE and any(e[0] == sp_ids["scarabeam"] for e in mp.enc)]
check(f"the emblem Scarabeam (Solar) is in exactly one wild zone: Ruta 11 ({wt}); a Lunar player meets Lunashell in its slot", wt == ["ruta_11"])
print(f"mapc_roam_test: {fails} failure(s)")
sys.exit(1 if fails else 0)
