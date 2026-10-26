#!/usr/bin/env python3
"""Reads back a packed 20q.dat and prints cells -- the ground truth check that
the file on the SD card says what the pipeline thinks it says.

  python3 tools/dump_dataset.py files/20q.dat banana dog hammer
"""
from __future__ import annotations

import struct
import sys


def load(path):
    d = open(path, "rb").read()
    assert d[:4] == b"F20Q", "bad magic"
    (
        ver,
        flags,
        n,
        q,
        matrix_off,
        ent_index_off,
        ent_text_off,
        qst_index_off,
        qst_text_off,
        prior_off,
        cat_off,
        qrank_off,
    ) = struct.unpack_from("<HHIIIIIIIIII", d, 4)
    lut = struct.unpack_from("<16f", d, 0x30)
    fam_off = struct.unpack_from("<I", d, 0x98)[0]

    def texts(index_off, text_off, count):
        idx = struct.unpack_from(f"<{count + 1}I", d, index_off)
        return [
            d[text_off + idx[i] : text_off + idx[i + 1]].decode() for i in range(count)
        ]

    names = texts(ent_index_off, ent_text_off, n)
    questions = texts(qst_index_off, qst_text_off, q)
    stride = (n + 1) // 2

    def cell(i, j):
        b = d[matrix_off + j * stride + (i >> 1)]
        return lut[(b >> 4) if (i & 1) else (b & 0x0F)]

    qrank = struct.unpack_from(f"<{q}H", d, qrank_off)
    cats = d[cat_off : cat_off + n]
    fams = d[fam_off : fam_off + q]
    return dict(
        n=n,
        q=q,
        names=names,
        questions=questions,
        cell=cell,
        lut=lut,
        qrank=qrank,
        cats=cats,
        fams=fams,
        flags=flags,
        version=ver,
    )


def main():
    path = sys.argv[1] if len(sys.argv) > 1 else "files/20q.dat"
    ds = load(path)
    print(f"{path}: {ds['n']} entities x {ds['q']} questions, flags {ds['flags']:#x}")
    wanted = sys.argv[2:] or ["dog"]
    for name in wanted:
        if name not in ds["names"]:
            print(f"  {name}: not in vocabulary")
            continue
        i = ds["names"].index(name)
        print(f"\n{name} (category {ds['cats'][i]})")
        cells = sorted(((ds["cell"](i, j), j) for j in range(ds["q"])), reverse=True)
        print("  most true:")
        for p, j in cells[:8]:
            print(f"    {p:.2f}  {ds['questions'][j]}")
        print("  most false:")
        for p, j in cells[-4:]:
            print(f"    {p:.2f}  {ds['questions'][j]}")


if __name__ == "__main__":
    main()
