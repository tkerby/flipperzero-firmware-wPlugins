#!/usr/bin/env python3
"""C9 (contract 56): swaps the stand-in species of Iver's and Arkan's teams for C7's real ones.

Chapter 9 was built before chapter 7's species (Belugrand, Barnaclad, Frostiva, Archaeon: species 111 .. 133) were merged, so `assets/data/trainers.toml` names stand-ins on the lines marked
    # C7SWAP gobrawl>belugrand urchlord>barnaclad mesmaris>frostiva
(the role of each of these species is in the design notes). Once assets/data/species.toml has the target, this tool replaces `"<stand-in>"` by `"<target>"` on that
line only (the party list), and leaves the marker. It is idempotent and does nothing for a target that does not exist yet.

  python3 tools/c9_c7_swap.py apply (rewrites trainers.toml when something changed)
  python3 tools/c9_c7_swap.py --check report what would change, change nothing (exit 1 when a swap is pending and possible)
  python3 tools/c9_c7_swap.py --selftest
After it: python3 tools/regc.py (build_assets.sh does it); tools/host_world/c9_check.c checks Iver's and Arkan's teams by level and by the species that are not stand-ins, so it passes before and after.
"""
import re
import sys
from pathlib import Path

try:
    import tomllib
except ImportError:
    import tomli as tomllib

ROOT = Path(__file__).resolve().parent.parent
MARK = re.compile(r"#\s*C7SWAP\s+(.*)$")


def swap(text, species):
    out, done = [], []
    for ln in text.split("\n"):
        m = MARK.search(ln)
        if m and ln.lstrip().startswith("party"):
            body = ln[: m.start()]
            for pair in m.group(1).split():
                a, _, b = pair.partition(">")
                if b in species and f'"{a}"' in body:
                    body = body.replace(f'"{a}"', f'"{b}"')
                    done.append(f"{a} -> {b}")
            ln = body + ln[m.start() :]
        out.append(ln)
    return "\n".join(out), done


def species_of(path):
    return set(tomllib.load(open(path, "rb")))


def selftest():
    src = 'party = [["gobrawl", 54], ["sagatee", 54]]   # C7SWAP gobrawl>belugrand\n'
    a, d = swap(src, {"gobrawl", "sagatee"})
    assert a == src and not d, "no target yet: nothing changes"
    b, d = swap(src, {"gobrawl", "sagatee", "belugrand"})
    assert (
        '"belugrand"' in b and '"gobrawl"' not in b and d == ["gobrawl -> belugrand"]
    ), b
    c, d = swap(b, {"belugrand"})
    assert c == b and not d, "idempotent"
    e, d = swap('x = ["gobrawl"]   # C7SWAP gobrawl>belugrand\n', {"belugrand"})
    assert "gobrawl" in e and not d, "only party lines are rewritten"
    print("c9_c7_swap selftest: ok")


def main():
    if "--selftest" in sys.argv:
        selftest()
        return 0
    tp = ROOT / "assets/data/trainers.toml"
    text = tp.read_text(encoding="utf-8")
    new, done = swap(text, species_of(ROOT / "assets/data/species.toml"))
    for d in done:
        print("swap:", d)
    if not done:
        print(
            "nothing to swap (a target species is missing, or the stand-ins are already gone)"
        )
        return 0
    if "--check" in sys.argv:
        return 1
    tp.write_text(new, encoding="utf-8")
    print("assets/data/trainers.toml rewritten: run tools/regc.py")
    return 0


if __name__ == "__main__":
    sys.exit(main())
