#!/usr/bin/env python3
"""Contract 64: every technique disc and every new item of the data can be OBTAINED: sold in a shop, a prize of the casino, a ground pouch (`item=`) or a gift (`give_item`).
Real maps only (not test_*). Run: python3 -I tools/disc_sources_check.py"""
import re
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
items = tomllib.load(open(ROOT / "assets/data/items.toml", "rb"))
shops = tomllib.load(open(ROOT / "assets/data/shops.toml", "rb"))
where = {}


def add(item, what):
    where.setdefault(item, []).append(what)


for name, sh in shops.items():
    for i in sh.get("items", []):
        add(i, "shop " + name)
    for p in sh.get("prizes", []):
        if "item" in p:
            add(p["item"], "prize counter " + name)
for p in sorted((ROOT / "maps").glob("*.map")):
    if p.name.startswith("test_"):
        continue
    t = p.read_text()
    for m in re.finditer(r"\bitem=(\w+)", t):
        add(m.group(1), "pouch in " + p.stem)
    for m in re.finditer(r"give_item\s+(\w+)", t):
        add(m.group(1), "gift in " + p.stem)
want = [k for k, v in items.items() if v["kind"] == "disc"] + ["life_spark", "life_blaze", "vital_draught", "growth_seed", "vigor_drop", "vigor_flask", "strong_ward", "grand_ward", "exit_wisp",
        # contract 73: the battle items (shops), Defrost / Cure-All (shops), Ether / Max Ether (pouches), contract 77: PP Extra (pouches)
        "pulso_atk", "pulso_def", "pulso_spd", "pulso_spc", "pulso_acc", "velo_bruma", "foco_critico", "descongelar", "panacea", "savia", "savia_max", "pp_extra",
        # contract 80: the Gold Chunk (the Vertice recruiter of Ruta 24 gives one, four more lie hidden)
        "trozo_oro"]
bad = [k for k in want if k not in where]
for k in want:
    print("%-14s %s" % (k, ", ".join(sorted(set(where.get(k, ["NOWHERE"]))))))
print("disc_sources_check: %d items, %d obtainable, %d NOT" % (len(want), len(want) - len(bad), len(bad)))
sys.exit(1 if bad else 0)
