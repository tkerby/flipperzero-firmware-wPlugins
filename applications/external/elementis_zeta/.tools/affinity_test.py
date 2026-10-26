#!/usr/bin/env python3
"""Test of the Solar / Lunar affinity data tools (contract 66): assets/data/affinity.toml, tools/regc.py (files/aff.bin) and tools/species_sources_check.py (the per-affinity
sources, the rules and the cross-affinity trade report).
  python3 -I tools/affinity_test.py        (exit status 1 on a failure; part of tools/build_assets.sh)"""
import contextlib
import copy
import io
import struct
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(Path(__file__).resolve().parent))
import regc  # noqa: E402
import species_sources_check as ssc  # noqa: E402

fails = 0


def check(what, cond):
    global fails
    print(("ok   " if cond else "FAIL ") + what)
    if not cond:
        fails += 1


def run(mod_load=None, mod_scan=None, argv=("x",)):
    """species_sources_check.main() with patched data; -> (exit code, output)."""
    old_load, old_scan, old_argv = ssc.load, ssc.scan_maps, sys.argv
    ssc.load = mod_load or old_load
    ssc.scan_maps = mod_scan or old_scan
    sys.argv = list(argv)
    out = io.StringIO()
    try:
        with contextlib.redirect_stdout(out):
            rc = ssc.main()
    finally:
        ssc.load, ssc.scan_maps, sys.argv = old_load, old_scan, old_argv
    return rc, out.getvalue()


def patched(load_edits=None):
    """a `load` that edits the toml documents: load_edits = {relative path: fn(doc)}."""

    def load(rel):
        doc = copy.deepcopy(ssc_load_real(rel))
        if load_edits and rel in load_edits:
            load_edits[rel](doc)
        return doc

    return load


ssc_load_real = ssc.load
scan_real = ssc.scan_maps

# ---- the real data ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------
sp = ssc_load_real("assets/data/species.toml")
aff = ssc_load_real("assets/data/affinity.toml")
pairs = aff["pair"]
rc, out = run()
check(f"the real data passes the checker ({out.strip()[:140]})", rc == 0)
rc, out = run(argv=("x", "-v"))
check(
    "-v prints the per-affinity trade report",
    "AFFINITY (contract 66)" in out
    and "SOLAR needs a Lunar friend for 12" in out
    and "LUNAR needs a Solar friend for 12" in out,
)
SOLAR_ONLY = [
    "coilisp",
    "hoodcoil",
    "mossbud",
    "mossbloom",
    "bloomoss",
    "ramplet",
    "ibexer",
    "cindermeer",
    "pyromeer",
    "razorfly",
    "scarabeam",
    "kheprion",
]
LUNAR_ONLY = [
    "dunedil",
    "dunedrake",
    "glintail",
    "lustrel",
    "pitlion",
    "flarechick",
    "pyreacock",
    "droselet",
    "drosaura",
    "droserath",
    "lunashell",
    "selentor",
]
lines = {
    ln.split(" needs")[0].strip(): ln for ln in out.splitlines() if " needs a " in ln
}
got_l = [x.strip() for x in lines["SOLAR"].split(": ", 1)[1].split(",")]
got_s = [x.strip() for x in lines["LUNAR"].split(": ", 1)[1].split(",")]
check(
    "the species each affinity needs a trade for are the 12 + 12 of tests/host_world/affinity_check.c, c9_check.c and the contract (a Solar player needs the Lunar ones and the other way round)",
    sorted(got_l) == sorted(LUNAR_ONLY) and sorted(got_s) == sorted(SOLAR_ONLY),
)
ids = {n: i + 1 for i, n in enumerate(sp)}
check(
    "the pairs: 12 in all, 10 wild + 2 final-stage counterparts (`wild = false`), every species in at most one, the emblems among them",
    len(pairs) == 12
    and sum(1 for p in pairs if p.get("wild", True)) == 10
    and len({p[k] for p in pairs for k in ("solar", "lunar")}) == 24
    and {"solar": "scarabeam", "lunar": "lunashell"}
    in [{k: p[k] for k in ("solar", "lunar")} for p in pairs],
)
check(
    "the 9 species that are exclusive inside ONE save (starters, fossils, Mimbara's stone forms) are unchanged and not pair members",
    not (
        {
            "embercub",
            "puddlet",
            "sprouton",
            "trilobite",
            "sealily",
            "tidebara",
            "embara",
        }
        & {p[k] for p in pairs for k in ("solar", "lunar")}
    ),
)
check(
    "no species is on both affinities' exclusive list and the two lists have the same size (12 = 10 existing pairs' lines + the 2 emblem lines)",
    not set(SOLAR_ONLY) & set(LUNAR_ONLY) and len(SOLAR_ONLY) == len(LUNAR_ONLY),
)

