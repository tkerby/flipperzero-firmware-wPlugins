#!/usr/bin/env python3
"""Decode a Gen 2 link party block (+ patch list) into a canonical text dump.

Independent of pkb_core on purpose: offsets and the charset below are written
out by hand from pokecrystal (engine/link/link.asm, macros/ram.asm) so the
host test can cross-check the C party-block builder field by field.

Usage:
  decode_party.py BLOCK.bin PATCH.bin [--expect EXPECTED.txt]

BLOCK.bin may be either the 450-byte block as sent or raw received bytes
(leading 0/$FD/$FE are skipped and every $FE dropped, like the Game Boy).
"""
import argparse
import sys

PREAMBLE, NO_DATA, PATCH_END = 0xFD, 0xFE, 0xFF
NAME_LEN, STRUCT_LEN, PARTY = 11, 48, 6
DATA_LEN = NAME_LEN + 8 + 2 + PARTY * (STRUCT_LEN + 2 * NAME_LEN)  # 441
PATCH_DATA_SIZE = 0xFC

CHARS = {
    0x7F: " ",
    0x9A: "(",
    0x9B: ")",
    0x9C: ":",
    0x9D: ";",
    0x9E: "[",
    0x9F: "]",
    0xE0: "'",
    0xE3: "-",
    0xE6: "?",
    0xE7: "!",
    0xE8: ".",
    0xEF: "M",
    0xF3: "/",
    0xF4: ",",
    0xF5: "F",
}
CHARS.update({0x80 + i: chr(ord("A") + i) for i in range(26)})
CHARS.update({0xA0 + i: chr(ord("a") + i) for i in range(26)})
CHARS.update({0xF6 + i: chr(ord("0") + i) for i in range(10)})


def gb_text(bs):
    out = []
    for b in bs:
        if b == 0x50:
            break
        out.append(CHARS.get(b, "?"))
    return "".join(out)


def unpack(block, patch):
    i = 0
    while i < len(block) and block[i] in (0, PREAMBLE, NO_DATA):
        i += 1
    data = bytearray(b for b in block[i:] if b != NO_DATA)[:DATA_LEN]
    if len(data) < DATA_LEN:
        raise SystemExit(f"party data too short: {len(data)} < {DATA_LEN}")
    region = NAME_LEN + 8  # patched region starts at the trainer ID
    base, parts = 0, 2
    for b in patch:
        if not parts:
            break
        if b in (0, PREAMBLE, NO_DATA):
            continue
        if b == PATCH_END:
            base, parts = PATCH_DATA_SIZE, parts - 1
            continue
        data[region + base + b - 1] = NO_DATA
    return bytes(data)


def be(bs):
    v = 0
    for b in bs:
        v = (v << 8) | b
    return v


def decode(data):
    lines = [f"trainer {gb_text(data[0:NAME_LEN])}"]
    count = data[NAME_LEN]
    pid = be(data[NAME_LEN + 8 : NAME_LEN + 10])
    lines += [f"id {pid}", f"count {count}"]
    mons = NAME_LEN + 10
    ots = mons + PARTY * STRUCT_LEN
    nicks = ots + PARTY * NAME_LEN
    for n in range(count):
        m = data[mons + n * STRUCT_LEN : mons + (n + 1) * STRUCT_LEN]
        species = m[0]
        listed = data[NAME_LEN + 1 + n]
        lines.append(
            f"mon {n + 1} species {species} listed {listed}"
            f" nick {gb_text(data[nicks + n * NAME_LEN: nicks + (n + 1) * NAME_LEN])}"
            f" ot {gb_text(data[ots + n * NAME_LEN: ots + (n + 1) * NAME_LEN])}"
            f" otid {be(m[6:8])} level {m[31]} exp {be(m[8:11])} item {m[1]}"
            f" hp {be(m[34:36])} maxhp {be(m[36:38])}"
            f" atk {be(m[38:40])} def {be(m[40:42])} spd {be(m[42:44])}"
            f" sat {be(m[44:46])} sdf {be(m[46:48])}"
            f" moves {m[2]} {m[3]} {m[4]} {m[5]} pp {m[23]} {m[24]} {m[25]} {m[26]}"
            f" dvs {be(m[21:23])}"
            f" statexp {' '.join(str(be(m[11 + 2 * k:13 + 2 * k])) for k in range(5))}"
            f" happiness {m[27]} status {m[32]}"
        )
    return "\n".join(lines) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("block")
    ap.add_argument("patch")
    ap.add_argument("--expect")
    args = ap.parse_args()
    with open(args.block, "rb") as f:
        block = f.read()
    with open(args.patch, "rb") as f:
        patch = f.read()
    text = decode(unpack(block, patch))
    if not args.expect:
        sys.stdout.write(text)
        return
    with open(args.expect, encoding="ascii") as f:
        want = f.read()
    if text != want:
        for i, (a, b) in enumerate(zip(text.splitlines(), want.splitlines())):
            if a != b:
                print(f"line {i + 1} differs:\n  decoded {a}\n  expected {b}")
        sys.exit("decode_party: mismatch")
    print("decode_party: OK,", text.count("\nmon "), "mons agree")


if __name__ == "__main__":
    main()
