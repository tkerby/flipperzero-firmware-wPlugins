#!/usr/bin/env python3
"""Fail if the FAP's loaded size (text + data + bss) exceeds the budget.

FAPs are loaded into RAM, so the budget is 100 KiB for code and data.
The loader also mallocs each allocated section on its own and needs a free
block of its size + 1 KiB (about 62 KiB is free in one piece on a stock
Flipper), so no single section may exceed --section-limit.
Uses arm-none-eabi-size from the ufbt toolchain (~/.ufbt/toolchain).

Usage: size_gate.py [--limit KIB] [--section-limit KIB] [ELF]
"""
import argparse
import pathlib
import subprocess
import sys


def find_tool():
    for root in (pathlib.Path.home() / ".ufbt", pathlib.Path(".ufbt")):
        for p in root.glob("toolchain/*/bin/arm-none-eabi-size*"):
            return p
    sys.exit("arm-none-eabi-size not found (install ufbt's toolchain)")


def find_elf():
    for pattern in ("dist/debug/*.elf", ".ufbt/build/*_d.elf"):
        hits = sorted(pathlib.Path(".").glob(pattern))
        if hits:
            return hits[0]
    sys.exit("no FAP ELF found; build with ufbt first")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("elf", nargs="?")
    ap.add_argument("--limit", type=int, default=100, help="KiB")
    ap.add_argument("--section-limit", type=int, default=48, help="KiB per section")
    args = ap.parse_args()
    elf = pathlib.Path(args.elf) if args.elf else find_elf()
    out = subprocess.run(
        [str(find_tool()), "-B", str(elf)], check=True, capture_output=True, text=True
    ).stdout
    text, data, bss = (int(x) for x in out.splitlines()[1].split()[:3])
    total = text + data + bss
    print(
        f"{elf}: text {text}, data {data}, bss {bss}: {total} bytes "
        f"({total / 1024:.1f} of {args.limit} KiB)"
    )
    if total > args.limit * 1024:
        sys.exit("over budget")
    # size -A lists every section; the allocated ones are those with a size
    # that size -B counts (code, rodata, data, bss), i.e. not debug info.
    out = subprocess.run(
        [str(find_tool()), "-A", str(elf)], check=True, capture_output=True, text=True
    ).stdout
    over = False
    for line in out.splitlines():
        f = line.split()
        if (
            len(f) == 3
            and f[0].startswith(".")
            and f[1].isdigit()
            and not f[0].startswith(
                (
                    ".debug",
                    ".ARM",
                    ".comment",
                    ".rel",
                    ".fast",
                    ".fap",
                    ".gnu",
                    ".note",
                    ".stab",
                )
            )
        ):
            size = int(f[1])
            print(f"  {f[0]}: {size} bytes ({size / 1024:.1f} KiB)")
            if size > args.section_limit * 1024:
                print(f"  {f[0]} is over the {args.section_limit} KiB section limit")
                over = True
    if over:
        sys.exit("a section is over budget")


if __name__ == "__main__":
    main()
