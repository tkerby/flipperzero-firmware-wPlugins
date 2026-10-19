#!/usr/bin/env python3
"""Short-call proof (contract MEM3, "calls"). For every linked module (elementis_zeta_d.elf, ez_menu_d.elf, ez_ui_d.elf, ez_pc_d.elf, ez_battle_d.elf):

* no PC-relative branch (R_ARM_THM_CALL = THM_PC22, THM_JUMP24, CALL, JUMP24, ...) may target a symbol the module does not define, except, for a
  plugin, a function of the app's API table (nucleo/app_api_table_i.h): the firmware lives in flash ~400 MB away from the heap block the module
  is loaded into, so the loader (flipperzero-firmware lib/flipper_application/elf/elf_file.c, elf_relocate_jmp_call) would have to create a
  malloc'ed 6-byte trampoline per distinct firmware target ("doing trampoline"); the app's functions are in RAM, a few hundred KB from the
  plugin (a BL reaches +-16 MB), and the table's pointers have the Thumb bit set, so no trampoline is needed. Calls to the firmware must stay
  `ldr rX, =f; blx rX` (an R_ARM_ABS32 literal);
* only the relocation types the loader handles may appear (ABS32 / TARGET1 / REL32 / THM_PC22 / THM_JUMP24 / CALL / THM_MOVW/MOVT_ABS): a
  R_ARM_THM_JUMP19 (conditional b.w) or JUMP11 against another section would be skipped or fail at load time.
It prints per module the numbers behind the gain: branches to the module's own symbols (cheap: no trampoline) and to the app (plugins), ABS32
literals, and the loader's expected trampoline count (violations above; must be 0).
Usage: python3 tools/check_short_calls.py [build dir]   (default ~/.ufbt/build, after `cd nucleo && ufbt build`); exit 1 on a violation."""
import re, subprocess, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
import os
HOME = pathlib.Path(os.environ.get("UFBT_HOME") or (pathlib.Path.home() / ".ufbt"))  # UFBT_HOME: a private ufbt state dir (parallel work trees)
BUILD = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else HOME / "build"
READELF = next((HOME / "toolchain").glob("*/bin/arm-none-eabi-readelf"), None)
TABLE = ROOT / "nucleo/app_api_table_i.h"
# branch relocations the loader turns into BL / B.W (+ trampoline when out of range or the target is ARM state)
LOADER_BRANCH = {"R_ARM_THM_CALL", "R_ARM_THM_JUMP24", "R_ARM_CALL", "R_ARM_JUMP24"}
# every other PC-relative branch type: not handled by elf_relocate_symbol
UNSUPPORTED_BRANCH = {"R_ARM_THM_JUMP19", "R_ARM_THM_JUMP11", "R_ARM_THM_JUMP8", "R_ARM_THM_JUMP6", "R_ARM_PC24", "R_ARM_PLT32", "R_ARM_THM_XPC22",
                      "R_ARM_THM_PC8", "R_ARM_THM_PC12", "R_ARM_JUMP11", "R_ARM_V4BX"}
MODULES = (("elementis_zeta", False), ("ez_menu", True), ("ez_ui", True), ("ez_pc", True), ("ez_casino", True), ("ez_link", True), ("ez_ending", True), ("ez_battle", True))


def undefined_symbols(elf):
    out = subprocess.check_output([str(READELF), "-s", "-W", str(elf)]).decode()
    und = set()
    for line in out.splitlines():
        p = line.split()
        if len(p) >= 8 and p[6] == "UND" and p[7]:
            und.add(p[7])
    return und


def check(elf, app_api):
    und = undefined_symbols(elf)
    out = subprocess.check_output([str(READELF), "-r", "-W", str(elf)]).decode()
    sec = None
    bad, own, app, abs32 = [], 0, 0, 0
    for line in out.splitlines():
        m = re.match(r"Relocation section '(\.rel\.(?:text|rodata|data)\S*)'", line)
        if m:
            sec = m.group(1)
            continue
        if line.startswith("Relocation section"):
            sec = None
            continue
        m = re.match(r"([0-9a-f]{8})\s+\S+\s+(R_ARM_\w+)\s+([0-9a-f]{8})\s+(\S+)", line)
        if not m or not sec:
            continue
        off, typ, val, sym = m.groups()
        if typ in UNSUPPORTED_BRANCH:
            bad.append((sec, off, typ, sym, "relocation type not handled by the loader"))
        elif typ in LOADER_BRANCH:
            if sym not in und:
                own += 1
                if not int(val, 16) & 1:  # elf_relocate_jmp_call: an even target address means ARM state (BLX, trampoline if bit 1 is set)
                    bad.append((sec, off, typ, sym, "target is not a Thumb function symbol (even address)"))
            elif sym in app_api:
                app += 1
            else:
                bad.append((sec, off, typ, sym, "branch to a firmware / unknown symbol: the loader would build a trampoline"))
        elif typ == "R_ARM_ABS32":
            abs32 += 1
    return bad, own, app, abs32


def main():
    if READELF is None:
        sys.exit("toolchain not found under ~/.ufbt/toolchain")
    app_api = set(re.findall(r"API_(?:METHOD|VARIABLE)\((\w+)", TABLE.read_text()))
    fail = False
    for name, plugin in MODULES:
        elf = BUILD / f"{name}_d.elf"
        if not elf.exists():
            print(f"{name}: {elf} missing (ufbt build first)")
            fail = True
            continue
        bad, own, app, abs32 = check(elf, app_api if plugin else set())
        print(f"{name:14s} {'OK  ' if not bad else 'FAIL'} trampolines expected {len(bad)}; branches: {own} to its own code, {app} to the app's API table; ABS32 relocations (literals, tables): {abs32}")
        for sec, off, typ, sym, why in bad[:20]:
            print(f"    {sec} +{off} {typ} -> {sym}: {why}")
        fail |= bool(bad)
    sys.exit(1 if fail else 0)


main()
