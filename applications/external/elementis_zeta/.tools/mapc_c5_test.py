#!/usr/bin/env python3
"""Test of mapc's C5 world-system syntax (contract 42): the water / fishing wild tables (`@encounters water|fish_old|fish_good|fish_super`, the extras tail), the legend attributes `boulder=`, `hole=`,
`sleeper=` + `level=`, `solid=yes` (invisible walls), the catalog's `slide_bike`, the builtin flags `badge_toxina` / `on_surf`, `@surf yes` (reachability over water) and tools/mkbike.py's surfing set.
  python3 -I tools/mapc_c5_test.py        (exit status 1 on a failure; part of tools/build_assets.sh)"""
import os
import struct
import subprocess
import sys
import tempfile
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


HEAD = """@map t
@size 6 3
@interior no
@bg 0
@fill dirt_a
@spawn start {spawn} S
"""
TILES = "  .  tile=dirt_a\n  ~  tile=sea_o\n"


def parse(
    extra_head="", legend="", grid=("......", "......", "......"), tail="", spawn="5 2"
):
    """-> (Map or None, Report) of a tiny map."""
    src = (
        HEAD.format(spawn=spawn)
        + extra_head
        + "@legend\n"
        + TILES
        + legend
        + "@grid\n"
        + "".join(f"  {r}\n" for r in grid)
        + tail
    )
    with tempfile.NamedTemporaryFile(
        "w", suffix=".map", delete=False, dir=tempfile.gettempdir()
    ) as f:
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


# ---- the wild tables -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
m, rep = parse(
    "@encounters water rate=10\n  flitter 5 7 3\n@end\n@encounters fish_old\n  muncrit 5 5 1\n@end\n@encounters fish_good\n  flitter 10 12 3\n  muncrit 10 10 1\n@end\n@encounters fish_super rate=200\n  flitter 20 25 1\n@end\n",
    grid=("......", ".~~~..", "......"),
    legend="",
)
check(
    f"the four tables parse ({rep.errors})",
    m is not None and not rep.errors and set(m.tables) == set(mapc.WILD_TABLES),
)
if m:
    check(
        "water: rate 10, one slot",
        m.tables["water"][0] == 10 and len(m.tables["water"][1]) == 1,
    )
    check(
        "fish_old defaults to 255 (always), fish_good to 128 (about half), fish_super to its own rate=200",
        (m.tables["fish_old"][0], m.tables["fish_good"][0], m.tables["fish_super"][0])
        == (255, 128, 200),
    )
    x = mapc.extras_bytes(m)
    check(
        "extras header: enc_rate 0, n_enc 0, n_ledges 0, has_tail 1, open_rate 0 (contract 63: 5 bytes)",
        x[:5] == bytes([0, 0, 0, 1, 0]),
    )
    check(
        "tail: rate[4] = 10 255 128 200, n[4] = 1 1 2 1",
        x[5:13] == bytes([10, 255, 128, 200, 1, 1, 2, 1]),
    )
    check(
        "then the slots in table order (water first)",
        x[13:17] == bytes([mapc.species_ids()["flitter"], 5, 7, 3])
        and len(x) == 13 + 4 * 5,
    )
    # the same bytes through the C reader's layout: map_table(m, k) = tail + 8 + 4 * (sum of the n before k)
    t = x[5:]
    rates, ns = t[:4], t[4:8]
    off = lambda k: 8 + 4 * sum(ns[:k])
    check(
        "map_table offsets: fish_good starts after water + fish_old (2 slots)",
        off(2) == 8 + 8 and t[off(2) : off(2) + 4][1] == 10,
    )
