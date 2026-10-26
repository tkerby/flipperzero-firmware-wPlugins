#!/usr/bin/env python3
"""Test of mapc's C9 syntax (contract 50): the builtin flag names of the League four block 960..983, the `ending run|browse|credits VAR` / `ending title` macros (native 22 operation 14), the `pc` instruction that
now also opens the Hall of Legacy browser when the PC menu asked for it, the `oneway` option of `@warp` (no "has no warp back" warning), and the door idiom of the league rooms (a solid NPC with show_flag /
hide_flag IN FRONT of a warp cell, `gate=yes` for the reachability checks; an NPC on the warp cell itself is refused). The world cases copy maps/ to a temporary root, change a line of the fixtures
(maps/test_c9sys.map = lobby, maps/test_c9sys_b.map = the two rooms) and run the whole load + link + validate + assemble.
  python3 -I tools/mapc_c9_test.py (exit status 1 on a failure; part of tools/build_assets.sh)"""
import shutil
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


def world(edit_lobby=None, edit_rooms=None, edit_other=None):
    """The real world from a temporary copy of maps/ (assets/ linked); edit_lobby / edit_rooms change maps/test_c9sys.map / test_c9sys_b.map, edit_other = {name: fn}. -> (World, Report)."""
    td = Path(tempfile.mkdtemp())
    try:
        (td / "assets").symlink_to(ROOT / "assets")
        shutil.copytree(ROOT / "maps", td / "maps")
        for nm, fn in (
            ("test_c9sys", edit_lobby),
            ("test_c9sys_b", edit_rooms),
            *(edit_other or {}).items(),
        ):
            if fn:
                p = td / f"maps/{nm}.map"
                p.write_text(fn(p.read_text(encoding="utf-8")), encoding="utf-8")
        rep = mapc.Report()
        w = mapc.load_world(td, rep)
        return w, rep
    finally:
        shutil.rmtree(td, ignore_errors=True)


def errs(rep, needle):
    return any(needle in e for e in rep.errors)


def warns(rep, needle):
    return any(needle in e for e in rep.warnings)


def script_bytes(w, name, script):
    """Assembles ONE script of a map on its own (labels resolved inside it)."""
    m = next(x for x in w.maps if x.name == name)
    sc = next(s for s in m.scripts if s.name == script)
    ctx = mapc.ScriptCtx(w, m)
    addr, addrs = 0, []
    for ins in sc.ins:
        addrs.append(addr)
        addr += mapc.ins_size(ins.op)
    out = bytearray()
    for n, ins in enumerate(sc.ins):
        vals = []
        for kind, tok in zip(ins.kinds, ins.args):
            if kind == "u8":
                vals.append(ctx.num(tok, 0, 255, "operand"))
            elif kind == "flag":
                vals.append(ctx.flag(tok))
            elif kind == "var":
                vals.append(ctx.var(tok))
            elif kind in mapc.VN_KINDS:
                op, hi = mapc.VN_KINDS[kind]
                v = ctx.var(tok)
                if v > hi:
                    raise ValueError(f"var {tok!r} must be 0..{hi} here")
                vals.append(v | op << 4)
            elif kind == "label":
                tgt = addrs[sc.labels[tok]] if sc.labels[tok] < len(sc.ins) else addr
                vals.append(tgt - (addrs[n] + mapc.ins_size(ins.op)))
            elif kind == "text":
                vals.append(0)
            else:
                vals.append(0)
        out += mapc.encode_ins(ins.op, vals)
    return bytes(out)


# ---- the flag block -----------------------------------------------------------------------------------------------------------------------------------------------------------------------------
B = mapc.BUILTIN_FLAGS
league = {k: v for k, v in B.items() if k.startswith("league_")}
check(
    "the league names are builtin: league_run 960, league_e1..e4 961..964, league_champ 965, two spare run flags 966 / 967, league_champion 968, league_hof_req 969",
    [
        B[k]
        for k in (
            "league_run",
            "league_e1",
            "league_e2",
            "league_e3",
            "league_e4",
            "league_champ",
            "league_run1",
            "league_run2",
            "league_champion",
            "league_hof_req",
        )
    ]
    == list(range(960, 970)),
)
check(
    "they all sit in the reserved block 960..983 (and nowhere near the Planeador's 984..999, 1016..1021 or the automatic pool 256..767)",
    all(960 <= v <= 983 for v in league.values())
    and not set(league.values())
    & (set(mapc.AUTO_FLAG_POOL) | set(range(984, 1000)) | set(range(1016, 1022))),
)
check(
    "the run byte is 960..967: every name that a blackout must clear is there, league_champion / league_hof_req are not",
    {B[k] for k in league} >= set(range(960, 970))
    and B["league_champion"] >= 968
    and B["league_hof_req"] >= 968,
)
check("no two builtin flags share a number", len(set(B.values())) == len(B))
src = (ROOT / "nucleo/world.h").read_text()
check(
    "world.h agrees: LEAGUE_FLAG0 960 (a multiple of 8), FLAG_LEAGUE_CHAMPION 968, FLAG_LEAGUE_HOF_REQ 969",
    "#define LEAGUE_FLAG0 960" in src
    and "#define FLAG_LEAGUE_CHAMPION 968" in src
    and "#define FLAG_LEAGUE_HOF_REQ 969" in src,
)
h = (ROOT / "nucleo/ending_host.h").read_text()
check(
    "ending_host.h agrees: EN_OP 14, sub-operations run 0 / browse 1 / credits 2 / title 3",
    "EN_OP = 14" in h
    and "EN_RUN = 0, EN_BROWSE, EN_CREDITS, EN_TITLE" in h
    and mapc.ENDING_SUBOPS == {"run": 0, "browse": 1, "credits": 2}
    and mapc.ENDING_TITLE == 3
    and mapc.VN_KINDS["vn14"] == (14, 15),
)

