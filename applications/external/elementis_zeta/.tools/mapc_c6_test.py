#!/usr/bin/env python3
"""Test of mapc's C6 world-system syntax (contract 46): `@fly` (the Planeador's towns: slot, landing tile, the header byte, the generated table), the legend `keydoor=<item>` (a card-key door),
`gift=<species> level=N hide_flag=<shared flag>` (the gift choice), `pad=yes` + `@pad` (teleport pads: pairing, exits, the reachability edges, across maps), the `give_box` macro. The tiny cases parse one
map; the world cases copy maps/ to a temporary root, change a line of the fixtures (maps/test_c6sys.map, test_c6sys_b.map, a town) and run the whole load + link + validate.
  python3 -I tools/mapc_c6_test.py        (exit status 1 on a failure; part of tools/build_assets.sh)"""
import os
import re
import shutil
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import mapc  # noqa: E402
import mapdump  # noqa: E402

fails = 0


def check(what, cond):
    global fails
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        fails += 1


HEAD = """@map t
@size 8 4
@interior no
@bg 0
@fill dirt_a
@spawn start {spawn} S
"""
TILES = "  .  tile=dirt_a\n  P  tile=pad pad=yes\n"


def parse(
    extra_head="",
    legend="",
    grid=("........", "........", "........", "........"),
    tail="",
    spawn="7 3",
):
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


def warns(rep, needle):
    return any(needle in e for e in rep.warnings)


# ---- the card-key door ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
m, rep = parse(
    legend="  K  tile=dirt_a keydoor=chip_acceso\n",
    grid=("K.......", "........", "........", "........"),
)
check(
    f"keydoor=ITEM compiles ({rep.errors})",
    m is not None
    and not rep.errors
    and len(m.npcs) == 1
    and m.npcs[0].script == "keydoor_0",
)
if m and m.npcs:
    n = m.npcs[0]
    sc = next(s for s in m.scripts if s.name == "keydoor_0")
    ops = [(i.op, i.args[0]) if i.op == "call_native" else (i.op,) for i in sc.ins]
    check(
        "default actor `keydoor`, solid, a script NPC, an automatic hide flag",
        n.name == mapc.KEYDOOR_SPRITE
        and n.flags & 1
        and n.flags & mapc.NPCF_SCRIPT
        and n.flag_auto
        and n.gen == "keydoor",
    )
    check(
        "the script: has_item (14), jv, item_var (20), say, end, open:, item_var, sfx (23), setf, say",
        ops
        == [
            ("call_native", "14"),
            ("jv",),
            ("call_native", "20"),
            ("say",),
            ("end",),
            ("call_native", "20"),
            ("call_native", "23"),
            ("setf",),
            ("say",),
        ],
    )
    check(
        "it says `gl.c6.door.locked` without the key and `gl.c6.door.open` with it",
        [i.args[0] for i in sc.ins if i.op == "say"]
        == ["gl.c6.door.locked", "gl.c6.door.open"],
    )
    check("it is a gate for the reachability checks", not m.solid_at(0, 0))
    check(
        "the item operand is the Access Chip's id",
        [i.args for i in sc.ins if i.op == "call_native" and i.args[0] == "14"][0][1]
        == str(mapc.parse_item("chip_acceso")),
    )
    check("the sound is the `door` effect", mapc.KEYDOOR_FX == "door")