m, rep = parse(
    "@encounters grass rate=30\n  flitter 2 5 1\n@end\n@encounters water rate=10\n  flitter 5 7 3\n@end\n",
    grid=("......", ".~~~..", "......"),
)
check(
    "a grass table and a water table can live together",
    m is not None and not rep.errors and m.enc_rate == 30 and "water" in m.tables,
)
m, rep = parse("@encounters water\n  flitter 5 7 3\n@end\n")
check("water without rate= is refused", errs(rep, "@encounters"))
m, rep = parse("@encounters fish_old rate=0\n  muncrit 5 5 1\n@end\n")
check("rate=0 is refused", errs(rep, "@encounters"))
m, rep = parse(
    "@encounters fish_old\n  muncrit 5 5 1\n@end\n@encounters fish_old\n  muncrit 6 6 1\n@end\n"
)
check("a second fish_old table is refused", errs(rep, "second @encounters fish_old"))
m, rep = parse("@encounters fishing\n  muncrit 5 5 1\n@end\n")
check("an unknown kind is refused", errs(rep, "@encounters"))
m, rep = parse("@encounters fish_super\n@end\n")
check("an empty table is refused", errs(rep, "has no slots"))
m, rep = parse(
    "@encounters fish_super\n  muncrit 5 5 1\n@end\n",
    grid=("......", "......", "......"),
)
check(
    "a fishing table on a map without water warns",
    m is not None and any("no water tile" in w for w in rep.warnings),
)
m, rep = parse(
    "@encounters fish_super\n  muncrit 5 5 1\n  muncrit 5 5 0\n@end\n",
    grid=("......", ".~~...", "......"),
)
check("a slot with weight 0 is refused", errs(rep, "weight"))
slots = "".join(f"  muncrit {5 + k % 3} 9 1\n" for k in range(17))
m, rep = parse(
    "@encounters fish_super\n" + slots + "@end\n", grid=("......", ".~~...", "......")
)
check("more than 16 slots are refused", errs(rep, "more than 16"))

# ---- boulders and holes ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
m, rep = parse(
    "@flag pit 790\n",
    legend="  b  tile=dirt_a boulder=yes\n  h  tile=dirt_a hole=pit\n  H  tile=dirt_a hole=791\n",
    grid=("b.h.H.", "......", "......"),
)
check(
    f"boulder=yes and hole=FLAG compile ({rep.errors})",
    m is not None and not rep.errors and len(m.npcs) == 3,
)
if m and len(m.npcs) == 3:
    b, h1, h2 = m.npcs
    check(
        "the boulder: actor boulder_rock, behavior 4, solid, no text, no script",
        b.name == mapc.BOULDER_SPRITE
        and b.behavior == mapc.NPCB_BOULDER
        and b.flags & 1
        and not b.script
        and not b.text_key,
    )
    check(
        "the pit: actor hole_pit, behavior 5, not solid",
        h1.name == mapc.HOLE_SPRITE
        and h1.behavior == mapc.NPCB_HOLE
        and not h1.flags & 1,
    )
    check(
        "hole=pit resolves the @flag name (790) and hole=791 a number: they are the pits' hide flags",
        h1.hide_flag == 790 and h2.hide_flag == 791,
    )
