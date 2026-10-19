#!/usr/bin/env python3
"""assets/data/{types,moves,species,trainers,items,affinity}.toml -> nucleo/files/{reg,moves,trainers,items,evostone,tm,affinity}.bin (+ reginfo.bin) + nucleo/gen/reg_gen.h

Usage: regc.py [build|check] (default build; `check` validates and verifies the files are up to date)
Formats: contract section 7. Little-endian, 4-byte aligned sections.
"""
import math
import re
import struct
import sys
import tomllib
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "assets" / "data"
OUT_FILES = ROOT / "nucleo" / "files"
OUT_GEN = ROOT / "nucleo" / "gen"

VERSION = 1
MAX_LEARN = 12
MAX_PARTY = 6
SPECIES_REC = 40
MOVE_REC = 12
TYPE_REC = 4
CHART_REC = 3  # (the SOURCE rows; the file packs them, contract 83: n bytes of att | def << 4 then ceil(n / 4) bytes of 2-bit multiplier codes, 0 / 5 / 20 -> 0 / 1 / 2)
TRAINER_REC = 32
ITEM_REC = 8
AI_REC = 8       # contract 82: ai.bin record: count, then 2 rules of (random byte below [0 = always], condition [0 none, 255 status, N: HP < max / N], item id [AI_SWITCH: switch creatures])
AI_MAX_COUNT = 14  # contract 85: the AI's uses live +1 in the low nibble of Battle.ai_st (battle_ai.c): 14 + 1 is the largest that does not carry into the turn count (bits 4 .. 5)
AI_SWITCH = 255

STATS = {"atk": 0, "def": 1, "spd": 2, "spc": 3, "acc": 4, "eva": 5}
STATUS = {"sleep": 1, "poison": 2, "burn": 3, "freeze": 4, "paralysis": 5, "confusion": 6}
SCREENS = {"reflect": 0, "light_screen": 1}
GROWTH = {"medium_fast": 0, "medium_slow": 1, "fast": 2, "slow": 3}
EFFECTS = {"none": 0, "stat_tgt": 1, "stat_self": 2, "status_tgt": 3, "focus": 4, "screen": 5, "flinch": 6,
           # contract 64: the engine effects of the battle plugin (arg0 / arg1 in moves.toml, see the header of that file)
           "multi": 7, "fixed": 8, "ohko": 9, "recoil": 10, "drain": 11, "heal": 12, "suicide": 13, "charge": 14, "lock": 15,
           "flee": 16, "stat_reset": 17, "mist": 18, "leech": 19, "counter": 20, "move_call": 21, "store_dmg": 22, "substitute": 23,
           # contract 73: Mega Beam's recharge, Jump Kick's crash, Disable, Typecast, Rage's lock, Transform (move_call arg0 2 = Echoing's slot copy; status_tgt arg1 1 = Toxic)
           "recharge": 24, "crash": 25, "disable": 26, "convert": 27, "rage": 28, "transform": 29,
           # contract 82 (agent): Pilfer (the coins scatter: 2 x the user's level, paid at the end of a won battle)
           "coin_hit": 30}
MF_HIGH_CRIT, MF_NEVER_MISS = 1, 2
MF_AI2, MF_SPEC = 0x10, 0x20   # contract 82 (derived from the effect, never written in moves.toml): AI modifier 2 encourages the move (a stat / set-up / screen / heal move ...); it counts as "a better move" for AI modifier 3 (special damage, Fly / Dig)
AI2_EFFECTS = ("stat_self", "heal", "screen", "stat_reset", "convert", "transform", "coin_hit")   # the reference's ATTACK_UP1 .. STAT_RESET and ATTACK_UP2 .. REFLECT (plus a power-0 stat_tgt: the Growl-likes) and COIN_HIT
ITEM_KINDS = {"catch": 1, "heal": 2, "cure": 3, "key": 4, "repel": 5, "rope": 6, "disc": 7, "stone": 8, "vitamin": 9,
              "revive": 10, "restore": 11, "candy": 12, "elixir": 13,
              "battle": 14, "ppup": 15, "ether": 16}  # items.bin kind (0 = none); 10 .. 13: contract 64; 14 .. 16: contract 73 (battle: boost items / Mist Veil / Crit Focus; ppup: PP Plus; ether: Ether / Max Ether)
STONES = {"moon": 0, "fire": 1, "water": 2, "thunder": 3, "leaf": 4}                 # C4-B (contract 36): arg of an IK_STONE item = the stone's index; species.toml `evolve_stone` keys
STONE_ES = {"luna": "moon", "fuego": "fire", "agua": "water", "trueno": "thunder", "hoja": "leaf"}  # ... the Spanish item keys are accepted too
EVO_FRIEND = 0x80                                                              # contract 65: reg.bin evo_level bit 7 = a friendship evolution (level in the low 7 bits = the floor; species.toml `evolve_friend`)
NO_STONE = 7                                                                   # contract 80: the "stone" index of a TREASURE item (items.toml `kind = "treasure"`, the Gold Chunk: Gold Chunk): items.bin kind IK_STONE + this arg; no species evolves with it, so Usar is "no effect" (kept) and it sells for half its price (ST_NONE)
TRADE_STONE = 5                                                                # IR LINK (contract 45): the trade-evolution record of evostone.bin uses this "stone" index (ST_TRADE); no item has it
BATTLE_EFFECTS = {"mist": 5, "focus": 6}                                          # contract 73: arg of an IK_BATTLE item above the five stats (0 .. 4)
VITAMIN_STAT = {"hp": 0, "atk": 1, "def": 2, "spd": 3, "spc": 4}                       # arg of an IK_VITAMIN item = the Mon.statexp[] index it raises
EVOSTONE_REC = 4
MAX_DISCS = 64                                                                 # species tm mask: tm.bin (u64 per species, contract 64); reg.bin keeps bits 0..23 (u16 at byte 12 + u8 at byte 15)
CURE_STATUS = {"slp": "sleep", "psn": "poison", "brn": "burn", "frz": "freeze", "par": "paralysis"}  # items.toml status -> STATUS
BALLS = {"crystal": 0, "great": 1, "ultra": 2, "master": 3}  # items.bin arg of a catch item


