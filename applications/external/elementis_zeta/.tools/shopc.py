#!/usr/bin/env python3
"""assets/data/shops.toml (+ items.toml for the item ids) -> nucleo/files/shops.bin + nucleo/gen/shops_gen.h

Usage: shopc.py [build|check] (default build; `check` validates and verifies the files are up to date)
Format: contract section 17.1. Little-endian.
  header 8 B: "NSHP", u8 version = 1, u8 n_shops (INCLUDING id 0 = the empty shop), u16 file_size
  u16 offsets[n_shops] byte offset (from the start of the file) of each shop record
  record: u8 count (0..16), u8 item[count] (item ids = positions in items.toml, 1-based)
  PRIZE record (C4-C, contract 37; the casino's prize counter, priced in COINS; `shop` refuses it, the casino native opens it):
               u8 0x80 | count (1..8), then count entries of 5 bytes: u8 kind (0 item, 1 creature), u8 id (items.toml / species.toml position, 1-based),
               u8 level (a creature's level; an item's quantity), u16 price in coins (1..9999, little-endian)
"""
import re
import struct
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "assets" / "data"
OUT_BIN = ROOT / "nucleo" / "files" / "shops.bin"
OUT_GEN = ROOT / "nucleo" / "gen" / "shops_gen.h"
VERSION = 1
MAX_ITEMS = 16
MAX_SHOPS = 64
MAX_PRIZES = 8
PRIZE_FLAG = 0x80


class ShopError(Exception):
    pass


def load(name):
    with open(DATA / name, "rb") as f:
        return tomllib.load(f)


def compile_shops():
    items = load("items.toml")
    item_ids = {name: i + 1 for i, name in enumerate(items)}
    shops = load("shops.toml")
    species = load("species.toml")
    sp_ids = {name: i + 1 for i, name in enumerate(species)}
    if len(shops) + 1 > MAX_SHOPS:
        raise ShopError("too many shops (%d max)" % (MAX_SHOPS - 1))
    recs = [bytes([0])]  # id 0: empty
    for name, sh in shops.items():
        if not re.fullmatch(r"[a-z][a-z0-9_]*", name):
            raise ShopError("shop name %r: lower-case identifier expected" % name)
        if sh.get("coins"):  # C4-C: the prize counter (priced in coins)
            pr = sh.get("prizes")
            if "items" in sh or not isinstance(pr, list) or not pr:
                raise ShopError(
                    "shop %s: a coins shop has prizes = [{species|item, level, price}, ..] and no items"
                    % name
                )
            if len(pr) > MAX_PRIZES:
                raise ShopError(
                    "shop %s: %d prizes (max %d)" % (name, len(pr), MAX_PRIZES)
                )
            rec = [PRIZE_FLAG | len(pr)]
            seen = set()
            for k, e in enumerate(pr):
                if ("species" in e) == ("item" in e) or set(e) - {
                    "species",
                    "item",
                    "level",
                    "price",
                }:
                    raise ShopError(
                        "shop %s: prize %d: exactly one of species / item, plus price (and level)"
                        % (name, k)
                    )
                price = e.get("price")
                if not isinstance(price, int) or not 1 <= price <= 9999:
                    raise ShopError(
                        "shop %s: prize %d: price must be 1..9999 coins" % (name, k)
                    )
                if "species" in e:
                    if e["species"] not in sp_ids:
                        raise ShopError(
                            "shop %s: prize %d: unknown species %r (assets/data/species.toml)"
                            % (name, k, e["species"])
                        )
                    lv = e.get("level", 5)
                    if not isinstance(lv, int) or not 1 <= lv <= 100:
                        raise ShopError("shop %s: prize %d: level 1..100" % (name, k))
                    key, ent = ("s", e["species"]), (1, sp_ids[e["species"]], lv)
                else:
                    if (
                        e["item"] not in item_ids
                        or items[e["item"]].get("kind") == "key"
                    ):
                        raise ShopError(
                            "shop %s: prize %d: unknown item / key item %r"
                            % (name, k, e["item"])
                        )
                    key, ent = ("i", e["item"]), (0, item_ids[e["item"]], 1)
                if key in seen:
                    raise ShopError("shop %s: prize %d listed twice" % (name, k))
                seen.add(key)
                rec += list(ent) + [price & 255, price >> 8]
            recs.append(bytes(rec))
            continue
        lst = sh.get("items")
        if not isinstance(lst, list) or not lst:
            raise ShopError("shop %s: items = [..] expected (at least one item)" % name)
        if len(lst) > MAX_ITEMS:
            raise ShopError("shop %s: %d items (max %d)" % (name, len(lst), MAX_ITEMS))
        ids = []
        for k, it in enumerate(lst):
            if it not in item_ids:
                raise ShopError(
                    "shop %s: unknown item %r (assets/data/items.toml)" % (name, it)
                )
            d = items[it]
            if d.get("kind") == "key" or not d.get("price", 0):
                raise ShopError(
                    "shop %s: item %r has no price / is a key item" % (name, it)
                )
            if it in lst[:k]:
                raise ShopError("shop %s: item %r listed twice" % (name, it))
            ids.append(item_ids[it])
        recs.append(bytes([len(ids)] + ids))
    n = len(recs)
    off = 8 + 2 * n
    table = b""
    body = b""
    for r in recs:
        table += struct.pack("<H", off + len(body))
        body += r
    size = off + len(body)
    data = b"NSHP" + struct.pack("<BBH", VERSION, n, size) + table + body
    assert len(data) == size
    hdr = [
        "/* generated by tools/shopc.py from assets/data/shops.toml: do not edit */",
        "#ifndef SHOPS_GEN_H",
        "#define SHOPS_GEN_H",
        "",
        "/* shop ids (shops.bin) */",
    ]
    for i, name in enumerate(shops, 1):
        hdr.append(
            "#define SHOP_%s %d" % (re.sub(r"[^A-Za-z0-9]+", "_", name).upper(), i)
        )
    hdr += ["#define SHOP_COUNT %d" % n, "", "#endif", ""]
    return data, "\n".join(hdr)


def main():
    cmd = sys.argv[1] if len(sys.argv) > 1 else "build"
    try:
        data, gen = compile_shops()
    except (ShopError, KeyError, tomllib.TOMLDecodeError, OSError) as e:
        print("shopc: error: %s" % e, file=sys.stderr)
        return 1
    if cmd == "check":
        ok = (
            OUT_BIN.exists()
            and OUT_BIN.read_bytes() == data
            and OUT_GEN.exists()
            and OUT_GEN.read_text() == gen
        )
        print(
            "shopc: %s"
            % (
                "OK (%d bytes)" % len(data)
                if ok
                else "shops.bin / shops_gen.h are out of date: run tools/shopc.py"
            )
        )
        return 0 if ok else 1
    OUT_BIN.write_bytes(data)
    OUT_GEN.write_text(gen)
    print(
        "shopc: wrote %s (%d bytes, %d shops)"
        % (OUT_BIN.relative_to(ROOT), len(data), data[5] - 1)
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
