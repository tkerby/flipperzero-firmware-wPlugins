#!/usr/bin/env python3
"""Test of mapc's 10-bit story flags (contract 40): the flag operands of jf / jnf / setf / clrf (bits 8..9 in bits 6..7 of the opcode byte, an id <= 255 encodes byte for byte as before), the ranges
(1..1023), the NpcDef / ScriptDef / InteractDef spare bits, the automatic pool (a fresh range above everything used so far, the legacy numbers of maps/auto_flags.txt still valid) and the rule that
the interior floors of the Neon mansion are big 2 x 2 slabs (polished_<i%2><j%2>), never the small `carpet`.
  python3 -I tools/mapc_flags_test.py        (exit status 1 on a failure; part of tools/build_assets.sh)"""
import re
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mapc  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
fails = 0


def check(what, cond):
    global fails
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        fails += 1


def compile_lines(lines, flag_defs=None):
    m = mapc.Map(name="t")
    m.flag_defs = dict(flag_defs or {})
    rep = mapc.Report()
    sc = mapc.Script(name="s", map="t")
    m.scripts.append(sc)
    for n, ln in enumerate(lines):
        mapc.parse_script_line(sc, ln, ln.split(), f"t:{n}", rep)
    if rep.errors:
        return None, rep.errors
    sc.ins.append(mapc.Ins("end", [], [], "t:end"))
    _t, code = mapc.assemble_scripts(None, m, rep)
    return (code, rep.errors) if not rep.errors else (None, rep.errors)


print("== flag operands: ids <= 255 are byte for byte what they were, higher ids put bits 8..9 in the opcode byte")
OLD = {"jf": 0x04, "jnf": 0x05, "setf": 0x07, "clrf": 0x08}
for op, base in OLD.items():
    for fid in (1, 2, 127, 128, 255):
        raw = mapc.encode_ins(op, [fid, 0] if op in ("jf", "jnf") else [fid])
        want = bytes([base, fid]) + (b"\0\0" if op in ("jf", "jnf") else b"")
        check(f"{op} {fid}: {raw.hex()} == the old encoding", raw == want)
    for fid in (256, 257, 511, 512, 768, 900, 1000, 1023):
        raw = mapc.encode_ins(op, [fid, 0] if op in ("jf", "jnf") else [fid])
        ok = raw[0] & 0x3F == base and raw[0] >> 6 == fid >> 8 and raw[1] == fid & 255 and len(raw) == mapc.ins_size(op)
        back = mapc.decode_script(raw)[0]
        check(f"{op} {fid}: {raw.hex()} (opcode {base:#04x} | {fid >> 8} << 6, operand {fid & 255}) decodes back to {fid}", ok and back[1] == op and back[2][0] == fid)
check("the instruction sizes did not change (setf 2 B, jf 4 B)", mapc.ins_size("setf") == 2 and mapc.ins_size("jf") == 4)
for bad in (bytes([0x01 | 0x40, 0, 0]), bytes([0x15 | 0x80]), bytes([0x20 | 0xC0, 0])):
    try:
        mapc.decode_script(bad)
        check(f"a non-flag opcode with bits 6..7 set is refused ({bad.hex()})", False)
    except ValueError:
        check(f"a non-flag opcode with bits 6..7 set is refused ({bad.hex()})", True)
check("every opcode number is still below 0x40 (the top two bits are free)", max(v[0] for v in mapc.OPS.values()) < 0x40)

print("== the assembler: ranges, @flag names, plain numbers")
code, errs = compile_lines(["setf 900", "setf hi", "clrf 1023", "jf 770 out", "jnf 256 out", "out:"], {"hi": 1000})
check("setf 900 / setf hi (1000) / clrf 1023 / jf 770 / jnf 256 assemble", code is not None)
if code:
    dec = mapc.decode_script(code)
    check("... and decode to those ids", [(n, v[0]) for _a, n, v in dec if n != "end"] == [("setf", 900), ("setf", 1000), ("clrf", 1023), ("jf", 770), ("jnf", 256)])
    jf, jnf, end = dec[3], dec[4], dec[5]
    check("... the jump labels still count from the end of the instruction (jf -> the final end, jnf too)", jf[0] + 4 + jf[2][1] == end[0] and jnf[0] + 4 + jnf[2][1] == end[0])