# ---- regc: aff.bin ------------------------------------------------------------------------------------------------------------------------------------------------------------------------
d = regc.parse()
b = regc.emit_affinity(d)
n = 1 + len(sp)
check(
    f"aff.bin: 'NAFF', version 1, {n} species, 12 pairs, size {len(b)} (12 + the n view bytes padded to 4)",
    b[:4] == b"NAFF"
    and b[4] == 1
    and b[5] == n
    and b[6] == 12
    and struct.unpack("<I", b[8:12])[0] == len(b) == 12 + (n + 3) // 4 * 4,
)
view = b[12 : 12 + n]
want = {ids[p["solar"]]: ids[p["lunar"]] for p in pairs if p.get("wild", True)}
check(
    "the Lunar view maps each Solar form of a WILD pair to its counterpart and everything else (the final-stage counterparts too) to ITSELF (a total view)",
    all(view[i] == want.get(i, i) for i in range(n)) and len(want) == 10,
)
check(
    "the file on disk is what regc writes",
    (ROOT / "nucleo" / "files" / "aff.bin").read_bytes() == b,
)


def bad_pairs(pr):
    try:
        old = regc.load_toml
        regc.load_toml = lambda name: (
            {"pair": pr} if name == "affinity.toml" else old(name)
        )
        try:
            regc.affinity_pairs(d["sid"])
        finally:
            regc.load_toml = old
        return None
    except regc.RegError as e:
        return str(e)


check(
    "regc refuses: an unknown species",
    "unknown species" in (bad_pairs([{"solar": "nope", "lunar": "dunedil"}]) or ""),
)
check(
    "regc refuses: a species in two pairs",
    "two pairs"
    in (
        bad_pairs(
            [
                {"solar": "coilisp", "lunar": "dunedil"},
                {"solar": "coilisp", "lunar": "ramplet"},
            ]
        )
        or ""
    ),
)
check(
    "regc refuses: a species paired with itself",
    "itself" in (bad_pairs([{"solar": "coilisp", "lunar": "coilisp"}]) or ""),
)
check(
    "regc refuses: a stray key",
    "keys are"
    in (bad_pairs([{"solar": "coilisp", "lunar": "dunedil", "extra": 1}]) or ""),
)

# ---- the checker's rules ---------------------------------------------------------------------------------------------------------------------------------------------------------------------
wild, direct, wild_sets = scan_real(set(sp))


def scan_with(wild_add=None, direct_add=None, sets_add=None, wild_drop=()):
    def scan(names):
        w = {k: list(v) for k, v in wild.items() if k not in wild_drop}
        dr = dict(direct)
        ws = dict(wild_sets)
        for k, v in (wild_add or {}).items():
            w[k] = v
        dr.update(direct_add or {})
        ws.update(sets_add or {})
        return w, dr, ws

    return scan


rc, out = run(mod_scan=scan_with(wild_add={"dunedil": ["ruta_4:grass"]}))
check(
    "a map table that lists a Lunar form is an error",
    rc == 1 and "LUNAR form of a pair" in out and "dunedil" in out,
)
rc, out = run(
    mod_scan=scan_with(direct_add={"droselet": ("give", "give_box in somewhere")})
)
check(
    "a pair member with a gift / give_box source is an error (it would reach both affinities)",
    rc == 1 and "non-wild source" in out and "droselet" in out,
)
rc, out = run(
    mod_scan=scan_with(direct_add={"scarabeam": ("trade", "trade in somewhere")})
)
check(
    "... also an in-game trade / prize / legend", rc == 1 and "non-wild source" in out
)
rc, out = run(mod_scan=scan_with(sets_add={"coilisp": "ruta_1"}))
check(
    "a scripted wild battle (`wild_set`) with a pair member is an error (it would be remapped)",
    rc == 1 and "scripted wild battle" in out,
)
rc, out = run(mod_scan=scan_with(wild_drop=("scarabeam",)))
check(
    "a wild pair whose Solar form is in no table is an error (the pair has no habitat)",
    rc == 1 and "no wild table lists it" in out and "scarabeam" in out,
)


def add_pair(doc):
    doc["pair"].append({"solar": "coilisp", "lunar": "jabbird"})


rc, out = run(mod_load=patched({"assets/data/affinity.toml": add_pair}))
check("a species in two pairs is an error", rc == 1 and "in two pairs" in out)


def new_orphan(doc):
    doc["orphanling"] = copy.deepcopy(doc["flitter"])
    doc["orphanling"].pop("evolve", None)


rc, out = run(mod_load=patched({"assets/data/species.toml": new_orphan}))
check(
    "a species with no source on either affinity is an error (NO SOURCE)",
    rc == 1 and "NO SOURCE for orphanling" in out,
)
# the pairs are what makes the Lunar forms obtainable (the tables list only the Solar ones): without the coilisp / dunedil pair nobody can get a Dunedil
rc, out = run(
    mod_load=patched(
        {"assets/data/affinity.toml": lambda doc: doc["pair"].__delitem__(0)}
    )
)
check(
    "without the coilisp / dunedil pair a Dunedil has no source at all (the tables list the Solar form only)",
    rc == 1 and "NO SOURCE for dunedil" in out,
)
print(f"affinity_test: {fails} failure(s)")
sys.exit(1 if fails else 0)