G1 = ("b.....", "......", "......")
m, rep = parse(legend="  b  tile=dirt_a boulder=yes text=x.y\n", grid=G1)
check("boulder with text= is refused", errs(rep, "generates the NPC"))
m, rep = parse(legend="  b  tile=dirt_a boulder=yes npc=vecino\n", grid=G1)
check("boulder with npc= is refused", errs(rep, "take no npc="))
m, rep = parse(legend="  b  tile=dirt_a boulder=yes hide_flag=5\n", grid=G1)
check(
    "boulder with hide_flag= is refused (boulders reset on reload)",
    errs(rep, "generates the NPC"),
)
m, rep = parse(
    legend="  h  tile=dirt_a hole=nosuchflag\n", grid=("h.....", "......", "......")
)
check("hole= with an unknown flag name is refused", errs(rep, "hole= needs a flag"))
m, rep = parse(legend="  h  tile=dirt_a hole=0\n", grid=("h.....", "......", "......"))
check("hole=0 is refused", errs(rep, "hole= needs a flag"))
m, rep = parse(
    legend="  b  tile=dirt_a boulder=nosuchactor\n", grid=("b.....", "......", "......")
)
check("an unknown boulder actor is refused", errs(rep, "unknown actor"))
m, rep = parse(
    legend="  w  tile=dirt_a hole=790\n",
    grid=("w.....", "......", "......"),
    tail="@warp w -> t spawn=start\n",
)
check(
    f"a pit may be a warp (a hole you fall through) ({rep.errors})",
    m is not None and not errs(rep, "NPC placed on a solid/warp"),
)
m, rep = parse(
    legend="  b  tile=dirt_a boulder=yes\n  w  tile=dirt_a\n",
    grid=("b.....", "......", "......"),
)
check(
    "a boulder is a gate for the reachability checks (the cell behind it is reachable)",
    m is not None and not m.solid_at(0, 0) and not rep.errors,
)

# ---- the sleeper -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
m, rep = parse(
    legend="  Z  tile=dirt_a sleeper=muncrit level=30 npc=vecino face=SE\n",
    grid=("Z.....", "......", "......"),
)
check(
    f"sleeper=SPECIES level=N npc=ACTOR compiles ({rep.errors})",
    m is not None
    and not rep.errors
    and len(m.npcs) == 1
    and m.npcs[0].script == "sleeper_0",
)
if m and m.npcs:
    sc = next(s for s in m.scripts if s.name == "sleeper_0")
    ops = [(i.op, i.args[0]) if i.op == "call_native" else (i.op,) for i in sc.ins]
    check(
        "the script: has_item (native 14), jv, say, end, ask, jv, end, sfx (native 23), say, wild (natives 9 + 10), setf",
        ops
        == [
            ("call_native", "14"),
            ("jv",),
            ("say",),
            ("end",),
            ("ask",),
            ("jv",),
            ("end",),
            ("call_native", "23"),
            ("say",),
            ("call_native", "9"),
            ("call_native", "10"),
            ("setf",),
        ],
    )
    check(
        "it wakes the species at the level of the legend (wild_set 8 30: muncrit, 30)",
        [i.args for i in sc.ins if i.op == "call_native" and i.args[0] == "9"]
        == [["9", str(mapc.species_ids()["muncrit"]), "30"]],
    )
    check(
        "it asks `gl.c5a.sleep.ask`, says `gl.c5a.sleep.look` / `.woke`",
        [i.args[0] for i in sc.ins if i.op in ("say", "ask")]
        == ["gl.c5a.sleep.look", "gl.c5a.sleep.ask", "gl.c5a.sleep.woke"],
    )
    check(
        "the NPC is solid and gets an automatic hide flag",
        m.npcs[0].flags & 1 and m.npcs[0].flag_auto and m.npcs[0].gen == "sleeper",
    )
    check("it is a gate for the reachability checks", not m.solid_at(0, 0))
