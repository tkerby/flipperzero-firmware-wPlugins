#!/usr/bin/env python3
"""Every species of assets/data/species.toml must be obtainable (contract 58, 66): a wild table (grass / water / fishing), a gift (`gift=`, `give_box`), a prize
(casino), a legendary encounter (`legend=` / `sleeper=`), a trade (`trade <give> <get>`: the creature received), a fossil (the lab's `give_box`), one of
Lin's three starters, or an EVOLUTION of a species that has one (level, stone, trade or friendship). Genesa (Lin's Registro gift) is excluded from the requirement.
So the Registro can be completed (with an IR trade partner for the exclusive choices: the two other starter lines, the other dojo gift, the other fossil).

AFFINITY (contract 66; assets/data/affinity.toml): the wild tables list the SOLAR form of each pair and a LUNAR player meets its counterpart in the same slot. The check understands both:
  * a species is obtainable when it is obtainable on at least ONE affinity (the other half of the Registro comes by an IR trade with a player of the other affinity: the user approved that
    completing the Registro needs one);
  * errors: a map table that lists a Lunar form; a pair member with any source but the wild table (a gift, give_box, trade, prize, legend, sleeper, starter, or named by a `wild_set`
    script), because that source would give it to both affinities; a `wild` pair whose Solar form is in no table; a species in two pairs;
  * the report (-v) lists, per affinity, the species that save can NOT get without a trade with the other affinity (the exclusives and their evolution lines).
  python3 tools/species_sources_check.py [-v]     exit 1 and the list of species without a source / the broken rules."""
import glob
import os
import re
import sys
import tomllib

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
STARTERS = {"embercub", "puddlet", "sprouton"}      # villa_lab.map: Lin's three bags (`setv starter 1 / 3 / 5`); one is taken, the others come by trade
EXEMPT = {"genesa"}                                  # the mythical one: excluded from the Registro's completion and from the wild requirement. Its source (D1, contract 68) is ONE scripted legendary fight, `wild_set genesa 178` in the Ruinas del Origen (it was Lin's give_box gift in contract 56): `main` checks that it still has a source
AFFS = ("solar", "lunar")


def load(rel):
    with open(os.path.join(ROOT, rel), "rb") as f:
        return tomllib.load(f)


def scan_maps(names):
    """-> (wild, direct, wild_sets): wild = {species: [map:table]} from every @encounters table of maps/*.map (test maps excluded); direct = {species: (kind, where)} of the non-wild sources;
    wild_sets = {species: where} named by a `wild_set` script line (a scripted wild battle: it goes through the affinity remap too)."""
    wild, direct, wild_sets = {}, {}, {}
    for p in sorted(glob.glob(os.path.join(ROOT, "maps", "*.map"))):
        base = os.path.basename(p)[:-4]
        if base.startswith("test_"):
            continue
        inenc, kind = False, "grass"
        for ln in open(p, encoding="utf-8"):
            code = ln.split("#")[0].strip()
            if code.startswith("@encounters"):
                inenc = True
                kind = code.split()[1] if len(code.split()) > 1 else "grass"
                continue
            if code.startswith("@end"):
                inenc = False
                continue
            if inenc:
                tok = code.split()
                if len(tok) >= 3 and tok[0] in names:
                    wild.setdefault(tok[0], []).append(f"{base}:{kind}")
                continue
            for m in re.finditer(r"\b(gift|legend|sleeper)=([a-z_]+)", code):
                if m.group(2) in names:
                    direct.setdefault(m.group(2), ("gift" if m.group(1) == "gift" else "legend", f"{m.group(1)}= of {base}"))
            m = re.match(r"give(?:_box)?\s+([a-z_]+)", code)
            if m and m.group(1) in names:
                direct.setdefault(m.group(1), ("give", f"give_box in {base}"))
            m = re.match(r"trade\s+([a-z_]+)\s+([a-z_]+)", code)
            if m and m.group(2) in names:
                direct.setdefault(m.group(2), ("trade", f"trade in {base}"))
            m = re.match(r"wild_set\s+([a-z_]+)", code)
            if m and m.group(1) in names:
                wild_sets.setdefault(m.group(1), base)
    for shop in load("assets/data/shops.toml").values():
        for pr in shop.get("prizes", []):
            if pr.get("species") in names:
                direct.setdefault(pr["species"], ("prize", "casino prize"))
    return wild, direct, wild_sets


def evo_targets(d):
    tos = []
    if "evolve" in d:
        tos.append(d["evolve"][1])
    if "evolve_friend" in d:                 # contract 65: level-up with friendship >= 220
        tos.append(d["evolve_friend"][1])
    tos += list(d.get("evolve_stone", {}).values())
    if "evolve_trade" in d:
        tos.append(d["evolve_trade"])
    return tos


def closure(sp, base):
    """The evolution closure of a set of species."""
    out, todo = set(), list(base)
    while todo:
        x = todo.pop()
        if x in out or x not in sp:
            continue
        out.add(x)
        todo += evo_targets(sp[x])
    return out


def line(sp, n):
    return closure(sp, [n])