m, rep = parse(
    legend="  K  tile=dirt_a keydoor=nosuchitem\n",
    grid=("K.......", "........", "........", "........"),
)
check("an unknown item is refused", errs(rep, "unknown item"))
m, rep = parse(
    legend="  K  tile=dirt_a keydoor=chip_acceso text=a.b\n",
    grid=("K.......", "........", "........", "........"),
)
check("keydoor= with text= is refused", errs(rep, "generates the NPC"))
m, rep = parse(
    legend="  K  tile=dirt_a keydoor=chip_acceso show_flag=5\n",
    grid=("K.......", "........", "........", "........"),
)
check("keydoor= with show_flag= is refused", errs(rep, "generates the NPC"))
m, rep = parse(
    "@flag opened 801\n",
    legend="  K  tile=dirt_a keydoor=chip_acceso hide_flag=opened\n",
    grid=("K.......", "........", "........", "........"),
)
check(
    f"an explicit hide_flag= is the door's flag ({rep.errors})",
    m is not None
    and not rep.errors
    and m.npcs[0].hide_flag == 801
    and not m.npcs[0].flag_auto,
)
m, rep = parse(
    legend="  K  tile=dirt_a keydoor=chip_acceso npc=nosuchactor\n",
    grid=("K.......", "........", "........", "........"),
)
check("an unknown actor is refused", errs(rep, "unknown actor"))
m, rep = parse(
    legend="  K  tile=dirt_a keydoor=chip_acceso npc=boulder_rock\n",
    grid=("K.......", "........", "........", "........"),
)
check(
    "npc= overrides the art (another actor)",
    m is not None and not rep.errors and m.npcs[0].name == "boulder_rock",
)
m, rep = parse(
    legend="  K  tile=dirt_a keydoor=chip_acceso script=x\n",
    grid=("K.......", "........", "........", "........"),
)
check("keydoor= with script= is refused", errs(rep, "generates the NPC"))

# ---- the gift choice --------------------------------------------------------------------------------------------------------------------------------------------------------------------------
G = "@flag taken 1005\n"
LG = "  a  tile=dirt_a gift=embercub level=12 hide_flag=taken\n  b  tile=dirt_a gift=puddlet level=14 hide_flag=taken\n"
m, rep = parse(G, legend=LG, grid=("a.b.....", "........", "........", "........"))
check(
    f"two gifts sharing one hide flag compile ({rep.errors})",
    m is not None
    and not rep.errors
    and len(m.npcs) == 2
    and all(n.hide_flag == 1005 and n.gen == "gift" for n in m.npcs),
)
if m and len(m.npcs) == 2:
    sc = next(s for s in m.scripts if s.name == "gift_0")
    ops = [(i.op, i.args[0]) if i.op == "call_native" else (i.op,) for i in sc.ins]
    check(
        "default actor item_bag, solid",
        m.npcs[0].name == mapc.ITEM_SPRITE and m.npcs[0].flags & 1,
    )
    check(
        "the script: setv, name_var (7), mon_show (8), ask, jv, end, give (3), jv, setf, sfx, say, end, say",
        ops
        == [
            ("setv",),
            ("call_native", "7"),
            ("call_native", "8"),
            ("ask",),
            ("jv",),
            ("end",),
            ("call_native", "3"),
            ("jv",),
            ("setf",),
            ("call_native", "23"),
            ("say",),
            ("end",),
            ("say",),
        ],
    )
    gv = [i.args for i in sc.ins if i.op == "call_native" and i.args[0] == "3"][0]
    check(
        "give: the species and the level | 128 (the box flag of native 3)",
        gv[1:] == [str(mapc.species_ids()["embercub"]), str(12 + 128)],
    )
    check(
        "both NPCs set the SAME flag; the two scripts differ in the species only",
        [
            i.args
            for i in next(s for s in m.scripts if s.name == "gift_1").ins
            if i.op == "setf"
        ]
        == [i.args for i in sc.ins if i.op == "setf"],
    )
    check(
        "says `gl.c6.gift.ask` / `.got` / `.fail`",
        [i.args[0] for i in sc.ins if i.op in ("say", "ask")]
        == ["gl.c6.gift.ask", "gl.c6.gift.got", "gl.c6.gift.fail"],
    )