m, rep = parse(
    legend="  Z  tile=dirt_a sleeper=muncrit npc=vecino\n",
    grid=("Z.....", "......", "......"),
)
check("sleeper= without level= is refused", errs(rep, "level="))
m, rep = parse(
    legend="  Z  tile=dirt_a sleeper=muncrit level=101 npc=vecino\n",
    grid=("Z.....", "......", "......"),
)
check("level 101 is refused", errs(rep, "level="))
m, rep = parse(
    legend="  Z  tile=dirt_a sleeper=nosuchmon level=5 npc=vecino\n",
    grid=("Z.....", "......", "......"),
)
check("an unknown species is refused", errs(rep, "unknown species"))
m, rep = parse(
    legend="  Z  tile=dirt_a sleeper=muncrit level=5\n",
    grid=("Z.....", "......", "......"),
)
check("sleeper= without npc= is refused", errs(rep, "need npc="))
m, rep = parse(
    legend="  Z  tile=dirt_a sleeper=muncrit level=5 npc=vecino text=a.b\n",
    grid=("Z.....", "......", "......"),
)
check("sleeper= with text= is refused", errs(rep, "generates the NPC"))
m, rep = parse(
    "@flag woke 791\n",
    legend="  Z  tile=dirt_a sleeper=muncrit level=5 npc=vecino hide_flag=woke\n",
    grid=("Z.....", "......", "......"),
)
check(
    f"an explicit hide_flag= (an @flag name) is the sleeper's flag ({rep.errors})",
    m is not None
    and not rep.errors
    and m.npcs[0].hide_flag == 791
    and not m.npcs[0].flag_auto,
)
m, rep = parse(
    legend="  t  tile=dirt_a npc=vecino text=a.a level=3\n",
    grid=("t.....", "......", "......"),
)
check("level= without sleeper= is refused", errs(rep, "level= needs sleeper="))

# ---- invisible walls, water under solid objects, slopes, builtin flags ---------------------------------------------------------------------------------------------------------------------
m, rep = parse(
    legend="  W  tile=dirt_a solid=yes\n", grid=("..W...", "......", "......")
)
check(
    f"`solid=yes` on a plain floor entry makes the cell solid with the same tile ({rep.errors})",
    m is not None
    and m.cflags[2] & mapc.C_SOLID
    and m.ground[2] == m.ground[1]
    and m.cflags[1] == 0,
)
m, rep = parse(
    legend="  W  tile=dirt_a solid=yes interact=step script=s\n",
    grid=("..W...", "......", "......"),
    tail="@script s step\n    end\n@end\n",
)
check(
    "a step trigger on an invisible wall is refused (a solid tile)",
    errs(rep, "solid tile"),
)
m, rep = parse(
    legend="  r  tile=sea_o object=boulder\n", grid=("~~r~~~", "......", "......")
)
check(
    "a solid object on a water tile strips the water flag (not surfable); the plain water keeps it",
    m is not None
    and m.cflags[2] & mapc.C_SOLID
    and not m.cflags[2] & mapc.C_WATER
    and m.cflags[1] & mapc.C_WATER,
)
m, rep = parse(
    legend="  s  tile=slope_s\n  a  tile=arrow_s\n", grid=("s.a...", "......", "......")
)
check(
    f"`slide_bike` tiles: C_SLIDE, kept out of the arrow-floor model; `slide` ones are in it ({rep.errors})",
    m is not None
    and m.cflags[0] & mapc.C_SLIDE
    and (0, 0) in m.bike_slides
    and (0, 0) not in m.slides
    and (2, 0) in m.slides
    and m.bike_slides[(0, 0)] == 0,
)
check(
    "builtin flags: badge_toxina = 84, on_surf = 1020 (FLAG_SURF), mod_hidro = 90, mod_empuje = 91",
    mapc.BUILTIN_FLAGS["badge_toxina"] == 84
    and mapc.BUILTIN_FLAGS["on_surf"] == 1020
    and mapc.BUILTIN_FLAGS["mod_hidro"] == 90
    and mapc.BUILTIN_FLAGS["mod_empuje"] == 91,
)
src = (ROOT / "nucleo/world.h").read_text()
check(
    "world.h agrees: FLAG_SURF 1020, FLAG_BADGE_TOXINA 84",
    "#define FLAG_SURF 1020" in src and "#define FLAG_BADGE_TOXINA 84" in src,
)