# ---- the real fixtures ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
w, rep = world()
check(
    f"the real world with the C9 fixtures (223 lobby, 224 rooms) loads without errors ({rep.errors[:2]})",
    not rep.errors,
)
by = {m.name: m for m in w.maps}
lobby, rooms = by["test_c9sys"], by["test_c9sys_b"]
check("ids 223 and 224", lobby.id == 223 and rooms.id == 224)
npc = {n.ident: n for n in rooms.npcs}
check(
    "room A's door ahead: solid, hide_flag league_e1 (961), no show_flag; a gate for the reachability checks",
    npc["door_a"].hide_flag == 961
    and npc["door_a"].show_flag == 0
    and npc["door_a"].flags & 1
    and npc["door_a"].gate,
)
check(
    "the doors behind: solid, show_flag league_run (960)",
    all(
        npc[k].show_flag == 960 and npc[k].hide_flag == 0 and npc[k].flags & 1
        for k in ("door_x", "door_b")
    ),
)
check(
    "the member is a trainer whose beaten flag is the builtin league_e1 (961): the door ahead and the member share it",
    npc["m1"].talk_flag == 961 and not npc["m1"].flag_auto,
)
check(
    "the door NPCs stand on the tile in FRONT of the warp cell (4,10): door_a at (4,11); no NPC on a warp cell",
    (npc["door_a"].i, npc["door_a"].j) == (4, 11)
    and any((wp.i, wp.j) == (4, 10) for wp in rooms.warps),
)
check(
    "the rooms have one warp (the way on to room B) and no warp back; the lobby's warp is `oneway`",
    len(rooms.warps) == 1 and len(lobby.warps) == 1 and lobby.warps[0].oneway,
)
check(
    'no "has no warp back" warning for the lobby -> rooms warp (oneway)',
    not warns(rep, "test_c9sys: warps to test_c9sys_b but"),
)
champ = next(s for s in rooms.scripts if s.name == "champ")
check(
    "the champion's script has the `ending run` and `ending title` natives (native 22 ops)",
    sum(1 for i in champ.ins if i.op == "call_native" and i.args[0] == "22") == 2,
)
b = script_bytes(w, "test_c9sys_b", "league_in")
check(
    "league_in compiles to: jf league_run in / sfx door / setf league_run / say / (end of script)",
    b[:3] == bytes([0xC4, 0xC0, 0x0E]) or b[0] == 0xC4 and b[1] == 0xC0,
)
check(
    "... with setf league_run = 0xC7 0xC0 (960: bits 8..9 in the opcode byte)",
    bytes([0xC7, 0xC0]) in b,
)


# ---- `ending ...` -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
def with_rooms_script(lines):
    return lambda t: t.replace(
        "@script champ npc\n",
        "@script champ npc\n"
        + "".join(f"    {x}\n" for x in lines)
        + "    end\n@end\n@script champ_old npc\n",
        1,
    )


w2, rep2 = world(
    None,
    with_rooms_script(
        ["ending run 1", "ending browse 3", "ending credits 2", "ending title"]
    ),
)
check(
    f"`ending run|browse|credits VAR` and `ending title` compile ({rep2.errors[:2]})",
    not rep2.errors,
)
if not rep2.errors:
    b = script_bytes(w2, "test_c9sys_b", "champ")
    check(
        "ending run 1 = call_native 22 (14 << 4 | 1) 0 = 1A 16 E1 00",
        b[:4] == bytes([0x1A, 22, 0xE1, 0x00]),
    )
    check("ending browse 3 = 1A 16 E3 01", b[4:8] == bytes([0x1A, 22, 0xE3, 0x01]))
    check("ending credits 2 = 1A 16 E2 02", b[8:12] == bytes([0x1A, 22, 0xE2, 0x02]))
    check(
        "ending title = 1A 16 E0 03 (the sub-operation 3 is the resident one)",
        b[12:16] == bytes([0x1A, 22, 0xE0, 0x03]),
    )
for bad, why in (
    ("ending", "no operand"),
    ("ending run", "no VAR"),
    ("ending run 1 2", "two VARs"),
    ("ending foo 1", "an unknown word"),
    ("ending title 1", "title takes none"),
):
    w3, rep3 = world(None, with_rooms_script([bad]))
    check(
        f"`{bad}` ({why}) is refused with the usage line",
        errs(rep3, "ending run VAR | ending browse VAR"),
    )