for bad in (["setf 1024"], ["setf 0"], ["clrf 5000"], ["jf 2000 x", "x:"]):
    _c, errs = compile_lines(bad)
    check(f"`{bad[0]}` is an error", _c is None and bool(errs))
code, _e = compile_lines(["setf 255", "jf 255 l", "l:"])
check("an old script (flag 255) assembles to the same bytes as before: 07 ff 04 ff 00 00 00", code == bytes.fromhex("07ff04ff000000"))

print("== map data: NpcDef / InteractDef / ScriptDef spare bits")
n = mapc.Npc(spr=0, name="x", i=0, j=0, facing=0, flags=1, show_flag=900, hide_flag=300, talk_flag=1023)
b = mapc.npc_flags_byte(n)
check(f"NpcDef.flags {b:#04x}: bit0 solid, show 900 = hi 3 in bits 2..3, hide 300 = hi 1 in bits 4..5, talk 1023 = hi 3 in bits 6..7", b == 1 | 3 << 2 | 1 << 4 | 3 << 6)
n2 = mapc.Npc(spr=0, name="x", i=0, j=0, facing=0, flags=3, show_flag=77, hide_flag=255, talk_flag=1)
check("an NPC with flags <= 255 keeps NpcDef.flags exactly (solid | script)", mapc.npc_flags_byte(n2) == 3)
d = mapc.pack_script_def(10, 20, 2, 906, 1)
check(f"ScriptDef for condition 906: {d.hex()} (cond_lo 138, value 1, cond_hi 3)", d == bytes([10, 0, 20, 0, 2, 138, 1, 3]))
d = mapc.pack_script_def(10, 20, 2, 89, 0)
check("a condition <= 255 leaves the old `reserved` byte 0", d[7] == 0 and d[5] == 89)

print("== the automatic pool")
check("AUTO_FLAG_POOL is one contiguous range above 255", list(mapc.AUTO_FLAG_POOL) == list(range(mapc.AUTO_FLAG_POOL[0], mapc.AUTO_FLAG_POOL[-1] + 1)) and mapc.AUTO_FLAG_POOL[0] > 255)
used = set(mapc.LEGACY_AUTO_POOL) | set(mapc.BUILTIN_FLAGS.values()) | set(range(230, 256))
check("... it is disjoint from every number chapters 1 - 4a could have used (the legacy pool, the builtins, the fixtures)", not used & set(mapc.AUTO_FLAG_POOL))
check("... and from the story range", not set(mapc.STORY_FLAG_RANGE) & set(mapc.AUTO_FLAG_POOL) and mapc.STORY_FLAG_RANGE[-1] == mapc.MAX_FLAG)
check("the reserved ids are untouched: badges 80..87, modules 88..92, the bike 93", [mapc.BUILTIN_FLAGS[k] for k in ("badge_cantera", "badge_marea", "badge_voltio", "badge_raiz")] == [80, 81, 82, 83]
      and [mapc.BUILTIN_FLAGS[k] for k in ("mod_cortador", "mod_linterna", "mod_hidro", "mod_empuje", "mod_planeador")] == [88, 89, 90, 91, 92] and mapc.BUILTIN_FLAGS["on_bike"] == 93)


def alloc(lock_lines, n_new, test_map=False):
    with tempfile.TemporaryDirectory() as td:
        (Path(td) / "maps").mkdir()
        (Path(td) / "maps" / mapc.AUTO_FLAGS_FILE).write_text("".join(lock_lines))
        m = mapc.Map(name="t", id=5 if not test_map else 205)
        for k in range(n_new):
            m.npcs.append(mapc.Npc(spr=0, name="x", i=k, j=0, facing=0, gen="trainer", flag_auto=f"beat_t_{k}_0"))
        w = mapc.World(catalog=None, maps=[m], start=("t", "s"), report=mapc.Report())
        mapc.alloc_auto_flags(w, td)
        return w, m, (Path(td) / "maps" / mapc.AUTO_FLAGS_FILE).read_text()