m, rep = parse(
    legend="  a  tile=dirt_a gift=embercub hide_flag=1005\n",
    grid=("a.......", "........", "........", "........"),
)
check("gift= without level= is refused", errs(rep, "gift= needs level"))
m, rep = parse(
    legend="  a  tile=dirt_a gift=embercub level=0\n",
    grid=("a.......", "........", "........", "........"),
)
check("level 0 is refused", errs(rep, "gift= needs level"))
m, rep = parse(
    legend="  a  tile=dirt_a gift=nosuchmon level=5\n",
    grid=("a.......", "........", "........", "........"),
)
check("an unknown species is refused", errs(rep, "unknown species"))
m, rep = parse(
    legend="  a  tile=dirt_a gift=embercub level=5 text=a.b\n",
    grid=("a.......", "........", "........", "........"),
)
check("gift= with text= is refused", errs(rep, "generates the NPC"))
m, rep = parse(
    legend="  a  tile=dirt_a gift=embercub level=5 npc=crystal_fire\n",
    grid=("a.......", "........", "........", "........"),
)
check(
    f"npc= picks the art (a crystal) ({rep.errors})",
    m is not None and not rep.errors and m.npcs[0].name == "crystal_fire",
)
m, rep = parse(
    legend="  a  tile=dirt_a gift=embercub level=5\n",
    grid=("a.......", "........", "........", "........"),
)
check(
    "without hide_flag= the flag is automatic (a single gift)",
    m is not None and m.npcs[0].flag_auto,
)

# ---- the give_box macro ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
m, rep = parse(
    legend="  a  tile=dirt_a npc=vecino script=s\n",
    grid=("a.......", "........", "........", "........"),
    tail="@script s npc\n    give_box embercub 20\n    give embercub 5\n    end\n@end\n",
)
sc = next((s for s in m.scripts if s.name == "s"), None) if m else None
check(f"give_box compiles ({rep.errors})", sc is not None and not rep.errors)
if sc:
    check(
        "give_box SP LV = native 3 SP (LV + 128); a plain give keeps its level",
        [i.args for i in sc.ins if i.op == "call_native"]
        == [["3", "embercub", "148"], ["3", "embercub", "5"]]
        or [i.args[2] for i in sc.ins if i.op == "call_native"] == ["148", "5"],
    )
m, rep = parse(
    legend="  a  tile=dirt_a npc=vecino script=s\n",
    grid=("a.......", "........", "........", "........"),
    tail="@script s npc\n    give_box embercub 101\n    end\n@end\n",
)
check("give_box with level 101 is refused", errs(rep, "give_box"))
m, rep = parse(
    legend="  a  tile=dirt_a npc=vecino script=s\n",
    grid=("a.......", "........", "........", "........"),
    tail="@script s npc\n    give_box embercub\n    end\n@end\n",
)
check("give_box without a level is refused", errs(rep, "give_box"))

# ---- pads: the syntax of one map ----------------------------------------------------------------------------------------------------------------------------------------------------------------
m, rep = parse(
    grid=("P......P", "........", "........", "........"),
    tail="@pad 0 0 -> 7 0 face=S\n@pad 7 0 -> 0 0 face=SE\n",
)
check(
    f"two pads that lead to each other compile ({rep.errors})",
    m is not None and not rep.errors and len(m.warps) == 2,
)
if m and len(m.warps) == 2:
    w0, w1 = m.warps
    check(
        "both are warps (C_WARP) with any arrival facing, the fade, target = the other pad cell",
        all(m.cflags[w.j * m.w + w.i] & mapc.C_WARP for w in m.warps)
        and w0.req_dirs == 0xFF
        and w0.flags == 0
        and w0.pad == (7, 0, 0)
        and w1.pad == (0, 0, 1),
    )
    check(
        "a pad is a plain floor tile: no solid, the animated `pad` tile",
        not m.cflags[0] & mapc.C_SOLID,
    )
