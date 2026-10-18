#!/usr/bin/env python3
"""Test of mapc's casino macros (contract 37.3 / 38.7; the friendship macros `bond tier|heal VAR`, contract 65): `coins_buy VAR`, `slots VAR`, `prizes SHOP VAR`, `coins_get VAR` compile to exactly the bytes of the raw
`native pc A B` forms of the contract's table (pc 16+VAR 0, 32+VAR 0, 48+VAR SHOP, 64+VAR 0), with var names, shop names and numbers, and their usage errors are reported.
  python3 -I tools/mapc_casino_test.py        (exit status 1 on a failure; part of tools/build_assets.sh)"""
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
    """-> (code bytes, errors) of one script made of `lines` (the same two entry points mapc.build uses: parse_script_line, assemble_scripts)."""
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


def same(macro, raw, var_defs=None):
    a, ea = compile_lines([macro], var_defs)
    b, eb = compile_lines([raw], var_defs)
    check(f"`{macro}` == `{raw}` ({a.hex() if a else ea})", a is not None and a == b)


same("coins_buy 4", "native pc 20 0")
same("coins_buy 0", "native pc 16 0")
same("coins_buy 15", "native pc 31 0")
same("slots 2", "native pc 34 0")
same("slots 15", "native pc 47 0")
same("coins_get 3", "native pc 67 0")
same("prizes 5 2", "native pc 50 5")
same("prizes sala_premios 2", "native pc 50 5")
same("prizes sala_premios 15", "native pc 63 5")
same("coins_buy price", "native pc 23 0", {"price": 7})
same("slots machine", "native pc 33 0", {"machine": 1})
same("prizes sala_premios got", "native pc 58 5", {"got": 10})
# C5-B (contract 43.3): the Reserva Salvaje's session macros, ops 5 / 6 / 7 of native 22 (pc 80+VAR / 96+VAR / 112+VAR)
same("reserva_start 4", "native pc 84 0")
same("reserva_start 0", "native pc 80 0")
same("reserva_start 15", "native pc 95 0")
same("reserva_end 3", "native pc 99 0")
same("reserva_end 15", "native pc 111 0")
same("reserva_left 0", "native pc 112 0")
same("reserva_left 15", "native pc 127 0")
same("reserva_start result", "native pc 87 0", {"result": 7})
same("reserva_left inside", "native pc 114 0", {"inside": 2})
code, _ = compile_lines(["reserva_start 4"])
check("reserva_start: one call_native, native 22, a = 80 + 4, b = 0", code is not None and code[:4] == bytes([0x1A, 22, 84, 0]))
code, _ = compile_lines(["reserva_end 4"])
check("reserva_end: a = 96 + 4", code is not None and code[:4] == bytes([0x1A, 22, 100, 0]))
code, _ = compile_lines(["reserva_left 4"])
check("reserva_left: a = 112 + 4", code is not None and code[:4] == bytes([0x1A, 22, 116, 0]))
# contract 65: the friendship operations, op 15 of native 22 (pc 240+VAR, sub-operation 0 = tier / 1 = heal in the second operand; nucleo/casino_host.h CAS_BOND)
same("bond tier 4", "native pc 244 0")
same("bond tier 0", "native pc 240 0")
same("bond tier 15", "native pc 255 0")
same("bond heal 0", "native pc 240 1")
same("bond heal 3", "native pc 243 1")
same("bond tier shown", "native pc 246 0", {"shown": 6})
code, _ = compile_lines(["bond tier 4"])
check("bond tier: one call_native, native 22, a = 240 + 4, b = 0", code is not None and code[:4] == bytes([0x1A, 22, 244, 0]))
code, _ = compile_lines(["bond heal 0"])
check("bond heal: a = 240, b = 1", code is not None and code[:4] == bytes([0x1A, 22, 240, 1]))
# the macro lines are one call_native each (native 22), the operand layout is [22, a, b]
code, _ = compile_lines(["coins_buy 4"])
check("one call_native instruction: opcode 0x1A, native 22, a = 16 + 4, b = 0", code is not None and code[:4] == bytes([0x1A, 22, 20, 0]))
code, _ = compile_lines(["prizes 5 2"])
check("prizes: native 22, a = 48 + 2, b = the shop", code is not None and code[:4] == bytes([0x1A, 22, 50, 5]))
# errors
for bad, why in (("coins_buy", "no operand"), ("coins_buy 1 2", "two operands"), ("slots", "no operand"), ("coins_get 1 2", "two operands"), ("prizes 5", "one operand"),
                 ("coins_buy 16", "var 16 does not exist"), ("slots 20", "var 20 does not exist"), ("prizes nope 1", "unknown shop"), ("prizes 5 99", "var 99"),
                 ("coins_get nosuchvar", "unknown var"),
                 ("reserva_start", "no operand"), ("reserva_end 1 2", "two operands"), ("reserva_left", "no operand"), ("reserva_start 16", "var 16 does not exist"),
                 ("reserva_end 99", "var 99"), ("reserva_left nosuchvar", "unknown var"),
                 ("bond", "no operands"), ("bond tier", "no variable"), ("bond cuddle 1", "unknown operation"), ("bond tier 1 2", "three operands"), ("bond tier 16", "var 16 does not exist"), ("bond heal nosuchvar", "unknown var")):
    code, errs = compile_lines([bad])
    check(f"`{bad}` is refused ({why}): {errs[0] if errs else 'accepted'}", code is None and bool(errs))
# a whole script of the contract's clerk model assembles
code, errs = compile_lines(["coins_buy 4", "jv 4 1 bought", "end", "bought:", "coins_get 5", "slots 2", "prizes sala_premios 2", "end"])
check(f"a clerk-style script assembles ({errs})", code is not None and not errs)
code, errs = compile_lines(["bond tier 4", "jv 4 255 none", "jv 4 0 cold", "end", "none:", "end", "cold:", "bond heal 5", "end"])
check(f"an evaluator-style script assembles ({errs})", code is not None and not errs)
code, errs = compile_lines(["reserva_start 4", "jv 4 1 ok", "end", "ok:", "reserva_left 5", "reserva_end 6", "end"])
check(f"a Reserva gate-style script assembles ({errs})", code is not None and not errs)
print(f"mapc_casino_test: {'FAILED ' + str(fails) if fails else 'all ok'}")
sys.exit(1 if fails else 0)
