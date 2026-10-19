#!/usr/bin/env python3
"""Test of mapc's IR LINK macros (contract 45.8): `link trade VAR` = native 22 (9 << 4 | VAR) followed by the evolution operation on VAR + 1 (12 << 4 | VAR + 1), `link battle VAR TRAINER` =
native 22 (10 << 4 | VAR) with the trainer id, `link selftest VAR` = native 22 (11 << 4 | VAR); VAR must be 0..14. The raw `native link A B` form compiles too (mapdump round trips).
  python3 -I tools/mapc_link_test.py        (exit status 1 on a failure; part of tools/build_assets.sh)"""
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import mapc  # noqa: E402

fails = 0


def check(what, cond):
    global fails
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        fails += 1


def compile_lines(lines, var_defs=None):
    m = mapc.Map(name="t")
    m.var_defs = dict(var_defs or {})
    rep = mapc.Report()
    sc = mapc.Script(name="s", map="t")
    m.scripts.append(sc)
    for n, ln in enumerate(lines):
        tok = ln.split()
        mapc.parse_script_line(sc, ln, tok, f"t:{n}", rep)
    if rep.errors:
        return None, rep.errors
    sc.ins.append(mapc.Ins("end", [], [], "t:end"))
    _table, code = mapc.assemble_scripts(None, m, rep)
    return (code, rep.errors) if not rep.errors else (None, rep.errors)


def native(a, b):  # one call_native: opcode 0x1A, native id, a, b
    return bytes([0x1A, 22, a, b])


code, errs = compile_lines(["link trade 4"])
check(f"link trade 4 = native 22 (9<<4|4), then the evolution op on var 5 (12<<4|5): {code.hex() if code else errs}", code is not None and code[:8] == native(0x94, 0) + native(0xC5, 0))
code, errs = compile_lines(["link trade 0"])
check("link trade 0 -> a = 0x90, evolution on var 1 (0xC1)", code is not None and code[:8] == native(0x90, 0) + native(0xC1, 0))
code, errs = compile_lines(["link trade 14"])
check("link trade 14 -> a = 0x9E, evolution on var 15 (0xCF)", code is not None and code[:8] == native(0x9E, 0) + native(0xCF, 0))
code, errs = compile_lines(["link trade res"], {"res": 3})
check("link trade with a named var (res = 3) -> 0x93 then 0xC4", code is not None and code[:8] == native(0x93, 0) + native(0xC4, 0))
code, errs = compile_lines(["link battle 4 2"])
check(f"link battle 4 2 = native 22 (10<<4|4), trainer 2: {code.hex() if code else errs}", code is not None and code[:4] == native(0xA4, 2))
first = next(iter(mapc._toml_ids("trainers.toml")))
code, errs = compile_lines([f"link battle 4 {first}"])
check(f"link battle 4 {first} (a trainers.toml name) -> trainer id 1", code is not None and code[:4] == native(0xA4, 1))
code, errs = compile_lines(["link selftest 6"])
check("link selftest 6 = native 22 (11<<4|6)", code is not None and code[:4] == native(0xB6, 0))
code, errs = compile_lines(["native pc 196 0", "native pc 20 0"])
check("the raw `native pc 196 0` (evolution) compiles; the casino's `native pc 20 0` is unchanged", code is not None and code[:4] == native(196, 0) and code[4:8] == native(20, 0))
for bad, why in (("link", "no operands"), ("link trade", "no var"), ("link trade 15", "VAR 15 leaves no room for VAR + 1"), ("link trade 16", "var 16 does not exist"),
                 ("link battle 4", "no trainer"), ("link battle 4 9999", "unknown trainer"), ("link battle 15 2", "var 15"), ("link selftest", "no var"),
                 ("link trade 1 2", "two operands")):
    code, errs = compile_lines([bad])
    check(f"`{bad}` is refused ({why}): {errs[0] if errs else 'accepted'}", code is None and bool(errs))
code, errs = compile_lines(["link trade 4", "jv 4 1 done", "end", "done:", "link battle 6 2", "end"])
check(f"a club attendant style script assembles ({errs})", code is not None and not errs)
# the operation numbers of native 22 are shared with the casino (CAS_*, casino_host.h: 1..8 incl. the Reserva's 5..7 and the step hook's 8): the link's must not collide with them
cas_ops = {op for (op, _n, _u) in mapc.CASINO_MACROS.values()} | {8}   # 8 = CAS_RV_TIMEUP (not a macro)
link_ops = {op for (op, _hi, _plus) in mapc.LINK_KINDS.values()}
check(f"link ops {sorted(link_ops)} are disjoint from the casino's {sorted(cas_ops)} and fit the 4-bit field", not (link_ops & cas_ops) and max(link_ops) <= 15)
code, errs = compile_lines(["reserva_start 4", "link trade 4"])
check("a casino macro and a link macro in one script keep their own ops (0x54 reserva_start, 0x94 link trade)", code is not None and code[:4] == native(0x54, 0) and code[4:8] == native(0x94, 0))
# the Club de Enlace attendant (contract 61): every Centro has the same `club` script at (8,1) with the three link natives, and the trainer row `amigo` exists
import glob, os
cent = sorted(glob.glob(str(Path(__file__).resolve().parent.parent / "maps" / "centro*.map")))
check(f"{len(cent)} Centro maps (the ten of the game)", len(cent) == 10)
for f in cent:
    t = Path(f).read_text()
    nm = os.path.basename(f)
    check(f"{nm}: the attendant `script=club` stands at (8,1) and the script uses link trade / battle amigo / selftest and the 4-row menu",
          "script=club" in t and "@script club npc" in t and "link trade club_res" in t and "link battle club_res amigo" in t and "link selftest club_res" in t
          and "elevator club_sel gl.club.trade gl.club.battle gl.club.test gl.club.exit" in t and [ln for ln in t.split("@grid\n")[1].split("\n")][1][2 + 8] == "X")
check("trainers.toml has the exhibition row `amigo`", "[amigo]" in (Path(__file__).resolve().parent.parent / "assets/data/trainers.toml").read_text())
print(f"mapc_link_test: {'FAILED ' + str(fails) if fails else 'all ok'}")
sys.exit(1 if fails else 0)