# ---- the C side reads the same tail: the binary of the test fixture ------------------------------------------------------------------------------------------------------------------------------------
f = ROOT / "nucleo/files/map_220.bin"
if f.is_file():
    d = f.read_bytes()
    x4 = struct.unpack_from("<H", d, 62)[0] * 4
    hdr = d[x4 : x4 + 4]
    check(
        "map_220.bin (test_c5sys): the extras header says has_tail = 1",
        hdr[3] == 1 and hdr[0] == 0 and hdr[1] == 0,
    )
    rates, ns = d[x4 + 5 : x4 + 9], d[x4 + 9 : x4 + 13]
    check(
        "map_220.bin: rates 255 255 128 255, slots 1 1 2 2",
        list(rates) == [255, 255, 128, 255] and list(ns) == [1, 1, 2, 2],
    )

# ---- @surf yes: the reachability checks may cross the water ----------------------------------------------------------------------------------------------------------------------------------------
grid = ("..~~..", "..~~..", "..~~..")
legend = "  T  tile=dirt_a interact=look text=x.island\n"
island = ("..~~T.", "..~~..", "..~~..")
m, rep = parse(legend=legend, grid=island, spawn="0 0")
if m:
    mapc.check_reach(m, rep)
check(
    "without @surf a sign beyond the lake cannot be reached (an error)",
    errs(rep, "cannot be reached/faced"),
)
m, rep = parse("@surf yes\n", legend=legend, grid=island, spawn="0 0")
if m:
    mapc.check_reach(m, rep)
check(
    f"with `@surf yes` the same map is fine ({rep.errors})",
    m is not None and not errs(rep, "cannot be reached/faced") and m.surf,
)
m, rep = parse(
    "@surf yes\n",
    legend="  T  tile=dirt_a interact=look text=x.island\n  r  tile=sea_o object=boulder\n",
    grid=("..r~T.", "..~~..", "..r~.."),
    spawn="0 0",
)
if m:
    mapc.check_reach(m, rep)
check(
    "a rock row blocks nothing as long as one water column is open",
    m is not None and not errs(rep, "cannot be reached/faced"),
)
m, rep = parse(
    "@surf yes\n",
    legend="  T  tile=dirt_a interact=look text=x.island\n  r  tile=sea_o object=boulder\n",
    grid=("..rrT.", "..rr..", "..rr.."),
    spawn="0 0",
)
if m:
    mapc.check_reach(m, rep)
check(
    "a wall of rocks across the water is still a wall (the sign stays unreachable)",
    errs(rep, "cannot be reached/faced"),
)

# ---- mkbike: the surfing set is built from assets/art/hero_surf-shaped art, hero.bin's format ------------------------------------------------------------------------------------------------------------
with tempfile.TemporaryDirectory() as td:
    out = Path(td) / "hero_surf.bin"
    r = subprocess.run(
        [
            sys.executable,
            "-I",
            str(ROOT / "tools/mkbike.py"),
            "surf",
            str(ROOT / "assets/art/hero_bike"),
            str(out),
        ],
        capture_output=True,
        text=True,
    )
    check(
        f"`mkbike.py surf SRC OUT` builds a file from an art folder ({r.stderr.strip()[:80]})",
        r.returncode == 0 and out.is_file(),
    )
    if out.is_file():
        b, h = out.read_bytes(), (ROOT / "nucleo/files/hero.bin").read_bytes()
        check(
            "hero_surf.bin has hero.bin's size and 16-byte header (NHRO, 24 x 35, 5 frames, 5 facings)",
            len(b) == len(h) and b[:16] == h[:16],
        )
        same = (ROOT / "nucleo/files/hero_bike.bin").read_bytes()
        check(
            "built from the bicycle's art folder it equals hero_bike.bin (the same pipeline, deterministic)",
            b == same,
        )
    r = subprocess.run(
        [sys.executable, "-I", str(ROOT / "tools/mkbike.py"), "check"],
        capture_output=True,
        text=True,
    )
    check(
        f"`mkbike.py check` passes without a surfing set ({r.stdout.strip()[-60:]})",
        r.returncode == 0,
    )

print(f"mapc_c5_test: {'FAILED ' + str(fails) if fails else 'all ok'}")
sys.exit(1 if fails else 0)
