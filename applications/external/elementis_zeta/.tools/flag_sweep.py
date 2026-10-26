#!/usr/bin/env python3
"""GCC size-flag sweep for the device build (contract 44). Measures what each `-fno-<pass>` (every pass that -Os still runs) does to the size of a translation unit,
with the EXACT command line ufbt uses, so a flag set can be chosen per file (a `#pragma GCC optimize` at the top of a resident file, application.fam's PLUGIN_* lists
for the plugins' unity units) with a measured, not guessed, gain.

  cd nucleo && ufbt cdb                         writes nucleo/.vscode/compile_commands.json (release flags; delete nucleo/.vscode afterwards)
  python3 tools/flag_sweep.py sweep  CDB OUT.json [REGEX]     per file: size with no extra flag and with each -fno-X (about 115 compiles per file, ~20 min for the whole app, 8 threads)
  python3 tools/flag_sweep.py file   CDB SWEEP.json REGEX     per file (outputs matching REGEX): greedy flag set from the sweep's negative flags, one file at a time -> the line to paste
  python3 tools/flag_sweep.py module CDB SWEEP.json PREFIX [EXCLUDE_REGEX]   one flag set for all files of a module (output path prefix, e.g. nucleo_ui), summed
For a unity plugin build give a CDB whose command points at lib/<plugin>/<plugin>_unity.c plus -fwhole-program (see the contract) and `sweep` it.
Sizes are `arm-none-eabi-size` totals of the object (text + data + bss): the linked ELF differs by alignment and literal-pool sharing, so confirm with `bash tools/mem_budget.sh`.
NEVER give a per-pixel loop (blit, frame compose, render, the battle painters) a loop flag (-fno-move-loop-invariants, -fno-tree-loop-*, -fno-expensive-optimizations ...) without
measuring the frame on the device."""
import json, sys, shlex, subprocess, os, re, hashlib
from concurrent.futures import ThreadPoolExecutor

T = os.path.expanduser("~/.ufbt/toolchain/arm64-darwin/bin/")
SKIP = {
    "function-sections",
    "data-sections",
    "common",
    "strict-aliasing",
    "var-tracking",
    "var-tracking-assignments",
    "gcse-lm",
    "ident",
    "verbose-asm",
    "asynchronous-unwind-tables",
    "unwind-tables",
    "exceptions",
    "stack-protector",
    "tree-vectorize",
    "fp-int-builtin-inexact",
    "math-errno",
    "trapping-math",
    "signed-zeros",
    "rounding-math",
    "associative-math",
    "reciprocal-math",
    "finite-math-only",
    "semantic-interposition",
    "plt",
    "pic",
    "pie",
    "lto",
    "fat-lto-objects",
    "zero-initialized-in-bss",
    "builtin",
    "dwarf2-cfi-asm",
    "gnu-unique",
    "keep-static-consts",
    "leading-underscore",
    "short-enums",
    "show-column",
    "split-ivs-in-unroller",
    "stdarg-opt",
    "tree-slp-vectorize",
    "reg-struct-return",
    "prefetch-loop-arrays",
}


def key(e):
    return e["output"].replace(os.path.expanduser("~/.ufbt/build/"), "")


def build(e, extra, work):
    a = shlex.split(e["command"])
    i = a.index("-o")
    tag = hashlib.md5((key(e) + " ".join(extra)).encode()).hexdigest()[:12]
    a[i + 1] = f"{work}/{tag}.o"
    r = subprocess.run(a + extra, cwd=e["directory"], capture_output=True, text=True)
    if r.returncode:
        return None
    s = int(
        subprocess.run(
            [T + "arm-none-eabi-size", f"{work}/{tag}.o"],
            capture_output=True,
            text=True,
        )
        .stdout.split("\n")[1]
        .split()[3]
    )
    os.remove(f"{work}/{tag}.o")
    return s


def flags_at_os():
    out = subprocess.run(
        f"echo | {T}arm-none-eabi-gcc -Os -Q --help=optimizers -x c - 2>/dev/null",
        shell=True,
        capture_output=True,
        text=True,
    ).stdout
    fl = [
        l.split()[0][2:]
        for l in out.splitlines()
        if "[enabled]" in l and l.split()[0].startswith("-f")
    ]
    return [f for f in fl if f not in SKIP and not f.startswith("no-")]


def main():
    mode, cdb_path, third = sys.argv[1], sys.argv[2], sys.argv[3]
    cdb = json.load(open(cdb_path))
    work = os.path.abspath("build/flag_sweep")
    os.makedirs(work, exist_ok=True)
    skip_names = ("plugin_stub", "app_api_table")
    if mode == "sweep":
        flt = re.compile(sys.argv[4]) if len(sys.argv) > 4 else None
        ents = [
            e
            for e in cdb
            if not any(x in e["file"] for x in skip_names)
            and (not flt or flt.search(key(e)))
        ]
        flags = flags_at_os()
        jobs = [(e, f) for e in ents for f in [""] + flags]
        res = {}
        with ThreadPoolExecutor(8) as ex:
            for (e, f), v in zip(
                jobs,
                ex.map(
                    lambda j: build(j[0], [] if j[1] == "" else [f"-fno-{j[1]}"], work),
                    jobs,
                ),
            ):
                res.setdefault(key(e), {})[f] = v
        json.dump(res, open(third, "w"), indent=1)
        print(len(ents), "files", len(flags), "flags ->", third)
        return
    sw = json.load(open(third))
    if mode == "file":
        sel = re.compile(sys.argv[4])
        ents = [e for e in cdb if key(e) in sw and sel.search(key(e))]

        def one(e):
            v = sw[key(e)]
            b = v[""]
            cand = [
                f
                for f, x in sorted(
                    v.items(),
                    key=lambda t: (t[1] if t[1] is not None else 1e9) if t[0] else -1,
                )
                if f and x is not None and x < b
            ][:10]
            cur, best = [], b
            for f in cand:
                s = build(e, [f"-fno-{x}" for x in cur + [f]], work)
                if s is not None and s < best:
                    cur.append(f)
                    best = s
            return key(e), b, best, cur

        with ThreadPoolExecutor(8) as ex:
            for k, b, best, cur in ex.map(one, ents):
                print(
                    f"{k:34s} {b:6d} -> {best:6d} ({best - b:+d})  #pragma GCC optimize(\"{','.join('no-' + x for x in cur)}\")"
                )
    elif mode == "module":
        prefix = sys.argv[4]
        exc = re.compile(sys.argv[5]) if len(sys.argv) > 5 else None
        ents = [
            e
            for e in cdb
            if key(e) in sw
            and key(e).split("/")[0] == prefix
            and not (exc and exc.search(key(e)))
        ]
        tot = {}
        for e in ents:
            for f, x in sw[key(e)].items():
                if f and x is not None:
                    tot[f] = tot.get(f, 0) + x - sw[key(e)][""]
        base = sum(sw[key(e)][""] for e in ents)
        cur, best = [], base

        def total(fl):
            with ThreadPoolExecutor(8) as ex:
                return list(
                    ex.map(lambda e: build(e, [f"-fno-{x}" for x in fl], work), ents)
                )

        for f in [f for f, d in sorted(tot.items(), key=lambda t: t[1]) if d < 0]:
            res = total(cur + [f])
            if None in res:
                continue
            if sum(res) < best:
                cur.append(f)
                best = sum(res)
        print(
            prefix,
            "base",
            base,
            "->",
            best,
            f"({best - base:+d})",
            [f"-fno-{x}" for x in cur],
        )


main()