m, rep = parse(
    grid=("P......P", "........", "........", "........"), tail="@pad 0 0 -> 7 0\n"
)
check("a pad=yes cell without its @pad line is refused", errs(rep, "has no @pad line"))
m, rep = parse(
    grid=("P.......", "........", "........", "........"),
    tail="@pad 3 3 -> 0 0\n@pad 0 0 -> 3 3\n",
)
check(
    "a @pad line on a cell that is not pad=yes is refused",
    errs(rep, "not a legend entry with pad=yes"),
)
m, rep = parse(
    grid=("P......P", "........", "........", "........"),
    tail="@pad 0 0 -> 7 0\n@pad 0 0 -> 7 0\n@pad 7 0 -> 0 0\n",
)
check("a pad declared twice is refused", errs(rep, "declared twice"))
m, rep = parse(
    grid=("P.......", "........", "........", "........"), tail="@pad 0 0 ->\n"
)
check("a malformed @pad is refused", errs(rep, "@pad <i> <j>"))
m, rep = parse(
    legend="  X  tile=dirt_a solid=yes pad=yes\n",
    grid=("X.......", "........", "........", "........"),
)
check(
    "pad=yes with solid= (or an object, a door, an NPC ...) is refused",
    errs(rep, "plain floor tile"),
)
m, rep = parse(
    legend="  X  tile=dirt_a pad=maybe\n",
    grid=("X.......", "........", "........", "........"),
)
check("pad=<other than yes> is refused", errs(rep, "pad= must be yes"))
m, rep = parse(
    grid=("P.......", "........", "........", "........"),
    tail="@pad 0 0 -> t 0 0 face=XX\n",
)
check("a bad facing is refused", errs(rep, "@pad <i> <j>"))

# ---- @fly: the syntax of one map ---------------------------------------------------------------------------------------------------------------------------------------------------------------
m, rep = parse("@fly 3 villa_aliso 1 1 face=SE\n")
check(
    f"@fly SLOT KEY I J face=DIR compiles ({rep.errors})",
    m is not None and not rep.errors and m.fly[:5] == (3, "villa_aliso", 1, 1, 1),
)
m, rep = parse("@fly 3 villa_aliso 1 1\n")
check("the facing defaults to S", m is not None and m.fly[4] == 0)
m, rep = parse("@fly 11 villa_aliso 1 1\n")
check(
    "slot 11 is refused (0..10: its flag 995 is D1's gruta_pit, contract 72)",
    errs(rep, "@fly <slot"),
)
m, rep = parse("@fly 3 villa_aliso 1\n")
check("a missing coordinate is refused", errs(rep, "@fly <slot"))
m, rep = parse("@fly 3 villa_aliso 1 1\n@fly 4 other 2 2\n")
check("two @fly lines in a map are refused", errs(rep, "at most one @fly"))
m, rep = parse("@fly 3 villa_aliso 1 1 face=up\n")
check("a bad facing is refused", errs(rep, "@fly <slot"))


# ---- the whole world: link, reachability, the header byte, the generated table ----------------------------------------------------------------------------------------------------------------
def world(edit=None, edit_other=None):
    """Loads the real world from a temporary copy of maps/ (assets/ is linked); edit(text) changes maps/test_c6sys.map, edit_other = {name: fn(text)} other maps. -> (World, Report)."""
    td = Path(tempfile.mkdtemp())
    try:
        (td / "assets").symlink_to(ROOT / "assets")
        shutil.copytree(ROOT / "maps", td / "maps")
        if edit:
            p = td / "maps/test_c6sys.map"
            p.write_text(edit(p.read_text(encoding="utf-8")), encoding="utf-8")
        for nm, fn in (edit_other or {}).items():
            p = td / f"maps/{nm}.map"
            p.write_text(fn(p.read_text(encoding="utf-8")), encoding="utf-8")
        rep = mapc.Report()
        w = mapc.load_world(td, rep)
        return w, rep
    finally:
        shutil.rmtree(td, ignore_errors=True)