w, m, txt = alloc(["# c\n", "old_a 150\n", "old_b 58\n"], 3)
got = [m.flag_defs[f"beat_t_{k}_0"] for k in range(3)]
check(f"new trainers get 256, 257, 258 ({got}); the legacy lines are valid and kept", got == [256, 257, 258] and not w.report.errors and "old_a 150" in txt and "beat_t_2_0 258" in txt)
check("the NPC's beaten flag is the allocated one", [x.talk_flag for x in m.npcs] == [256, 257, 258])
w, m, txt = alloc(["beat_t_0_0 300\n"], 2)
check("a name already locked to a new-range number keeps it; the next one takes the first free", m.flag_defs["beat_t_0_0"] == 300 and m.flag_defs["beat_t_1_0"] == 256 and not w.report.errors)
w, m, txt = alloc(["old 1000\n"], 1)
check("a line outside both pools is an error", bool(w.report.errors))
w, m, txt = alloc([], len(mapc.AUTO_FLAG_POOL) + 1)
check(f"the pool ({len(mapc.AUTO_FLAG_POOL)} numbers) runs out with an error, not a wrong number", any("no free automatic flag" in e for e in w.report.errors))
w, m, txt = alloc([], 2, test_map=True)
check("a test map (id >= 200) still takes the fixture pool 230..255 and writes nothing", [m.flag_defs[f"beat_t_{k}_0"] for k in range(2)] == [230, 231] and txt == "")

print("== the real files")
txt = (ROOT / "maps" / mapc.AUTO_FLAGS_FILE).read_text()
ids = [int(x.split()[1]) for x in txt.splitlines() if x.strip() and not x.startswith("#")]
check(f"maps/auto_flags.txt: {len(ids)} lines, all valid, unique", len(ids) == len(set(ids)) and all(i in mapc.AUTO_FLAG_VALID for i in ids))


def floor_tiles(path):
    """-> {(i, j): tile name or None} of a map source (legend `tile=`, else @fill)."""
    lines = [re.sub(r"\s+#.*$", "", ln) for ln in path.read_text().split("\n") if not ln.lstrip().startswith("#")]
    fill = next((ln.split()[1] for ln in lines if ln.startswith("@fill")), None)
    li, gi = lines.index("@legend"), lines.index("@grid")
    leg = {}
    for ln in lines[li + 1:gi]:
        mm = re.match(r"^\s+(\S)\s+(.*)$", ln)
        if mm:
            t = re.search(r"\btile=(\S+)", mm.group(2))
            leg[mm.group(1)] = t.group(1) if t else fill
    out = {}
    for j, row in enumerate(ln.strip("\n") for ln in lines[gi + 1:]):
        if row.startswith("@"):
            break
        row = row[2:]
        for i, ch in enumerate(row):
            out[(i, j)] = leg.get(ch, fill)
    return out


print("== the Neon mansion's floors are big slabs")
src = (ROOT / "maps" / "mansion_neon.map").read_text()
check("mansion_neon.map has no `carpet` tile", "tile=carpet" not in src and "@fill carpet" not in src)
bad = [(c, t) for c, t in floor_tiles(ROOT / "maps" / "mansion_neon.map").items() if t not in (None, "none", "mat") and t != f"polished_{c[0] % 2}{c[1] % 2}"]
check(f"every floor cell is polished_<i%2><j%2> (the slab joints run through objects and NPCs); {len(bad)} wrong {bad[:3]}", not bad)
tiles2 = {t for t in floor_tiles(ROOT / "maps" / "mansion_neon_2.map").values() if t not in (None, "none")}
check(f"mansion_neon_2 (the study) uses only a plain floor ({sorted(tiles2)}): no small tiles", tiles2 <= {"floor_wood"} or all(t.startswith(("polished_", "oplank_")) for t in tiles2))

print(f"mapc_flags_test: {fails} problem(s)")
sys.exit(1 if fails else 0)
