#!/usr/bin/env python3
"""One-off converter: nucleo/gen/hero.h (pl2h.py output with the sprite arrays) -> nucleo/files/hero.bin (the arrays) and a
constants-only nucleo/gen/hero.h. The app keeps ONE facing (5 frames, 1.4 KB) in RAM and reads the others from hero.bin when
the hero turns (render.c hero_view) instead of holding all 7 KB of sprite data in rodata. tools/pl2h.py --bin writes the same
format directly, so this is only needed for a hero.h that still has the arrays.

hero.bin (little-endian): 16-byte header "NHRO", u8 version = 1, u8 W, u8 H, u8 BPR, u8 MBPR, u8 FRAMES, u8 DIRS, 5 reserved
zero bytes; then DIRS blocks of FRAMES * (BPR*H) pixel bytes followed by FRAMES * (MBPR*H) mask bytes."""
import re, sys, pathlib

ROOT = pathlib.Path(__file__).resolve().parent.parent
HDR = ROOT / "nucleo/gen/hero.h"
BIN = ROOT / "nucleo/files/hero.bin"


def parse_arrays(text):
    out = {}
    for name in ("frames", "mask"):
        m = re.search(r"pcharw_%s\[(\d+)\]\[(\d+)\]\[(\d+)\] = \{(.*?)\n\};" % name, text, re.S)
        if not m:
            sys.exit(f"no pcharw_{name} array in {HDR}")
        nd, nf, n = int(m[1]), int(m[2]), int(m[3])
        nums = [int(x) for x in re.findall(r"\d+", m[4])]
        if len(nums) != nd * nf * n:
            sys.exit(f"pcharw_{name}: {len(nums)} bytes, expected {nd * nf * n}")
        out[name] = (nd, nf, n, bytes(nums))
    return out


def main():
    text = HDR.read_text()
    d = {k: int(v) for k, v in re.findall(r"#define PCHARW_(\w+) (\d+)", text)}
    arr = parse_arrays(text)
    (nd, nf, nb, frames), (_, _, nm, mask) = arr["frames"], arr["mask"]
    assert (nd, nf) == (d["DIRS"], d["FRAMES"]) and nb == d["BPR"] * d["H"] and nm == d["MBPR"] * d["H"]
    blob = b"NHRO" + bytes([1, d["W"], d["H"], d["BPR"], d["MBPR"], nf, nd, 0, 0, 0, 0, 0])
    for k in range(nd):
        blob += frames[k * nf * nb:(k + 1) * nf * nb] + mask[k * nf * nm:(k + 1) * nf * nm]
    BIN.write_bytes(blob)
    keep = [l for l in text.splitlines() if l.startswith("#define") or l.startswith("//")]
    keep.insert(1, "// the sprite data lives in files/hero.bin (tools/hero_h2bin.py / pl2h.py --bin); render.c hero_view loads one facing at a time")
    HDR.write_text("\n".join(keep) + "\n")
    print(f"wrote {BIN} ({len(blob)} B: {nd} dirs x {nf} frames, {d['W']}x{d['H']}) and the constants-only {HDR}")


main()