w, rep = world()
check(
    f"the real world (with the C6 fixtures) loads without errors ({rep.errors[:2]})",
    not rep.errors,
)
by = {m.name: m for m in w.maps}
check(
    "the towns carry their slots 0..8 (C5a: Ciudad Ambar is slot 7; C6: Ciudad Cenit is slot 8) and the header byte",
    [
        (by[n].fly[0] if by[n].fly else None)
        for n in (
            "villa_aliso",
            "ciudad_nodo",
            "pueblo_cantera",
            "ciudad_marea",
            "puerto_dinamo",
            "pueblo_bruma",
            "metropolis_neon",
            "ciudad_ambar",
            "ciudad_cenit",
        )
    ]
    == list(range(9)),
)
check(
    "the room of test_c6sys is reachable through the pads (its person is not an error, the room has no other way in)",
    not errs(rep, "cannot be reached")
    and any(n.i == 20 and n.j == 6 for n in by["test_c6sys"].npcs),
)
check(
    "the pad warps are resolved: A (4,4) -> (18,4) facing S in this map; E (4,12) -> test_c6sys_b (9,5)",
    any(
        (wp.i, wp.j, wp.target_i, wp.target_j, wp.target_facing, wp.target_map_id)
        == (4, 4, 18, 4, 0, by["test_c6sys"].id)
        for wp in by["test_c6sys"].warps
    )
    and any(
        (wp.i, wp.j, wp.target_i, wp.target_j, wp.target_map_id)
        == (4, 12, 9, 5, by["test_c6sys_b"].id)
        for wp in by["test_c6sys"].warps
    ),
)
check(
    "no warning about a missing way back for pads",
    not any("test_c6sys" in x and "no warp back" in x for x in rep.warnings),
)
bin_ = (
    mapc.build_map_bin(w, by["ciudad_nodo"]) if hasattr(mapc, "build_map_bin") else None
)
f = ROOT / "nucleo/files/map_04.bin"
if f.is_file():
    r = mapdump.parse_map_bin(f.read_bytes())
    check("map_04.bin (Ciudad Nodo): header byte 11 = 2 (slot 1)", r["fly"] == 2)
f = ROOT / "nucleo/files/map_05.bin"
if f.is_file():
    check(
        "map_05.bin (the Centro): header byte 11 = 0",
        mapdump.parse_map_bin(f.read_bytes())["fly"] == 0,
    )
h = mapc.emit_header_h(w)
check(
    "maps_gen.h text: FLY_N 11 (C5a: Ambar is slot 7; C6: Cenit slot 8; C9, contract 56: the Meseta Cumbre is slot 10; slot 9 is C7's) and a row per slot with the landing tile and the area text",
    "#define FLY_N 11" in h
    and "[10] = {MAP_MESETA_CUMBRE, 10, 4, 0, TXT_GL_AREA_MESETA_CUMBRE}" in h
    and "[1] = {MAP_CIUDAD_NODO, 4, 13, 0, TXT_GL_AREA_CIUDAD_NODO}" in h
    and "[6] = {MAP_METROPOLIS_NEON, 26, 8, 0, TXT_GL_AREA_METROPOLIS_NEON}" in h,
)
src = (ROOT / "nucleo/world.h").read_text()
check(
    "world.h agrees: FLY_FLAG0 984, 11 slots (contract 72), the Voltio badge 82",
    "#define FLY_FLAG0 984" in src
    and "#define FLY_SLOTS 11" in src
    and "#define FLAG_BADGE_VOLTIO 82" in src,
)
check(
    "mapc agrees with world.h: FLY_FLAG_BASE 984, FLY_SLOTS 11",
    mapc.FLY_FLAG_BASE == 984 and mapc.FLY_SLOTS == 11,
)
check(
    "no builtin story flag lies in the Planeador's range 984 .. 984 + FLY_SLOTS - 1 (contract 72), and @fly refuses a slot whose flag is builtin",
    not any(
        mapc.FLY_FLAG_BASE <= v < mapc.FLY_FLAG_BASE + mapc.FLY_SLOTS
        for v in mapc.BUILTIN_FLAGS.values()
    ),
)
_saved = mapc.FLY_SLOTS
mapc.FLY_SLOTS = 16  # as before the fix: slots 11 / 12 would be the flags 995 / 996
m, rep = parse("@fly 11 villa_aliso 1 1\n")
check(
    "... even with the old 16 slots, slot 11 (995 gruta_pit) is refused as a builtin flag",
    errs(rep, "builtin story flag"),
)
m, rep = parse("@fly 12 villa_aliso 1 1\n")
check("... and slot 12 (996 genesa_try)", errs(rep, "builtin story flag"))
mapc.FLY_SLOTS = _saved

