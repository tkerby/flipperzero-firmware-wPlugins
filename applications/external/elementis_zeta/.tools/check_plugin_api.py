#!/usr/bin/env python3
"""Checks the plugins' imports (contract 10.7, 11, 11.5 and 25.6): the battle plugin (ez_battle_d.elf), the menu plugin
(ez_menu_d.elf), the UI plugin (ez_ui_d.elf), the PC plugin (ez_pc_d.elf), the casino plugin (ez_casino_d.elf, C4-C), the link plugin (C6) and the ending plugin (ez_ending_d.elf, C9).

Every symbol a plugin leaves undefined must be provided either by the firmware API
(~/.ufbt/current/sdk_headers/.../api_symbols.csv) or by the app's table (nucleo/app_api_table_i.h), which all four plugins
share; entries of the table that NO plugin uses are reported too (dead weight in the app's rodata, 8 bytes each). It also prints each
plugin's load pre-check (plugin_loader.c: sections + relocation data + bookkeeping + the scene's margin) from the built .fal files.
Usage: python3 tools/check_plugin_api.py [build dir]   (after `cd nucleo && ufbt build`; the dir defaults to ~/.ufbt/build, tools/mem_budget.sh keeps a copy of
each mode's build in build/mem_budget/elf_release and elf_dev: run it on both, the release and the dev build (NUCLEO_DEBUG=1) must pass)"""
import re, struct, subprocess, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
import os

HOME = pathlib.Path(
    os.environ.get("UFBT_HOME") or (pathlib.Path.home() / ".ufbt")
)  # UFBT_HOME: a private ufbt state dir (parallel work trees)
BUILD = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else HOME / "build"
ELFS = {
    name: BUILD / f"ez_{name}_d.elf"
    for name in ("battle", "menu", "ui", "pc", "casino", "link", "ending")
}
CSV = HOME / "current/sdk_headers/f7_sdk/targets/f7/api_symbols.csv"
TABLE = ROOT / "nucleo/app_api_table_i.h"
NM = next((HOME / "toolchain").glob("*/bin/arm-none-eabi-nm"), None)
IGNORE = {
    "uxTopUsedPriority"
}  # kept alive by the SDK linker flags, never relocated against

LOADER = ROOT / "nucleo/plugin_loader.c"


def load_need(fal):
    """What plugin_loader.c's pre-check reads from the .fal's section headers (elf_need): sum of the ALLOC sections (4-aligned), sum of the
    non-ALLOC PROGBITS (.fast.rel.* + .fapmeta + .gnu_debuglink), the biggest ALLOC section.
    """
    d = fal.read_bytes()
    shoff, n = struct.unpack_from("<I", d, 32)[0], struct.unpack_from("<H", d, 48)[0]
    ram = reloc = block = 0
    for i in range(n):
        ty, fl, _ad, _of, sz = struct.unpack_from("<IIIII", d, shoff + 40 * i + 4)
        if fl & 2:
            ram += (sz + 3) & ~3
            block = max(block, sz)
        elif ty == 1:
            reloc += sz
    return ram, reloc, block


def report_sizes():
    """The loader's pre-check per plugin (contract 10.7): sections + relocation data + LOADER_BOOKKEEPING + the scene's margin."""
    src = LOADER.read_text()
    bk = int(re.search(r"#define LOADER_BOOKKEEPING (\d+)u", src).group(1))
    margin = {}
    for name, ver in (
        ("battle", "BATTLE"),
        ("menu", "MENU"),
        ("ui", "UI"),
        ("pc", "PC"),
        ("casino", "CASINO"),
        ("link", "LINK"),
        ("ending", "ENDING"),
    ):
        m = re.search(ver + r"_PLUGIN_API_VERSION, ([\d* ]+)\}", src)
        margin[name] = eval(m.group(1)) if m else 0
    for name in ("battle", "menu", "ui", "pc", "casino", "link", "ending"):
        fal = BUILD / f"ez_{name}.fal"
        if not fal.exists():
            continue
        ram, reloc, block = load_need(fal)
        print(
            f"{name} plugin: sections {ram:,} + relocation data {reloc:,} + bookkeeping {bk:,} + scene margin {margin[name]:,} = pre-check needs {ram + reloc + bk + margin[name]:,} B free (.text block {block:,})"
        )


def main():
    if NM is None or not all(e.exists() for e in ELFS.values()):
        sys.exit(
            f"missing {[str(e) for e in ELFS.values() if not e.exists()]} or the toolchain: run `cd nucleo && ufbt build` first"
        )
    fw = set()
    for line in CSV.read_text().splitlines():
        p = line.split(",")
        if len(p) > 2 and p[0] in ("Function", "Variable") and p[1] == "+":
            fw.add(p[2])
    table = set(re.findall(r"API_(?:METHOD|VARIABLE)\((\w+)", TABLE.read_text()))
    ok = True
    used = set()
    for name, elf in ELFS.items():
        undef = {
            l.split()[1]
            for l in subprocess.check_output(
                [str(NM), "-u", str(elf)], text=True
            ).splitlines()
            if l.strip()
        }
        undef -= IGNORE
        used |= undef
        missing = sorted(undef - fw - table)
        print(
            f"{name} plugin imports {len(undef)} symbols: {len(undef & fw)} from the firmware, {len(undef & table)} from the app table"
        )
        if missing:
            ok = False
            print(
                f"MISSING from app_api_table_i.h (the {name} plugin would fail to load):",
                " ".join(missing),
            )
    unused = sorted(table - used)
    shadow = sorted(table & fw)
    if unused:
        print("table entries no plugin uses:", " ".join(unused))
    if shadow:
        print("table entries that the firmware API also exports:", " ".join(shadow))
    report_sizes()
    print("OK" if ok else "FAILED")
    sys.exit(0 if ok else 1)


main()