class RegError(Exception):
    pass


def ident(s):
    return re.sub(r"[^A-Za-z0-9]+", "_", s).strip("_").upper()


def load_toml(name):
    with open(DATA / name, "rb") as f:
        return tomllib.load(f)


def chance_byte(pct):
    """percent -> 0..255 threshold for `rand8 < t` (10 -> 26 like the reference's); 0 = always."""
    if pct is None or pct >= 100:
        return 0
    return min(255, -(-pct * 256 // 100))


def acc_byte(pct):
    if not 1 <= pct <= 100:
        raise RegError(f"accuracy {pct} out of range")
    return pct * 255 // 100


# ---- parse + validate -------------------------------------------------------------------------
def parse():
    t = load_toml("types.toml")
    types = [(x["id"], bool(x["physical"])) for x in t["type"]]
    tid = {n: i for i, (n, _) in enumerate(types)}
    if len(tid) != len(types):
        raise RegError("duplicate type id")
    chart = []
    for a, d, m in t["chart"]["rows"]:
        if a not in tid or d not in tid:
            raise RegError(f"chart: unknown type in {[a, d, m]}")
        if m not in (0, 5, 20):
            raise RegError(f"chart: bad multiplier {m}")
        chart.append((tid[a], tid[d], m))
    if len({(a, d) for a, d, _ in chart}) != len(chart):
        raise RegError("chart: duplicate pair")

    mv = load_toml("moves.toml")
    mnames = list(mv)
    if len(mnames) > 254:
        raise RegError("too many moves")
    mid = {n: i + 1 for i, n in enumerate(mnames)}
    moves = []
    for n in mnames:
        m = mv[n]
        if m["type"] not in tid:
            raise RegError(f"move {n}: unknown type")
        eff = m.get("effect", "none")
        if eff not in EFFECTS:
            raise RegError(f"move {n}: unknown effect {eff}")
        a0, a1 = 0, 0
        if eff in ("stat_tgt", "stat_self"):
            a0, a1 = STATS[m["stat"]], int(m["stages"])
            if not -6 <= a1 <= 6 or a1 == 0:
                raise RegError(f"move {n}: bad stages")
        elif eff == "status_tgt":
            a0 = STATUS[m["status"]]
            a1 = 1 if m.get("toxic") else 0  # contract 73: Toxic (a poison that grows every turn)
        elif eff == "screen":
            a0 = SCREENS[m["screen"]]
        elif EFFECTS[eff] >= 7:  # contract 64: the effects of the data; arg0 (0..255) / arg1 (-128..127) are given in the table (default 0)
            a0, a1 = int(m.get("arg0", 0)), int(m.get("arg1", 0))
            if not 0 <= a0 <= 255 or not -128 <= a1 <= 127:
                raise RegError(f"move {n}: bad arg0 / arg1")
        pr = int(m.get("priority", 0))
        if not -6 <= pr <= 6:
            raise RegError(f"move {n}: bad priority")
        power = int(m["power"])
        if not 0 <= power <= 255:
            raise RegError(f"move {n}: bad power")
        flags = (MF_HIGH_CRIT if m.get("high_crit") else 0) | (MF_NEVER_MISS if m.get("never_miss") else 0)
        if eff in AI2_EFFECTS or (eff == "stat_tgt" and power == 0):
            flags |= MF_AI2
        if eff == "fixed" or (eff == "charge" and a0 & 1):
            flags |= MF_SPEC
        moves.append(dict(name=n, type=tid[m["type"]], power=power, acc=acc_byte(int(m["acc"])),
                          pp=int(m["pp"]), effect=EFFECTS[eff], a0=a0, a1=a1,
                          chance=chance_byte(m.get("chance")), priority=pr, flags=flags))

    sp = load_toml("species.toml")
    snames = list(sp)
    if len(snames) > 254:
        raise RegError("too many species")
    sid = {n: i + 1 for i, n in enumerate(snames)}
    species = []
    for n in snames:
        s = sp[n]
        ty = s["types"]
        if not 1 <= len(ty) <= 2 or any(x not in tid for x in ty):
            raise RegError(f"species {n}: bad types")
        t1 = tid[ty[0]]
        t2 = tid[ty[1]] if len(ty) == 2 else t1
        base = s["base"]
        if len(base) != 5 or any(not 1 <= b <= 255 for b in base):
            raise RegError(f"species {n}: bad base stats")
        if s["growth"] not in GROWTH:
            raise RegError(f"species {n}: bad growth")
        ev_lv, ev_to = 0, 0
        if "evolve" in s:
            ev_lv, ev_name = s["evolve"]
            if ev_name not in sid:
                raise RegError(f"species {n}: evolves to unknown {ev_name}")
            ev_to = sid[ev_name]
        if "evolve_friend" in s:  # contract 65: evolve_friend = [level, "species"]: a level-up at that level or later WITH friendship >= 220 (bc_level_evo): reg.bin evo_level = level | 0x80 (the level floor + the
            if "evolve" in s or "evolve_stone" in s or "evolve_trade" in s:  # friendship mark), evo_to = the target; the resident chain only masks the bit, the plugins that open the evolution ask the friendship
                raise RegError(f"species {n}: evolve_friend is exclusive with evolve, evolve_stone and evolve_trade")
            ef = s["evolve_friend"]
            if not (isinstance(ef, list) and len(ef) == 2 and isinstance(ef[0], int) and 2 <= ef[0] <= 100 and isinstance(ef[1], str)):
                raise RegError(f"species {n}: evolve_friend must be [level 2..100, \"species\"]")
            if ef[1] not in sid or ef[1] == n:
                raise RegError(f"species {n}: evolve_friend: unknown or own target {ef[1]!r}")
            ev_lv, ev_to = ef[0] | EVO_FRIEND, sid[ef[1]]
        stones = []  # C4-B: evolve_stone = { leaf = "species", ... } -> [(stone index, target id)] (written to evostone.bin, contract 36)
        if "evolve_stone" in s:
            if "evolve" in s:
                raise RegError(f"species {n}: evolve and evolve_stone are exclusive (a species evolves by level OR by stone)")
            es = s["evolve_stone"]
            if not isinstance(es, dict) or not es:
                raise RegError(f"species {n}: evolve_stone must be a table {{ stone = \"species\", ... }}")
            for k, tgt in es.items():
                k = STONE_ES.get(k, k)
                if k not in STONES:
                    raise RegError(f"species {n}: evolve_stone: unknown stone {k!r} (one of {sorted(STONES)})")
                if tgt not in sid or tgt == n:
                    raise RegError(f"species {n}: evolve_stone {k}: unknown or own target {tgt!r}")
                if any(STONES[k] == x for x, _ in stones):
                    raise RegError(f"species {n}: evolve_stone: {k} twice")
                stones.append((STONES[k], sid[tgt]))
            stones.sort()
        if "evolve_trade" in s:  # IR LINK (contract 45): evolve_trade = "species": evolves right after a trade; one record {species, TRADE_STONE, target} of evostone.bin
            tgt = s["evolve_trade"]
            if "evolve" in s or "evolve_stone" in s:
                raise RegError(f"species {n}: evolve_trade is exclusive with evolve and evolve_stone (a species evolves by level OR by stone OR by trade)")
            if not isinstance(tgt, str) or tgt not in sid or tgt == n:
                raise RegError(f"species {n}: evolve_trade: unknown or own target {tgt!r}")
            stones.append((TRADE_STONE, sid[tgt]))
        learn = []
        tm = list(s.get("tm", []))  # item keys of the discs (kind = "disc") this species can learn (C2-S1); resolved below
        for lv, m in s["learn"]:
            if m not in mid or not 1 <= lv <= 100:
                raise RegError(f"species {n}: bad learn entry {[lv, m]}")
            learn.append((lv, mid[m]))
        if len(learn) > MAX_LEARN or sorted(learn, key=lambda x: x[0]) != learn:
            raise RegError(f"species {n}: learnset too long or not sorted by level")
        species.append(dict(name=n, t1=t1, t2=t2, base=base, growth=GROWTH[s["growth"]],
                            base_exp=int(s["base_exp"]), catch=int(s.get("catch_rate", 45)),
                            ev_lv=ev_lv, ev_to=ev_to, learn=learn, tm=tm, stones=stones))

    tr = load_toml("trainers.toml")
    tnames = list(tr)
    trainers = []
    for n in tnames:
        x = tr[n]
        party = []
        for s, lv in x["party"]:
            if s not in sid or not 1 <= lv <= 100:
                raise RegError(f"trainer {n}: bad party entry {[s, lv]}")
            party.append((sid[s], lv))
        if not 1 <= len(party) <= MAX_PARTY:
            raise RegError(f"trainer {n}: party size")
        counter = [(sid[a], sid[b]) for a, b in x.get("counter", {}).items()]
        if len(counter) > 3 or any(a not in sid for a in x.get("counter", {})) \
                or any(b not in sid for b in x.get("counter", {}).values()):
            raise RegError(f"trainer {n}: bad counter table")
        aim = 0
        for k in x.get("ai_mod", []):
            if k not in (1, 2, 3):
                raise RegError(f"trainer {n}: bad ai_mod")
            aim |= 1 << (k - 1)
        aicls = x.get("ai")  # contract 82: the trainer class AI of assets/data/ai.toml (its move-choice modifiers and its item / switch routine); resolved below, once the items are known
        if x.get("counter_starter") and not counter:
            raise RegError(f"trainer {n}: counter_starter without counter table")
        tname = x.get("name", "")  # a global text key ("gl.tr.<id>") or "rival" (the runtime {rival} token)
        if not isinstance(tname, str) or not (tname == "rival" or tname.startswith("gl.tr.")):
            raise RegError(f"trainer {n}: name must be \"rival\" or a \"gl.tr.*\" text key")
        trainers.append(dict(name=n, ai=aim, aicls=aicls, flags=1 if x.get("counter_starter") else 0, party=party,
                             counter=counter, money=int(x.get("money", 0)), tname=tname))
    it = load_toml("items.toml")
    inames = list(it)
    if not 1 <= len(inames) <= 254:
        raise RegError("items: 1..254 items")
    iid = {n: i + 1 for i, n in enumerate(inames)}
    items = []
    disc_bit = {}  # disc item key -> its bit in the species tm mask (order among the disc items)
    for n in inames:
        x = it[n]
        kind = x.get("kind")
        if kind not in ITEM_KINDS and kind != "treasure":
            raise RegError(f"item {n}: unknown kind {kind!r}")
        if kind == "treasure":  # contract 80: a valuable to sell: IK_STONE with the stone index nobody evolves with (the battle plugin's item validator and every reader know IK_STONE already)
            kind, arg = "stone", NO_STONE
        elif kind == "catch":
            if x.get("ball", "crystal") not in BALLS:
                raise RegError(f"item {n}: unknown ball {x.get('ball')!r}")
            arg = BALLS[x.get("ball", "crystal")]
        elif kind == "heal":
            arg = int(x["hp"])
            if not 1 <= arg <= 255:
                raise RegError(f"item {n}: hp must be 1..255")
        elif kind == "cure":
            st = x.get("status")
            if st != "all" and st not in CURE_STATUS:  # contract 73: "all" = Cure-All (arg 0: any status)
                raise RegError(f"item {n}: cure needs status = one of {sorted(CURE_STATUS)} or \"all\"")
            arg = 0 if st == "all" else STATUS[CURE_STATUS[st]]
        elif kind == "battle":  # contract 73: `stat` = atk | def | spd | spc | acc (+1 stage, boost items) or `effect` = "mist" (Mist Veil) | "focus" (Crit Focus); in battle only
            if x.get("effect") in BATTLE_EFFECTS:
                arg = BATTLE_EFFECTS[x["effect"]]
            elif x.get("stat") in ("atk", "def", "spd", "spc", "acc"):
                arg = STATS[x["stat"]]
            else:
                raise RegError(f"item {n}: battle needs stat = atk|def|spd|spc|acc or effect = mist|focus")
        elif kind == "ether":  # contract 73: restores `pp` points to ONE move (255 = all of it); outside battle only
            arg = int(x["pp"])
            if not 1 <= arg <= 255:
                raise RegError(f"item {n}: pp must be 1..255")
        elif kind == "repel":
            arg = int(x["steps"])
            if not 1 <= arg <= 255:
                raise RegError(f"item {n}: steps must be 1..255")
        elif kind == "disc":
            if x.get("move") not in mid:
                raise RegError(f"item {n}: disc needs move = a key of moves.toml")
            arg = mid[x["move"]]
            disc_bit[n] = len(disc_bit)
            if len(disc_bit) > MAX_DISCS:
                raise RegError(f"items: at most {MAX_DISCS} discs (the species tm mask is 64 bits)")
        elif kind == "revive":  # contract 64: a fainted creature comes back with half (`full = false`) or all (`full = true`) of its HP
            arg = 255 if x.get("full") else 0
        elif kind == "elixir":  # contract 64: restores `pp` points to EVERY move of one creature (255 = all of them); outside battle only
            arg = int(x["pp"])
            if not 1 <= arg <= 255:
                raise RegError(f"item {n}: pp must be 1..255")
        elif kind == "stone":
            st = x.get("stone")
            if st not in STONES:
                raise RegError(f"item {n}: stone needs stone = one of {sorted(STONES)}")
            arg = STONES[st]
        elif kind == "vitamin":
            st = x.get("stat")
            if st not in VITAMIN_STAT:
                raise RegError(f"item {n}: vitamin needs stat = one of {sorted(VITAMIN_STAT)}")
            arg = VITAMIN_STAT[st]
        else:  # key item, exit wisp, vital draught, growth seed
            arg = 0
        price = int(x.get("price", 0))
        if not 0 <= price <= 65535:
            raise RegError(f"item {n}: bad price")
        if kind == "key" and price:
            raise RegError(f"item {n}: a key item has no price")
        items.append(dict(name=n, kind=ITEM_KINDS[kind], arg=arg, price=price, aux=disc_bit.get(n, 0)))
    # contract 82 (agent): the trainer AI classes (assets/data/ai.toml). byte 0 of a trainers.bin record = the move-choice modifiers (bits 0 .. 2) | the routine's row in ai.bin << 3 (0 = no routine)
    ai = load_toml("ai.toml")
    ai_rows, ai_cls = [bytes(AI_REC)], {}  # row 0 = none
    for cn, c in ai.items():
        mods = 0
        for k in c.get("mods", []):
            if k not in (1, 2, 3):
                raise RegError(f"ai class {cn}: bad mods")
            mods |= 1 << (k - 1)
        row = bytearray(AI_REC)
        rules = c.get("rules", [])
        if len(rules) > 2 or (rules and not 1 <= int(c.get("count", 0)) <= AI_MAX_COUNT):
            raise RegError(f"ai class {cn}: at most 2 rules and a count of 1 .. {AI_MAX_COUNT}")
        if rules:
            row[0] = int(c["count"])
            for k, r in enumerate(rules):
                below = int(r.get("below", 256))  # the rule fires when the AI's random byte is below this (256 = always); the reference
                cond = 255 if r.get("status") else int(r.get("hp_below", 0))  # 255: the creature has a status; N: its HP is below max / N
                if not 1 <= below <= 256 or not 0 <= cond <= 255 or ("switch" in r) == ("item" in r):
                    raise RegError(f"ai class {cn}: bad rule {r}")
                if "switch" in r:
                    item = AI_SWITCH
                else:
                    if r["item"] not in iid or items[iid[r["item"]] - 1]["kind"] not in (ITEM_KINDS["heal"], ITEM_KINDS["cure"], ITEM_KINDS["restore"], ITEM_KINDS["battle"]):
                        raise RegError(f"ai class {cn}: item {r['item']!r} is not a heal / cure / restore / battle item")
                    item = iid[r["item"]]
                row[1 + 3 * k:4 + 3 * k] = bytes((0 if below >= 256 else below, cond, item))
            ai_cls[cn] = len(ai_rows)
            if len(ai_rows) > 31:
                raise RegError("ai.toml: at most 31 routine classes (5 bits)")
            ai_rows.append(bytes(row))
        c["mask"] = mods
    for t_ in trainers:
        if t_["aicls"] is not None:
            if t_["aicls"] not in ai:
                raise RegError(f"trainer {t_['name']}: unknown ai class {t_['aicls']!r}")
            t_["ai"] |= ai[t_["aicls"]]["mask"] | (ai_cls.get(t_["aicls"], 0) << 3)
    for sp_ in species:  # tm = ["disc_01", ...] -> the 24-bit mask
        mask = 0
        for k in sp_["tm"]:
            if k not in disc_bit:
                raise RegError(f"species {sp_['name']}: tm {k!r} is not a disc item of items.toml")
            mask |= 1 << disc_bit[k]
        sp_["tm"] = mask
    # contract 64 consistency: every technique disc can be used by at least one species, and every move can be learned somewhere (by level, or through a disc some species can use)
    learned = {m for sp_ in species for _, m in sp_["learn"]}
    usable = {k for sp_ in species for k in range(64) if sp_["tm"] >> k & 1}
    dmove = {}
    for it_name, x in ((n, it[n]) for n in inames if it[n].get("kind") == "disc"):
        dmove[disc_bit[it_name]] = mid[x["move"]]
    dead = [k for k in dmove if k not in usable]
    if dead:
        raise RegError("discs no species can learn: " + ", ".join(n for n in inames if n in disc_bit and disc_bit[n] in dead))
    reach = learned | {dmove[k] for k in usable}
    lost = [m["name"] for i_, m in enumerate(moves, 1) if i_ not in reach]
    if lost:
        raise RegError("moves nobody can learn (no learnset entry, no usable disc): " + ", ".join(lost))
    return dict(types=types, tid=tid, chart=chart, moves=moves, mid=mid, species=species, sid=sid,
                trainers=trainers, items=items, iid=iid, affinity=affinity_pairs(sid), ai_rows=ai_rows)


# ---- binary emit -----------------------------------------------------------------------------
def pad4(b):
    return b + b"\0" * (-len(b) % 4)


def emit_reg(d):
    body = bytearray(b"\0" * SPECIES_REC)  # id 0 = none
    for s in d["species"]:
        lv = [x[0] for x in s["learn"]] + [0] * MAX_LEARN
        mv = [x[1] for x in s["learn"]] + [0] * MAX_LEARN
        rec = struct.pack("<BB5BBBBBBHBB", s["t1"], s["t2"], *s["base"], s["growth"], s["base_exp"],
                          s["catch"], s["ev_lv"], s["ev_to"], s["tm"] & 0xFFFF, len(s["learn"]), (s["tm"] >> 16) & 0xFF)
        rec += bytes(lv[:MAX_LEARN]) + bytes(mv[:MAX_LEARN])
        assert len(rec) == SPECIES_REC, len(rec)
        body += rec
    n = 1 + len(d["species"])
    hdr = struct.pack("<4sBBBBI", b"NREG", VERSION, n, SPECIES_REC, 0, 12 + len(body))
    return hdr + bytes(body)


def emit_tm(d):
    """tm.bin (contract 64): the technique-disc masks of ALL the discs (reg.bin's record only has room for 24 bits). "NTM1", u8 version 1, u8 n (species ids incl. 0), u8 rec_size 8, u8 0,
    u32 file_size; then n records INDEXED BY SPECIES ID: u64 mask (bit k = the disc whose items.bin record has aux = k, i.e. the k-th disc item of items.toml). Nothing of it is
    resident: the party screen (menu plugin) reads the 8 bytes of one species when a disc is used."""
    body = bytearray(b"\0" * 8)
    for s in d["species"]:
        body += struct.pack("<Q", s["tm"])
    return struct.pack("<4sBBBBI", b"NTM1", VERSION, 1 + len(d["species"]), 8, 0, 12 + len(body)) + bytes(body)


def affinity_pairs(sid):
    """assets/data/affinity.toml (contract 66) -> [(solar id, lunar id, wild)] in file order; the structural rules (the key names, one pair per species, known species)."""
    doc = load_toml("affinity.toml")
    out, seen = [], {}
    for k, pr in enumerate(doc.get("pair", [])):
        if set(pr) - {"solar", "lunar", "wild"} or "solar" not in pr or "lunar" not in pr:
            raise RegError(f"affinity.toml pair {k}: keys are solar, lunar and (optional) wild")
        a, b = pr["solar"], pr["lunar"]
        if a not in sid or b not in sid:
            raise RegError(f"affinity.toml pair {k}: unknown species {a if a not in sid else b}")
        if a == b:
            raise RegError(f"affinity.toml pair {k}: {a} is paired with itself")
        for n in (a, b):
            if n in seen:
                raise RegError(f"affinity.toml: {n} is in two pairs (pair {seen[n]} and {k})")
            seen[n] = k
        out.append((sid[a], sid[b], bool(pr.get("wild", True))))
    return out


def emit_affinity(d):
    """aff.bin (contract 66): "NAFF", u8 version 1, u8 n (species ids incl. 0), u8 pairs, u8 0, u32 file_size; then n bytes INDEXED BY SPECIES ID, padded to 4: the LUNAR view = the species a Lunar
    player meets in a wild battle where the map's table says this one (the `lunar` counterpart of a `wild` pair's `solar` form, else the same id: the view is total). The Solar view is the identity
    (the tables list the Solar form). Nothing of it is resident: the battle plugin reads one byte when a wild battle opens (nucleo/affinity.h)."""
    n = 1 + len(d["species"])
    body = bytearray(range(n))
    for a, b, wild in d["affinity"]:
        if wild:
            body[a] = b
    body = pad4(bytes(body))
    return struct.pack("<4sBBBBI", b"NAFF", VERSION, n, len(d["affinity"]), 0, 12 + len(body)) + body


def emit_moves(d):
    mv = bytearray(b"\0" * MOVE_REC)  # id 0 = none
    for m in d["moves"]:
        mv += struct.pack("<BBBBBBbBbBH", m["type"], m["power"], m["acc"], m["pp"], m["effect"], m["a0"],
                          m["a1"], m["chance"], m["priority"], m["flags"], 0)
    ty = b"".join(struct.pack("<BBH", 0 if phys else 1, 0, 0) for _, phys in d["types"])
    assert all(r[0] < 16 and r[1] < 16 and r[2] in (0, 5, 20) for r in d["chart"]), "chart: the packed rows need types < 16 and a multiplier of 0 / 5 / 20"
    mc = bytearray((len(d["chart"]) + 3) // 4)
    for i, r in enumerate(d["chart"]):
        mc[i >> 2] |= {0: 0, 5: 1, 20: 2}[r[2]] << ((i & 3) * 2)
    ch = pad4(bytes(r[0] | r[1] << 4 for r in d["chart"]) + bytes(mc))
    nm = 1 + len(d["moves"])
    off_m = 24
    off_t = off_m + len(mv)
    off_c = off_t + len(ty)
    size = off_c + len(ch)
    hdr = struct.pack("<4sBBBBIIII", b"NMOV", VERSION, nm, len(d["types"]), len(d["chart"]),
                      off_m, off_t, off_c, size)
    assert len(hdr) == off_m
    return hdr + bytes(mv) + ty + ch


def global_text_ids():
    """{global text key: its id (0x8000 | position among the global keys of the master text)}; {} when the text tools are unavailable."""
    try:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import textc
        return {k: 0x8000 | i for i, k in enumerate([k for k in textc.load() if textc.is_global(k)])}
    except Exception as e:  # pragma: no cover
        print(f"regc: warning: text ids unavailable ({e})", file=sys.stderr)
        return {}


def emit_trainers(d):
    """trainers.bin. Contract 64: the u16 at byte 26 of a record (reserved before) is the text id of the trainer's name (class + name: gl.tr.*), 0xFFFE = the runtime {rival} name, 0xFFFF = no key."""
    body = bytearray(b"\0" * TRAINER_REC)  # id 0 = none
    ids = global_text_ids()
    for t in d["trainers"]:
        sp = [p[0] for p in t["party"]] + [0] * MAX_PARTY
        lv = [p[1] for p in t["party"]] + [0] * MAX_PARTY
        cf = [c[0] for c in t["counter"]] + [0] * 3
        ct = [c[1] for c in t["counter"]] + [0] * 3
        rec = struct.pack("<BBBBHH", t["ai"], t["flags"], len(t["party"]), len(t["counter"]), 0, t["money"])
        rec += bytes(sp[:MAX_PARTY]) + bytes(lv[:MAX_PARTY]) + bytes(cf[:3]) + bytes(ct[:3])
        rec += b"\0" * (TRAINER_REC - len(rec))
        rec = rec[:26] + struct.pack("<H", 0xFFFE if t["tname"] == "rival" else ids.get(t["tname"], 0xFFFF)) + rec[28:]
        body += rec
    n = 1 + len(d["trainers"])
    hdr = struct.pack("<4sBBBBI", b"NTRN", VERSION, n, TRAINER_REC, 0, 12 + len(body))
    return hdr + bytes(body)


def emit_ai(d):
    """ai.bin (contract 82): 12-byte header "NAIC" + one 8-byte row per routine class (row 0 = none); the battle plugin reads a row from the card on the AI's turn."""
    body = b"".join(d["ai_rows"])
    return struct.pack("<4sBBBBI", b"NAIC", VERSION, len(d["ai_rows"]), AI_REC, 0, 12 + len(body)) + body


def emit_items(d):
    body = bytearray(b"\0" * ITEM_REC)  # id 0 = none
    for it in d["items"]:
        body += struct.pack("<BBBBHH", it["kind"], it["arg"], it["aux"], 0, it["price"], 0)
    n = 1 + len(d["items"])
    hdr = struct.pack("<4sBBBBI", b"NITM", VERSION, n, ITEM_REC, 0, 12 + len(body))
    return hdr + bytes(body)


def emit_evostone(d):
    """evostone.bin (contract 36): "NEVS", u8 version 1, u8 n, u8 rec_size 4, u8 0, u32 file_size; n records {u8 species, u8 stone, u8 target species, u8 0}
    sorted by species then stone. Species that evolve by stone only: reg.bin keeps evo_level = evo_to = 0 for them, so the by-level code never sees them."""
    recs = b""
    for i, s in enumerate(d["species"], 1):
        for stone, tgt in s["stones"]:
            recs += struct.pack("<BBBB", i, stone, tgt, 0)
    n = len(recs) // EVOSTONE_REC
    if n > 255:
        raise RegError("evostone.bin: too many entries")
    return struct.pack("<4sBBBBI", b"NEVS", VERSION, n, EVOSTONE_REC, 0, 12 + len(recs)) + recs


def emit_header(d):
    L = ["/* Generated by tools/regc.py - do not edit. */", "#pragma once", "#include <stdint.h>", "",
         '#include "maps_gen.h"  /* TXT_GL_* ids of the names (assets/text/es.txt order) */', "",
         "/* The name tables below that only plugins read are WEAK definitions: every source file that includes this header emits one, the linker keeps ONE per module",
         "   (--gc-sections drops the unreferenced copies). A plain `static const` was a private copy per source file (contract 38.2). C++ (the API table) gets internal ones. */",
         "#ifdef __cplusplus",
         "#define REG_TABLE static const",
         "#else",
         "#define REG_TABLE const __attribute__((weak))",
         "#endif", "",
         "/* species ids (reg.bin) */"]
    for n, i in d["sid"].items():
        L.append(f"#define SP_{ident(n)} {i}")
    L.append(f"#define SP_COUNT {len(d['species']) + 1}")
    L += ["", "/* move ids (moves.bin) */"]
    for n, i in d["mid"].items():
        L.append(f"#define MV_{ident(n)} {i}")
    L.append(f"#define MV_COUNT {len(d['moves']) + 1}")
    L.append("#define MV_STRUGGLE 255  /* built into battle_core, not in moves.bin */")
    L += ["", "/* type ids */"]
    for n, i in d["tid"].items():
        L.append(f"#define TY_{ident(n)} {i}")
    L.append(f"#define TY_COUNT {len(d['types'])}")
    L.append("#define TY_NONE 255  /* typeless damage (confusion) */")
    L += ["", "/* trainer ids (trainers.bin) */"]
    for i, t in enumerate(d["trainers"], 1):
        L.append(f"#define TR_{ident(t['name'])} {i}")
    L.append(f"#define TR_COUNT {len(d['trainers']) + 1}")
    L += ["", "/* item ids (items.bin) */"]
    for n, i in d["iid"].items():
        L.append(f"#define IT_{ident(n)} {i}")
    L.append(f"#define IT_COUNT {len(d['items']) + 1}")
    L += ["", "/* item kinds (items.bin kind) and ball classes (arg of a catch item) */"]
    for n, i in ITEM_KINDS.items():
        L.append(f"#define IK_{ident(n)} {i}")
    for n, i in BALLS.items():
        L.append(f"#define BALL_{ident(n)} {i}")
    L += ["", "/* evolution stones (arg of an IK_STONE item; evostone.bin, contract 36) */"]
    for n, i in STONES.items():
        L.append(f"#define ST_{ident(n)} {i}")
    L.append(f"#define ST_COUNT {len(STONES)}")
    L.append(f"#define ST_NONE {NO_STONE} /* the arg of a TREASURE item (items.toml kind = \"treasure\", contract 80): an IK_STONE no species evolves with */")
    L.append(f"#define ST_TRADE {TRADE_STONE} /* evostone.bin record of a trade evolution (species.toml evolve_trade, contract 45); never the arg of an item */")
    L.append(f"#define EVO_FRIEND {EVO_FRIEND} /* reg.bin evo_level bit 7: a friendship evolution (species.toml evolve_friend, contract 65): level = evo_level & 127, friendship >= 220 at the level-up */")
    L += ["", "/* growth groups */"]
    for n, i in GROWTH.items():
        L.append(f"#define GR_{ident(n)} {i}")
    L += ["", "/* stat indices: Mon.stat[], Side.stage[], move arg0 */"]
    for n, i in STATS.items():
        L.append(f"#define STI_{ident(n)} {i}")
    L += ["", "/* status kinds (move arg0 of status_tgt, EV_STATUS.a) */"]
    for n, i in STATUS.items():
        L.append(f"#define SK_{ident(n)} {i}")
    L += ["", "/* move effects */"]
    for n, i in EFFECTS.items():
        L.append(f"#define EF_{ident(n)} {i}")
    L += ["", "/* screens (move arg0 of EF_SCREEN) */"]
    for n, i in SCREENS.items():
        L.append(f"#define SCR_{ident(n)} {i}")
    L += ["", "/* move flags */", f"#define MF_HIGH_CRIT {MF_HIGH_CRIT}", f"#define MF_NEVER_MISS {MF_NEVER_MISS}",
          "#define MF_RECOIL 4  /* built-in struggle only */", f"#define MF_AI2 {MF_AI2}", f"#define MF_SPEC {MF_SPEC}", ""]

    def gl_text_ids():
        # the global text id of every `gl.` key = 0x8000 | its index among the global keys of es.txt, in file order (tools/mapc.py text_plan); {} when the master text is unreadable
        try:
            sys.path.insert(0, str(Path(__file__).resolve().parent))
            import textc
            gl = [k for k in textc.load() if textc.is_global(k)]
            return {k: 0x8000 | i for i, k in enumerate(gl)}
        except Exception as e:  # pragma: no cover
            print(f"regc: warning: text ids unavailable ({e})", file=sys.stderr)
            return {}

    def txt_array(name, prefix, keys, count, shared=False):
        if shared:
            # contract 63.1: the species / item name text ids are NOT a table any more: es.txt keeps the `gl.sp.*` (and the `gl.it.*`) texts consecutive in id order, so the id is
            # base + index - 1 (a static inline in reg_gen.h: no table, no function, nothing for the plugins to import). A key that is missing or out of order fails the build here.
            ids = gl_text_ids()
            want = [ids.get("gl." + prefix + "." + k) for k in keys]
            nm = prefix
            what = {"sp": "species", "mv": "move", "it": "item"}[prefix]
            src = {"sp": "species", "mv": "moves", "it": "items"}[prefix]
            if not ids:
                want = [0x8000 + i for i in range(len(want))]   # (master text unreadable: the warning above; a placeholder keeps the header well-formed)
            elif None in want or any(want[i] != want[0] + i for i in range(len(want))):
                bad = [k for k, v in zip(keys, want) if v is None]
                sys.exit(f"regc: the gl.{prefix}.* texts of assets/text/es.txt must be consecutive and in the order of assets/data/{src}.toml "
                         f"(contract 63.1 / 64: reg_{nm}_txt() is base + index - 1)" + (f"; missing: {', '.join(bad[:6])}" if bad else "") + "")
            first = f"TXT_{ident('gl.' + prefix + '.' + list(keys)[0])}"
            L.append(f"/* {name}: the name text id of {what} i = base + i - 1 (0xFFFF for 0 and out of range); es.txt keeps the gl.{prefix}.* texts consecutive (contract 63.1) */")
            L.append(f"#if defined({first}) && {first} != 0x{want[0]:04X}")
            L.append(f"#error reg_{nm}_txt() is out of date: run python3 tools/regc.py")
            L.append("#endif")
            L.append(f"static inline uint16_t reg_{nm}_txt(unsigned i) {{ return (i - 1u) < {count - 1}u ? (uint16_t)(0x{want[0]:04X}u + i - 1u) : (uint16_t)0xFFFF; }}")
            L.append("")
            return
        L.append(f"/* name text ids (0xFFFF while the key is not in assets/text/es.txt) */")
        L.append(f"REG_TABLE uint16_t {name}[{count}] = {{")
        out = L
        out.append("    0xFFFF,")
        for k in keys:
            m = f"TXT_{ident('gl.' + prefix + '.' + k)}"
            out.append(f"#ifdef {m}\n    {m},\n#else\n    0xFFFF,\n#endif")
        out.append("};")
        out.append("")

    txt_array("REG_SP_TXT", "sp", d["sid"].keys(), len(d["species"]) + 1, shared=True)
    # contract 64: the move names are arithmetic too (es.txt keeps the gl.mv.* texts consecutive in moves.toml order, in the block of the data), and so are the item names
    txt_array("REG_MV_TXT", "mv", d["mid"].keys(), len(d["moves"]) + 1, shared=True)
    txt_array("REG_IT_TXT", "it", d["iid"].keys(), len(d["items"]) + 1, shared=True)
    # type names: arithmetic (the 15 gl.ty.* texts are consecutive in type order; checked here)
    ids = gl_text_ids()
    tw = [ids.get("gl.ty." + k) for k in d["tid"]]
    if ids and (None in tw or any(tw[i] != tw[0] + i for i in range(len(tw)))):
        sys.exit("regc: the gl.ty.* texts of assets/text/es.txt must be consecutive and in the order of assets/data/types.toml (contract 64: reg_ty_txt() is base + index)")
    first = f"TXT_{ident('gl.ty.' + next(iter(d['tid'])))}"
    L.append("/* text id of the name of type n (type ids have no 'none' slot): arithmetic, see regc.py */")
    L.append(f"#if defined({first}) && {first} != 0x{(tw[0] if ids else 0x8000):04X}")
    L.append("#error reg_ty_txt() is out of date: run python3 tools/regc.py")
    L.append("#endif")
    L.append(f"static inline uint16_t reg_ty_txt(unsigned n) {{ return n < {len(tw)}u ? (uint16_t)(0x{(tw[0] if ids else 0x8000):04X}u + n) : (uint16_t)0xFFFF; }}")
    L.append("")
    return "\n".join(L)


def text_report(d):
    """warn about missing gl.sp/mv/ty keys in the master text file."""
    try:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import textc
        keys = set(textc.load())
    except Exception as e:  # pragma: no cover
        return [f"text check skipped ({e})"]
    want = [f"gl.sp.{n}" for n in d["sid"]] + [f"gl.mv.{n}" for n in d["mid"]] + \
           [f"gl.ty.{n}" for n in d["tid"]] + \
           [f"gl.it.{n}" for n in d["iid"]] + \
           [t["tname"] for t in d["trainers"] if t["tname"] != "rival"]
    miss = [k for k in want if k not in keys]
    return [f"missing text keys in es.txt ({len(miss)}/{len(want)}): " + ", ".join(miss[:8]) +
            (" ..." if len(miss) > 8 else "")] if miss else []


# ---- Registro data (contract section 30; C2-S2): files/reginfo.bin -------------------------------------------------------------
# NRGI v1, little-endian: "NRGI", u8 version 1, u8 n_entries (the Registro list), u8 n_rec (species ids incl. 0), u8 rec_size 8, u32 off_rank;
# then n_rec records INDEXED BY SPECIES ID: u16 height (decimetres), u16 weight (hectograms), u16 description text id (0xFFFF = none), u8 rank (1-based
# position in the list, 0 = not listed), u8 0; then at off_rank u8 species_of_rank[n_entries] (rank r -> species id), padded to 4. Nothing of it
# is resident: scene_reg.c (PC plugin) reads the rank table once and one record per entry it shows (section 27's rule).
REGINFO_REC = 8


def reginfo_build(d):
    """-> bytes of reginfo.bin. species.toml: height (m), weight (kg), optional reg = sort key; description = text key gl.rg.<species>."""
    sp = load_toml("species.toml")
    names = list(sp)
    keys = {}
    try:
        sys.path.insert(0, str(Path(__file__).resolve().parent))
        import textc
        glob = [k for k in textc.load() if textc.is_global(k)]
        keys = {k: 0x8000 | i for i, k in enumerate(glob)}
    except Exception as e:  # pragma: no cover
        print(f"regc: warning: reginfo: text keys unavailable ({e}); no descriptions", file=sys.stderr)
    order = []
    for i, n in enumerate(names, 1):
        rg = sp[n].get("reg", i)
        if isinstance(rg, bool) or not isinstance(rg, int) or not 1 <= rg <= 65535:
            raise RegError(f"species {n}: reg must be an integer 1..65535")
        order.append((rg, i))
    order.sort()
    rank = {sid: r for r, (_, sid) in enumerate(order, 1)}
    miss_hw, miss_tx = [], []
    recs = bytearray(b"\0" * REGINFO_REC)
    for i, n in enumerate(names, 1):
        s = sp[n]
        h, w = s.get("height"), s.get("weight")
        if h is None or w is None:
            miss_hw.append(n)
            h = h or 0
            w = w or 0
        for what, v in (("height", h), ("weight", w)):
            if isinstance(v, bool) or not isinstance(v, (int, float)) or v < 0 or round(v * 10) > 65535:
                raise RegError(f"species {n}: bad {what} {v!r}")
        t = keys.get("gl.rg." + n, 0xFFFF)
        if t == 0xFFFF:
            miss_tx.append(n)
        recs += struct.pack("<HHHBB", round(h * 10), round(w * 10), t, rank[i], 0)
    if miss_hw:
        print("regc: warning: Registro: no height/weight for " + ", ".join(miss_hw), file=sys.stderr)
    if miss_tx:
        print("regc: warning: Registro: no text gl.rg.<id> for " + ", ".join(miss_tx), file=sys.stderr)
    ranks = bytes(sid for _, sid in order)
    off_rank = 12 + len(recs)
    hdr = struct.pack("<4sBBBBI", b"NRGI", 1, len(names), len(names) + 1, REGINFO_REC, off_rank)
    return hdr + bytes(recs) + pad4(ranks)


def build_all():
    d = parse()
    hdr = emit_header(d)
    return d, {
        OUT_FILES / "reg.bin": emit_reg(d),
        OUT_FILES / "tm.bin": emit_tm(d),
        OUT_FILES / "aff.bin": emit_affinity(d),
        OUT_FILES / "reginfo.bin": reginfo_build(d),
        OUT_FILES / "evostone.bin": emit_evostone(d),
        OUT_FILES / "moves.bin": emit_moves(d),
        OUT_FILES / "trainers.bin": emit_trainers(d),
        OUT_FILES / "items.bin": emit_items(d),
        OUT_FILES / "ai.bin": emit_ai(d),
        OUT_GEN / "reg_gen.h": hdr.encode(),
    }


def main(argv):
    cmd = argv[1] if len(argv) > 1 else "build"
    try:
        d, outs = build_all()
    except (RegError, KeyError, tomllib.TOMLDecodeError) as e:
        print(f"regc: ERROR: {e!r}", file=sys.stderr)
        return 1
    if cmd == "check":
        bad = [p.name for p, b in outs.items() if not p.exists() or p.read_bytes() != b]
        if bad:
            print("regc: out of date: " + ", ".join(bad), file=sys.stderr)
            return 1
        print("regc: up to date")
    else:
        OUT_FILES.mkdir(parents=True, exist_ok=True)
        OUT_GEN.mkdir(parents=True, exist_ok=True)
        for p, b in outs.items():
            p.write_bytes(b)
            print(f"{p.relative_to(ROOT)}: {len(b)} bytes")
        print(f"{len(d['types'])} types, {len(d['chart'])} chart rows, {len(d['moves'])} moves, "
              f"{len(d['species'])} species, {len(d['trainers'])} trainers, {len(d['items'])} items")
    for w in text_report(d):
        print("regc: warning: " + w, file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