# negative cases on the whole world
w, rep = world(
    lambda t: t.replace(
        "@pad 4 4 -> 18 4 face=S\n", "@pad 4 4 -> 8 4 face=S\n"
    ).replace("@pad 8 4 -> 17 6 face=S\n", "@pad 8 4 -> 4 4 face=S\n")
)
check(
    "reachability follows the pads as edges: when no pad of the open area leads into the room, its person and its pads are unreachable (errors)",
    errs(rep, "cannot be reached") and errs(rep, "unreachable"),
)
w, rep = world(
    lambda t: t.replace("@pad 4 4 -> 18 4 face=S\n", "@pad 4 4 -> 5 5 face=S\n")
)
check(
    "a pad whose target is not a pad cell is refused",
    errs(rep, "is not a pad=yes cell"),
)
w, rep = world(
    lambda t: t.replace("@pad 4 4 -> 18 4 face=S\n", "@pad 4 4 -> 4 4 face=S\n")
)
check("a pad that leads to itself is refused", errs(rep, "cannot lead to itself"))


def set_cell(t, i, j, ch):
    """Changes the grid cell (i, j) of a map text."""
    lines = t.split("\n")
    k = lines.index("@grid") + 1 + j
    lines[k] = lines[k][: 2 + i] + ch + lines[k][3 + i :]
    return "\n".join(lines)


w, rep = world(lambda t: set_cell(t, 19, 5, "W"))
check(
    "a pad whose arrival facing (S of B (18,4) = (19,5)) is a wall warns",
    warns(rep, "facing a wall") and not rep.errors[:1],
)
w, rep = world(
    lambda t: t.replace("@pad 4 4 -> 18 4 face=S\n", "@pad 4 4 -> 18 4 face=N\n")
)
check(
    "(N of B is (17,3), free: no warning)",
    not warns(rep, "facing a wall") and not rep.errors,
)
w, rep = world(
    None,
    {
        "ciudad_nodo": lambda t: t.replace(
            "@fly 1 ciudad_nodo 4 13", "@fly 0 ciudad_nodo 4 13"
        )
    },
)
check(
    "two maps with the same @fly slot are refused",
    errs(rep, "@fly slot 0 is also used"),
)
w, rep = world(
    None,
    {
        "ciudad_nodo": lambda t: t.replace(
            "@fly 1 ciudad_nodo 4 13", "@fly 1 ciudad_nodo 0 0"
        )
    },
)
check(
    "a landing tile that is blocked / outside is refused",
    errs(rep, "@fly landing tile"),
)
w, rep = world(
    None,
    {
        "ciudad_nodo": lambda t: t.replace(
            "@fly 1 ciudad_nodo 4 13", "@fly 1 ciudad_nodo 4 12"
        )
    },
)
check(
    "a landing tile on the door (the Centro's warp tile) is refused: (4,12) is the door",
    errs(rep, "@fly landing tile"),
)
w, rep = world(
    None,
    {
        "ciudad_nodo": lambda t: t.replace(
            "@fly 1 ciudad_nodo 4 13", "@fly 1 no_such_town 4 13"
        )
    },
)
check(
    "an unknown town key (no gl.area.<key> text) is refused",
    errs(rep, "does not exist"),
)
w, rep = world(
    None,
    {
        "centro": lambda t: t.replace(
            "@interior yes", "@interior yes\n@fly 10 villa_aliso 1 1"
        )
    },
)
check(
    "a Planeador town must be outdoors (an interior with @fly is refused)",
    errs(rep, "outdoor map"),
)

print(f"mapc_c6_test: {'FAILED ' + str(fails) if fails else 'all ok'}")
sys.exit(1 if fails else 0)