w3, rep3 = world(None, with_rooms_script(["ending run 16"]))
check(
    "`ending run 16`: VAR must be 0..15 (a variable that does not exist)",
    bool(rep3.errors),
)
w3, rep3 = world(None, with_rooms_script(["ending run res"]))
check(
    "a variable name is not defined unless the map says so (an error); `@var res 1` gives it a name",
    bool(rep3.errors),
)
w3, rep3 = world(
    None,
    lambda t: with_rooms_script(["ending run res"])(
        t.replace("@spawn enter1", "@var res 1\n@spawn enter1", 1)
    ),
)
check(f"... with `@var res 1` it compiles ({rep3.errors[:2]})", not rep3.errors)

# ---- `pc` ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
b = script_bytes(w, "test_c9sys", "pc_box")
check(
    "`pc` = native 22 0 0 / jnf league_hof_req L / clrf league_hof_req / ending browse (VAR 0) / L: (end of the script)",
    b == bytes([0x1A, 22, 0, 0, 0xC5, 0xC9, 6, 0, 0xC8, 0xC9, 0x1A, 22, 0xE0, 0x01]),
)
check(
    "the flag operand 969 travels as in contract 40.3: opcode | (969 >> 8) << 6 = 0xC5 / 0xC8, operand 969 & 255 = 0xC9",
    969 >> 8 == 3 and 969 & 255 == 0xC9,
)
w3, rep3 = world(
    None,
    None,
    {
        "centro": lambda t: t.replace(
            "@script pc_box interact\n    pc\n",
            "@script pc_box interact\n    say gl.center.bye\n    pc\n    say gl.center.bye\n",
        )
    },
)
check(
    f"`pc` in the middle of a script (a Centro's) works and the jump lands on the next instruction ({rep3.errors[:2]})",
    not rep3.errors,
)
if not rep3.errors:
    b = script_bytes(w3, "centro", "pc_box")
    # say (3 bytes) + pc (4 + 4 + 2 + 4) + say (3): the jnf's rel16 skips the clrf and the native (6 bytes)
    check(
        "... say, 1A 16 00 00, C5 C9 06 00, C8 C9, 1A 16 E0 01, say",
        b[3:7] == bytes([0x1A, 22, 0, 0])
        and b[7:11] == bytes([0xC5, 0xC9, 6, 0])
        and len(b) == 3 + 14 + 3,
    )
w3, rep3 = world(
    None,
    None,
    {
        "centro": lambda t: t.replace(
            "@script pc_box interact\n    pc\n", "@script pc_box interact\n    pc 1\n"
        )
    },
)
check(
    "`pc` with an operand is still the plain native (the PC box takes none: the generic native operand check refuses it)",
    errs(rep3, "takes 0 operand"),
)
for nm in ("centro", "centro_marea", "casa_teo"):
    m = by[nm]
    s = next(x for x in m.scripts if x.name == "pc_box")
    check(
        f"every real PC terminal ({nm}) got the browser tail: 4 instructions",
        len(s.ins) == 4 and s.ins[1].op == "jnf" and s.ins[2].op == "clrf",
    )

# ---- `@warp ... oneway` -------------------------------------------------------------------------------------------------------------------------------------------------------------------------
w3, rep3 = world(lambda t: t.replace(" oneway", ""))
check(
    "without `oneway` the lobby -> rooms warp WARNS that the rooms have no way back",
    warns(rep3, "test_c9sys: warps to test_c9sys_b but test_c9sys_b has no warp back"),
)
check(
    "`oneway` is tool-only: the warp record is the same (flags 0, any arrival facing)",
    lobby.warps[0].flags == 0 and lobby.warps[0].req_dirs == 0xFF,
)

# ---- the door idiom ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------
w3, rep3 = world(
    None,
    lambda t: t.replace(
        "  a  tile=dirt_a\n",
        "  a  tile=dirt_a npc=keydoor face=S solid=yes gate=yes text=gl.c9.door.locked hide_flag=league_e1\n",
        1,
    ),
)
check(
    "an NPC ON the warp cell is refused (the doors stand in front of it)",
    errs(rep3, "NPC placed on a solid/warp tile"),
)
w3, rep3 = world(
    None, lambda t: t.replace("hide_flag=league_e1", "hide_flag=league_nope", 1)
)
check(
    "a made-up league name is refused (the names are the builtin ones)",
    bool(rep3.errors),
)
w3, rep3 = world(None, lambda t: t.replace("@legend", "@flag league_run 5\n@legend", 1))
check(
    "a map cannot redefine a builtin league flag under another number",
    errs(rep3, "league_run") or errs(rep3, "another map says"),
)
w3, rep3 = world(
    None,
    lambda t: t.replace("beaten_flag=league_e1", "beaten_flag=league_e2", 1).replace(
        "hide_flag=league_e1", "hide_flag=league_e2", 1
    ),
)
check(
    f"another member's beaten flag (league_e2 = 962) is as good ({rep3.errors[:2]})",
    not rep3.errors,
)

print(f"mapc_c9_test: {'FAILED ' + str(fails) if fails else 'all ok'}")
sys.exit(1 if fails else 0)
