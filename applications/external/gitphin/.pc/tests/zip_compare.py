#!/usr/bin/env python3
"""Compare two zip files entry by entry: names, unix mode, symlink flag, content.
Usage: zip_compare.py A.zip B.zip      exit 0 when equal; prints the differences.
Folder entries are compared by name and mode; dates and the zip comment are ignored.
An entry without unix mode counts as 0644 (file) or 0755 (folder)."""

import stat
import sys
import zipfile


def load(path):
    out = {}
    with zipfile.ZipFile(path) as z:
        bad = z.testzip()
        if bad:
            print(f"{path}: corrupt entry {bad}")
            sys.exit(2)
        for i in z.infolist():
            mode = i.external_attr >> 16
            if (
                mode == 0
            ):  # git archive stores a unix mode only for executables and symlinks
                mode = (stat.S_IFDIR | 0o755) if i.is_dir() else (stat.S_IFREG | 0o644)
            out[i.filename] = (
                stat.S_IMODE(mode),
                stat.S_IFMT(mode),
                z.read(i) if not i.is_dir() else b"",
            )
    return out


a, b = load(sys.argv[1]), load(sys.argv[2])
bad = 0
for name in sorted(set(a) | set(b)):
    if name not in a or name not in b:
        print(f"only in {'second' if name not in a else 'first'}: {name}")
        bad += 1
    elif a[name] != b[name]:
        print(
            f"differs: {name}: mode {oct(a[name][0])}/{oct(b[name][0])} type {oct(a[name][1])}/{oct(b[name][1])} "
            f"content {'same' if a[name][2] == b[name][2] else 'different'}"
        )
        bad += 1
print(f"{len(a)} vs {len(b)} entries, {bad} difference(s)")
sys.exit(1 if bad else 0)