def main():
    verbose = "-v" in sys.argv
    sp = load("assets/data/species.toml")
    names = set(sp)
    pairs = load("assets/data/affinity.toml").get("pair", [])
    wild, direct, wild_sets = scan_maps(names)
    errors = []

    # ---- the pairs
    seen, solar, lunar, view = {}, set(), set(), {a: {} for a in AFFS}   # view[aff][species in a table] -> the species that affinity meets
    for k, pr in enumerate(pairs):
        a, b = pr["solar"], pr["lunar"]
        for n in (a, b):
            if n not in names:
                errors.append(f"affinity.toml pair {k}: unknown species {n}")
            if n in seen:
                errors.append(f"affinity.toml: {n} is in two pairs")
            seen[n] = k
        if not pr.get("wild", True):
            continue
        solar.add(a)
        lunar.add(b)
        view["lunar"][a] = b
    for n in lunar:
        if n in wild:
            errors.append(f"{n} is the LUNAR form of a pair but a wild table lists it ({', '.join(wild[n][:3])}): list its Solar counterpart there")
    for a in solar:
        if a not in wild:
            errors.append(f"{a} is the Solar form of a wild pair but no wild table lists it (the pair {a} / {view['lunar'][a]} has no habitat)")
    members = set(seen)
    for n in sorted(members):
        if n in direct:
            errors.append(f"{n} is an affinity-pair member but has a non-wild source ({direct[n][1]}): that source gives it to both affinities")
        if n in STARTERS:
            errors.append(f"{n} is an affinity-pair member and a starter")
        if n in wild_sets:
            errors.append(f"{n} is an affinity-pair member but {wild_sets[n]}.map starts a scripted wild battle with it (it would be remapped)")

    # ---- what each affinity can get
    def sources(aff):
        got = {s: "starter (Lin)" for s in STARTERS}
        for s, where in wild.items():
            t = view[aff].get(s, s)
            got.setdefault(t, f"wild table of {where[0]}")
        for s, (_k, why) in direct.items():
            got.setdefault(s, why)
        for s in closure(sp, got):
            got.setdefault(s, "evolution")
        return got
    src = {a: sources(a) for a in AFFS}
    union = set(src["solar"]) | set(src["lunar"])
    for s in sp:
        if s not in union and s not in EXEMPT:
            errors.append(f"NO SOURCE for {s} (on neither affinity)")
    for n in sorted(EXEMPT):   # D1 (contract 68): the mythical species is not required to be wild, but it must keep ONE source (a scripted legendary fight, a gift, a legend NPC)
        if n not in wild_sets and n not in direct and n not in wild:
            errors.append(f"{n} (excluded from the Registro's completion) has no source at all: the Ruinas del Origen's `wild_set {n} <level + 128>` is gone")
    if verbose:
        for s in sp:
            on = "both" if s in src["solar"] and s in src["lunar"] else "SOLAR only" if s in src["solar"] else "LUNAR only" if s in src["lunar"] else "-- NO SOURCE --"
            print(f"{s:14s} {on:11s} {src['solar'].get(s) or src['lunar'].get(s, '')}")
        trade_report(sp, direct, wild, src)
    if errors:
        print("species_sources_check: FAILED")
        for e in errors:
            print("  " + e)
        return 1
    solar_only = sorted(s for s in union if s in src["solar"] and s not in src["lunar"] and s not in EXEMPT)
    lunar_only = sorted(s for s in union if s in src["lunar"] and s not in src["solar"] and s not in EXEMPT)
    print(f"species_sources_check: OK ({len(sp)} species, every one has a source on at least one affinity; {len(EXEMPT)} excluded (genesa: its source is the scripted legendary fight of {wild_sets.get('genesa', '-')}); {len(pairs)} affinity pairs: "
          f"{len(solar_only)} Solar-only and {len(lunar_only)} Lunar-only species, each needs a trade with the other affinity)")
    return 0


def trade_report(sp, direct, wild, src):
    """Read-only info (-v): which species a single save can NOT have without a second Flipper (the exclusive choices of ONE save, and the species of the other affinity)."""
    free = lambda n: n in wild or (n in direct and direct[n][0] in ("trade", "prize", "legend"))   # (a wild table, an in-game trade, a prize, a legendary: no choice involved)
    groups = {"starter (Lin's three bags)": sorted(STARTERS), "fossil (Monte Eco: one of two)": ["trilobite", "sealily"], "Mimbara's stone forms (one Mimbara)": ["tidebara", "embara"]}
    print("-- needs a second Flipper in ONE save (exclusive choices; report only, the Genesa rule is `every species but Genesa`)")
    total = 0
    for g, heads in groups.items():
        lines = [line(sp, h) for h in heads]
        bad = [h for h in heads if free(h)]
        extra = sum(len(x) for x in lines) - max(len(x) for x in lines)
        total += extra
        print(f"  {g}: " + " | ".join("/".join(sorted(x)) for x in lines) + f"   -> {extra} species by trade" + (f"   (but {', '.join(bad)} also has a free source)" if bad else ""))
    tev = sorted((n, d["evolve_trade"]) for n, d in sp.items() if "evolve_trade" in d)
    print("  trade evolutions: " + ", ".join(f"{a}->{b}" + (" (target also free: " + ("wild" if free(b) else "NO") + ")") for a, b in tev))
    print(f"  total {total} species of {len(sp) - 1} need trading in any one save")
    print("-- AFFINITY (contract 66): the species one affinity cannot get at all without a trade with the other affinity (report only)")
    for aff, other in (("solar", "lunar"), ("lunar", "solar")):
        need = sorted((s for s in sp if s in src[other] and s not in src[aff] and s not in EXEMPT), key=list(sp).index)
        print(f"  {aff.upper()} needs a {other.capitalize()} friend for {len(need)}: " + ", ".join(need))


if __name__ == "__main__":
    sys.exit(main())
