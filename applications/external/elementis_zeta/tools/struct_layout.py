#!/usr/bin/env python3
"""Struct field order for 16-bit Thumb load / store encodings (contract 77).

On the Cortex-M4 a byte / half-word / word load or store with an immediate offset below 32 / 64 / 124 is a 16-bit instruction (low registers); above that it is a 32-bit one. A struct whose
hot fields sit past those offsets costs 2 B per access: ordering the fields by how often the code touches them gave the battle peak back 1.1 KB (contract section 77).

  cd nucleo && ufbt cdb writes nucleo/.vscode/compile_commands.json (delete nucleo/.vscode afterwards)
  python3 tools/struct_layout.py offsets INCLUDE STRUCT FIELDS SRCS [dev]
        the ARM offset / size of each field of STRUCT (INCLUDE = the header or .c that defines it, FIELDS = comma separated, "auto" = parsed from the source of INCLUDE) and how often
        SRCS (comma separated, repo-relative) mention it; fields that sit too far for their size are marked WIDE xN (N = references). Example:
            python3 tools/struct_layout.py offsets world.h World auto nucleo/world.c,nucleo/natives.c
  python3 tools/struct_layout.py wide ELF [N] the N fields (by name) behind the most 32-bit loads / stores in a LINKED elf (build/mem_budget/elf_dev/*.elf; needs the elf's line info)
A struct is reordered by hand (hot bytes first, then half-words, then words, big blocks last); keep a field's comment with it. Re-measure with `bash tools/mem_budget.sh`.
Never reorder a struct that is SAVED verbatim (Mon, Party, PartyState, the box and link records): its layout is the file format."""
import collections
import json
import os
import re
import shlex
import subprocess
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
HOME = os.environ.get("UFBT_HOME") or os.path.expanduser("~/.ufbt")
TC = HOME + "/toolchain/arm64-darwin/bin/"


def struct_fields(path, name):
    """The field names of `} name;` / `struct name {` (anonymous unions / structs flattened)."""
    src = open(path).read()
    m = re.search(r"\}\s*" + re.escape(name) + r"\s*;", src)
    if m:
        depth, i = 0, m.start()
        while True:
            i -= 1
            if src[i] == "}":
                depth += 1
            elif src[i] == "{":
                if depth == 0:
                    break
                depth -= 1
        body = src[i + 1:m.start()]
    else:
        m = re.search(r"struct\s+" + re.escape(name) + r"\s*\{", src)
        j, depth = m.end(), 1
        while depth:
            depth += src[j] == "{"
            depth -= src[j] == "}"
            j += 1
        body = src[m.end():j - 1]
    body = re.sub(r"/\*.*?\*/", "", re.sub(r"//[^\n]*", "", body), flags=re.S)
    body = re.sub(r"^\s*#.*$", "", body, flags=re.M)
    out, depth, stmt = [], 0, ""
    for ch in body:
        if ch in "{}":
            depth += 1 if ch == "{" else -1
            stmt = ""
        elif ch == ";":
            decl = re.sub(r"\[[^\]]*\]", "", stmt)
            stmt = ""
            for part in decl.split(","):
                t = re.findall(r"[A-Za-z_]\w*", part)
                if t and part.strip():
                    out.append(t[-1])
        else:
            stmt += ch
    return out


def cmd_offsets(args):
    inc, typ, fields, srcs = args[:4]
    dev = "dev" in args[4:]
    if fields == "auto":
        fields = ",".join(struct_fields(os.path.join(ROOT, "nucleo", inc), typ))
    cdb = json.load(open(ROOT + "/nucleo/.vscode/compile_commands.json"))
    ent = next(e for e in cdb if e["file"].endswith("/scene_world.c"))
    a = shlex.split(ent["command"])
    i = a.index("-o")
    tmp = ROOT + "/build/struct_layout_tmp.c"
    os.makedirs(ROOT + "/build", exist_ok=True)
    with open(tmp, "w") as f:
        f.write(f'#include "{inc}"\n#include <stddef.h>\nconst unsigned long OFS[] = {{\n')
        for n in fields.split(","):
            f.write(f"  offsetof({typ}, {n}), sizeof((({typ}*)0)->{n}),\n")
        f.write(f"  sizeof({typ})}};\n")
    cmd = [x for x in a[:i] + ["-S", "-o", ROOT + "/build/struct_layout_tmp.s"] + a[i + 2:-1] if x != "-c"] + (["-DNUCLEO_DEBUG"] if dev else []) + [tmp]
    r = subprocess.run(cmd, cwd=ROOT + "/nucleo", capture_output=True, text=True)
    if r.returncode:
        sys.exit(r.stderr[-2500:])
    vals = [int(x) for x in re.findall(r"\.word\s+(\d+)", re.search(r"OFS:\n((?:\s*\.word\s+\d+\n)+)", open(ROOT + "/build/struct_layout_tmp.s").read()).group(1))]
    code = "".join(open(os.path.join(ROOT, s)).read() for s in srcs.split(","))
    print("sizeof", vals[-1])
    for k, n in enumerate(fields.split(",")):
        off, sz = vals[2 * k], vals[2 * k + 1]
        refs = len(re.findall(r"(?:->|\.)" + re.escape(n) + r"\b", code))
        wide = (sz == 1 and off > 31) or (sz == 2 and off > 62) or (sz == 4 and off > 124)
        print("%5d %5d  %-18s refs %3d%s" % (off, sz, n, refs, ("  WIDE x%d" % refs) if wide and refs else ""))


def cmd_wide(args):
    elf = args[0]
    top = int(args[1]) if len(args) > 1 else 40
    lines = subprocess.run([TC + "arm-none-eabi-objdump", "-d", "-l", "--no-show-raw-insn", elf], capture_output=True, text=True).stdout.splitlines()
    loc = re.compile(r"^(/\S+):(\d+)")
    pat = re.compile(r"^\s*[0-9a-f]+:\s+(ldr|str)(b|h|sb|sh)?\.w\s+(\w+), \[(\w+), #(\d+)\]")
    low = {"r%d" % k for k in range(8)}
    cache, cur, cnt, n = {}, None, collections.Counter(), 0
    for l in lines:
        m = loc.match(l)
        if m:
            cur = (m.group(1), int(m.group(2)))
            continue
        m = pat.match(l)
        if not m or not cur:
            continue
        _op, sz, rt, rn, imm = m.groups()
        lim = {None: 124, "b": 31, "h": 62}.get(sz, -1)
        if rn in ("sp", "pc") or lim < 0 or int(imm) <= lim or rt not in low or rn not in low:
            continue
        n += 1
        if cur[0] not in cache:
            cache[cur[0]] = open(cur[0], errors="replace").read().split("\n") if os.path.exists(cur[0]) else []
        txt = cache[cur[0]][cur[1] - 1] if 0 < cur[1] <= len(cache[cur[0]]) else ""
        names = set(re.findall(r"(?:->|\.)(\w+)", txt))
        for nm in names:
            cnt[nm] += 1.0 / len(names)
    print("32-bit loads / stores that a smaller offset would make 16-bit:", n)
    for nm, c in cnt.most_common(top):
        print("%6.1f %s" % (c, nm))


if __name__ == "__main__":
    if len(sys.argv) < 3 or sys.argv[1] not in ("offsets", "wide"):
        sys.exit(__doc__)
    (cmd_offsets if sys.argv[1] == "offsets" else cmd_wide)(sys.argv[2:])
