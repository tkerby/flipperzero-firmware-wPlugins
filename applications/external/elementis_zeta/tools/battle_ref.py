#!/usr/bin/env python3
"""Independent Python reference of the battle formulas (with the project's fixes).

It reads the TOML sources in assets/data (NOT the .bin files) and re-implements stats, damage, crits,
accuracy, experience and running from the written spec, so tools/host_battle can compare it with
nucleo/battle_core.c on thousands of random cases.

Usage: battle_ref.py compare <cases.txt> compare C output lines against this implementation
       battle_ref.py selftest a few hand-checked values
       battle_ref.py data nucleo/files/{reg,moves}.bin == the TOMLs for EVERY species / move (M4-E roster included)
Case line: KIND inputs... = outputs... (all integers)
PRIZE (trainer prize money, contract 16) and CURE (status-cure / key items) are the M4-A rules, computed here from trainers.toml / items.toml.
CATCH is the capture formula (contract 14.2); CPROB compares the turn engine's measured catch
frequency with the exact probability computed here by enumerating every pair of draws (n, m).
"""
import math
import sys
import tomllib
from pathlib import Path

DATA = Path(__file__).resolve().parent.parent / "assets" / "data"


def _toml(name):
    with open(DATA / name, "rb") as f:
        return tomllib.load(f)


_T = _toml("types.toml")
TYPES = [t["id"] for t in _T["type"]]
TID = {n: i for i, n in enumerate(TYPES)}
PHYSICAL = [bool(t["physical"]) for t in _T["type"]]
CHART = [(TID[a], TID[d], m) for a, d, m in _T["chart"]["rows"]]
TYPELESS = 255

_M = _toml("moves.toml")
MOVE_NAMES = list(_M)
MID = {n: i + 1 for i, n in enumerate(MOVE_NAMES)}
STATIDX = {"atk": 0, "def": 1, "spd": 2, "spc": 3, "acc": 4, "eva": 5}


def move(mid):
    """-> dict(type, power, acc255, high_crit, never_miss)"""
    m = _M[MOVE_NAMES[mid - 1]]
    return dict(type=TID[m["type"]], power=m["power"], acc=m["acc"] * 255 // 100,
                high_crit=bool(m.get("high_crit")), never_miss=bool(m.get("never_miss")))


_S = _toml("species.toml")
SPECIES_NAMES = list(_S)
SID = {n: i + 1 for i, n in enumerate(SPECIES_NAMES)}
GROWTH = {"medium_fast": 0, "medium_slow": 1, "fast": 2, "slow": 3}


def species(sid):
    s = _S[SPECIES_NAMES[sid - 1]]
    t = [TID[x] for x in s["types"]]
    return dict(t1=t[0], t2=t[-1], base=list(s["base"]), growth=GROWTH[s["growth"]], base_exp=s["base_exp"])


# ---- formulas ---------------------------------------------------------------------------------
def stat_calc(base, dv, statexp, level, is_hp):
    term = min(math.ceil(math.sqrt(statexp)), 255) // 4
    core = ((base + dv) * 2 + term) * level // 100
    v = core + level + 10 if is_hp else core + 5
    return min(v, 999)


def hp_dv(dv):
    return (dv[0] & 1) << 3 | (dv[1] & 1) << 2 | (dv[2] & 1) << 1 | (dv[3] & 1)


def mon_stats(sid, level, dv, sx):
    b = species(sid)["base"]
    return [stat_calc(b[0], hp_dv(dv), sx[0], level, True)] + \
           [stat_calc(b[i + 1], dv[i], sx[i + 1], level, False) for i in range(4)]


def exp_for_level(growth, n):
    if n <= 1:
        return 0
    if growth == 0:
        v = n ** 3
    elif growth == 2:
        v = 4 * n ** 3 // 5
    elif growth == 3:
        v = 5 * n ** 3 // 4
    else:
        v = 6 * n ** 3 // 5 - 15 * n * n + 100 * n - 140
    return max(v, 0)


def level_for_exp(growth, exp):
    lv = 1
    for n in range(2, 101):
        if exp >= exp_for_level(growth, n):
            lv = n
    return lv


def exp_gain(base_exp, level, trainer):
    e = base_exp * level // 7
    return e + e // 2 if trainer else e


RATIOS = {-6: (25, 100), -5: (28, 100), -4: (33, 100), -3: (40, 100), -2: (50, 100), -1: (66, 100), 0: (1, 1),
          1: (15, 10), 2: (2, 1), 3: (25, 10), 4: (3, 1), 5: (35, 10), 6: (4, 1)}


def stage_apply(v, stage):
    n, d = RATIOS[max(-6, min(6, stage))]
    return max(1, min(999, v * n // d))


def eff_stat(stat, unmod, stage, status):
    """stat 0 atk 1 def 2 spd 3 spc; status bits: 8 psn 16 brn 32 frz 64 par"""
    v = stage_apply(unmod, stage)
    if stat == 2 and status & 64:
        v = max(1, v // 4)
    if stat == 0 and status & 16:
        v = max(1, v // 2)
    return v


def type_eff(mt, t1, t2):
    e = 10
    for a, d, m in CHART:
        if a == mt and d in (t1, t2):
            e = e * m // 10
    return e


def damage(level, power, mt, a1, a2, d1, d2, A, D, crit, rnd):
    lvl = level * 2 if crit else level
    if A > 255 or D > 255:
        A //= 4
        D //= 4
    A, D = max(A, 1), max(D, 1)
    x = (lvl * 2 // 5 + 2) * power
    x = x * A // D // 50
    x = min(x, 997) + 2
    if mt == TYPELESS:
        return x, 10
    if mt in (a1, a2):
        x += x // 2
    eff = 10
    for a, d, m in CHART:
        if a == mt and d in (d1, d2):
            x = x * m // 10
            eff = eff * m // 10
    if x > 1:
        x = x * rnd // 255
    if eff > 0:
        x = max(x, 1)
    else:
        x = 0
    return x, eff


def crit_threshold(base_speed, high, focus):
    b = base_speed // 2
    t = min(255, b * 8) if high else b
    if focus:
        t = min(255, t * 4)
    return t


def _ratio255(v, stage):
    n, d = RATIOS[stage]
    return max(1, min(255, v * n // d))


def hit_threshold(acc, acc_stage, eva_stage, never):
    if never:
        return 256
    a = _ratio255(_ratio255(acc, acc_stage), -eva_stage)
    return 256 if a >= 255 else a


def run_threshold(p, e, attempts):
    if p >= e:
        return 256
    div = (e // 4) % 256
    if div == 0:
        return 256
    f = p * 32 // div
    if f > 255:
        return 256
    f += 30 * attempts
    return 256 if f > 255 else f


# ---- capture. status: Mon.status bits (sleep counter 1..7, 8 psn, 16 brn, 32 frz, 64 par).
BALL_RANGE = (255, 200, 150)   # crystal (= ball), great, ultra: the first draw is uniform in 0..range
BALL_DIV = (12, 8, 12)         # HP divisor of the f factor


def catch_ref(ball, rate, max_hp, hp, status, n, m):
    """-> (caught, shakes). n = first draw (0..range), m = second draw (0..255)."""
    if ball == 3:                                        # master
        return True, 3
    asleep_or_frozen = (status & 7) != 0 or (status & 32) != 0
    bonus = 25 if asleep_or_frozen else (12 if status else 0)
    if n - bonus < 0:                                    # went below zero: caught without the second draw
        return True, 3
    n -= bonus
    f = (max_hp * 255 // BALL_DIV[ball]) // max(1, hp // 4)
    f = min(f, 255)
    if n <= rate and m <= f:
        return True, 3
    x = min(255, rate * 100 // BALL_RANGE[ball])
    z = x * f // 255 + (5 if asleep_or_frozen else (10 if status else 0))
    return False, (0 if z < 10 else 1 if z < 30 else 2 if z < 70 else 3)


def catch_prob(ball, rate, max_hp, hp, status):
    """Exact probability (n uniform 0..range, m uniform 0..255), as a float."""
    if ball == 3:
        return 1.0
    rng = BALL_RANGE[ball]
    ok = sum(1 for n in range(rng + 1) for m in range(256) if catch_ref(ball, rate, max_hp, hp, status, n, m)[0])
    return ok / ((rng + 1) * 256)


def full_damage(sa, la, dva, sxa, sta, status_a, vol_a, sd, ld, dvd, sxd, std, status_d, vol_d, mv, crit, rnd):
    """VF_REFLECT = 8, VF_LSCREEN = 16 (battle_core.h)."""
    A_sp, D_sp, m = species(sa), species(sd), move(mv)
    stA, stD = mon_stats(sa, la, dva, sxa), mon_stats(sd, ld, dvd, sxd)
    phys = PHYSICAL[m["type"]]
    ai, di = (0, 1) if phys else (3, 3)
    if crit:
        A, D = stA[1 + ai], stD[1 + di]
    else:
        A = eff_stat(ai, stA[1 + ai], sta[ai], status_a)
        D = eff_stat(di, stD[1 + di], std[di], status_d)
        if vol_d & (8 if phys else 16):
            D *= 2
    if _M[MOVE_NAMES[mv - 1]].get("effect") == "suicide":  # contract 64: Blow Up / Explosion halve the target's Defense
        D >>= 1
    return damage(la, m["power"], m["type"], A_sp["t1"], A_sp["t2"], D_sp["t1"], D_sp["t2"], A, D, crit, rnd)


# ---- M4-A: prize money and status cures (contract 16) --------------------------------------------
_TR = _toml("trainers.toml")
TRAINER_NAMES = list(_TR)
_IT = _toml("items.toml")
ITEM_NAMES = list(_IT)
PRIZE_MAX = 999999
# items.toml status -> the bits of Mon.status that mean "has it" (sleep: the 3-bit counter; battle_core.h BS_*)
STATUS_MASK = {"slp": 0x07, "psn": 0x08, "brn": 0x10, "frz": 0x20, "par": 0x40}


def trainer_prize(tid):
    """base money x the level of the LAST creature of the party (capped at 999,999)."""
    t = _TR[TRAINER_NAMES[tid - 1]]
    return min(PRIZE_MAX, int(t.get("money", 0)) * t["party"][-1][1])


def cure_ref(item, status, hp):
    """-> (accepted, cured SK kind). Cure item: a living creature with exactly that status; a key item is never usable."""
    x = _IT[ITEM_NAMES[item - 1]]
    if x["kind"] != "cure":
        return False, 0
    sk = {"slp": 1, "psn": 2, "brn": 3, "frz": 4, "par": 5}[x["status"]]
    ok = hp > 0 and (status & STATUS_MASK[x["status"]]) != 0
    return ok, sk if ok else 0


# ---- compare ----------------------------------------------------------------------------------
def check(kind, a, n_out):
    """-> list of expected outputs for the inputs `a`."""
    if kind == "STAT":
        return [stat_calc(a[0], a[1], a[2], a[3], bool(a[4]))]
    if kind == "FMON":
        return mon_stats(a[0], a[1], a[2:6], a[6:11])
    if kind == "EXPL":
        return [exp_for_level(a[0], a[1])]
    if kind == "LVLX":
        return [level_for_exp(a[0], a[1])]
    if kind == "GAIN":
        return [exp_gain(a[0], a[1], bool(a[2]))]
    if kind == "EFF":
        return [type_eff(a[0], a[1], a[2])]
    if kind == "DMG":
        return list(damage(a[0], a[1], a[2], a[3], a[4], a[5], a[6], a[7], a[8], bool(a[9]), a[10]))
    if kind == "CRIT":
        return [crit_threshold(a[0], bool(a[1]), bool(a[2]))]
    if kind == "HIT":
        return [hit_threshold(a[0], a[1], a[2], bool(a[3]))]
    if kind == "RUN":
        return [run_threshold(a[0], a[1], a[2])]
    if kind == "CATCH":
        ok, sh = catch_ref(*a)
        return [int(ok), sh]
    if kind == "PRIZE":
        return [1, trainer_prize(a[0])]
    if kind == "CURE":
        ok, sk = cure_ref(*a)
        return [int(ok), sk]
    if kind == "ESTAT":
        return [eff_stat(a[0], a[1], a[2], a[3])]
    if kind == "FDMG":
        # sa la dv4 sx5 st6 status vol | sd ld dv4 sx5 st6 status vol | mv crit rnd
        o = 0
        def mon(o):
            return (a[o], a[o + 1], a[o + 2:o + 6], a[o + 6:o + 11], a[o + 11:o + 17], a[o + 17], a[o + 18]), o + 19
        (sa, la, dva, sxa, sta, sa_s, va), o = mon(0)
        (sd, ld, dvd, sxd, std, sd_s, vd), o = mon(o)
        mv, crit, rnd = a[o], a[o + 1], a[o + 2]
        return list(full_damage(sa, la, dva, sxa, sta, sa_s, va, sd, ld, dvd, sxd, std, sd_s, vd, mv, bool(crit), rnd))
    raise ValueError(kind)


def compare(path):
    n = bad = 0
    kinds = {}
    for ln, line in enumerate(open(path), 1):
        line = line.strip()
        if not line or line.startswith("#"):
            continue
        lhs, rhs = line.split("=")
        toks = lhs.split()
        kind, args = toks[0], [int(x) for x in toks[1:]]
        got = [int(x) for x in rhs.split()]
        n += 1
        kinds[kind] = kinds.get(kind, 0) + 1
        if kind == "CPROB":  # ball rate max_hp hp status trials = caught_count: within 5 sigma of the exact probability
            ball, rate, mx, hp, st, trials = args
            p = catch_prob(ball, rate, mx, hp, st)
            sigma = (trials * p * (1 - p)) ** 0.5
            if abs(got[0] - trials * p) > 5 * sigma + 1:
                bad += 1
                print(f"MISMATCH line {ln}: {line}\n    exact probability {p:.4f} -> expected ~{trials * p:.1f} +- {sigma:.1f}")
            continue
        want = check(kind, args, len(got))
        if want != got:
            bad += 1
            if bad <= 20:
                print(f"MISMATCH line {ln}: {line}\n    python says {want}")
    summary = ", ".join(f"{k}={v}" for k, v in sorted(kinds.items()))
    print(f"compared {n} cases ({summary}): {bad} mismatches")
    return 1 if bad else 0


def selftest():
    # known values
    assert exp_for_level(1, 2) == 9 and exp_for_level(1, 5) == 135 and exp_for_level(1, 100) == 1059860
    assert exp_for_level(0, 100) == 1000000 and exp_for_level(2, 100) == 800000 and exp_for_level(3, 100) == 1250000
    assert type_eff(TID["ghost"], TID["psychic"], TID["psychic"]) == 20  # FIX
    assert type_eff(TID["ice"], TID["fire"], TID["fire"]) == 10
    assert type_eff(TID["water"], TID["fire"], TID["rock"]) == 40
    assert hit_threshold(255, 0, 0, False) == 256
    assert crit_threshold(100, False, True) == 200 and crit_threshold(100, True, True) == 255
    assert damage(5, 40, TID["normal"], 0, 0, 0, 0, 300, 3, False, 255)[0] >= 1
    # capture (hand-computed from the text): rate 45, 20/20 HP -> f = 85, X = 17, Z = 5
    assert catch_ref(0, 45, 20, 20, 0, 40, 85) == (True, 3) and catch_ref(0, 45, 20, 20, 0, 40, 86) == (False, 0)
    assert catch_ref(0, 45, 20, 1, 0, 200, 0) == (False, 1)                 # 1 HP: HP/4 -> 1, f = 255, Z = 17
    assert catch_ref(0, 45, 20, 20, 32, 24, 255) == (True, 3)               # frozen: 24 - 25 < 0
    assert catch_ref(0, 45, 20, 20, 64, 200, 255) == (False, 1)             # paralysed: Z = 5 + 10
    assert abs(catch_prob(0, 255, 100, 1, 0) - 1.0) < 1e-9                  # rate 255, 1 HP: certain
    assert 0.0 < catch_prob(0, 45, 20, 20, 0) < 0.1
    # M4-A: prize = base money x last creature's level; rival_lab pays 35 x its (only) level-5 creature
    assert trainer_prize(1) == int(_TR[TRAINER_NAMES[0]]["money"]) * _TR[TRAINER_NAMES[0]]["party"][-1][1]
    assert cure_ref(ITEM_NAMES.index("antidote") + 1, 0x08, 10) == (True, 2)
    assert cure_ref(ITEM_NAMES.index("antidote") + 1, 0x10, 10) == (False, 0)
    assert cure_ref(ITEM_NAMES.index("wake_salts") + 1, 3, 10) == (True, 1)
    assert cure_ref(ITEM_NAMES.index("antidote") + 1, 0x08, 0) == (False, 0)
    assert cure_ref(ITEM_NAMES.index("parcel") + 1, 0x08, 10) == (False, 0)
    print("battle_ref selftest ok")
    return 0


def data_check():
    """Decode reg.bin / moves.bin straight from the bytes (contract 7.1) and compare every field with species.toml / moves.toml."""
    import struct
    files = Path(__file__).resolve().parent.parent / "nucleo" / "files"
    errs = []
    d = (files / "reg.bin").read_bytes()
    magic, ver, n, rec, _, size = struct.unpack_from("<4sBBBBI", d, 0)
    if magic != b"NREG" or n != len(SPECIES_NAMES) + 1 or rec != 40 or size != len(d):
        errs.append(f"reg.bin header {magic} n={n} (want {len(SPECIES_NAMES) + 1})")
    for sid, name in enumerate(SPECIES_NAMES, 1):
        t = _S[name]
        r = d[12 + 40 * sid:12 + 40 * (sid + 1)]
        ty = [TID[x] for x in t["types"]]
        want_ev = (t["evolve"][0], SID[t["evolve"][1]]) if "evolve" in t else (0, 0)
        if "evolve_friend" in t:  # contract 65: reg.bin evo_level = the level floor | 0x80 (the friendship mark), evo_to = the target
            want_ev = (t["evolve_friend"][0] | 0x80, SID[t["evolve_friend"][1]])
        got = dict(types=(r[0], r[1]), base=list(r[2:7]), growth=r[7], base_exp=r[8], catch=r[9], ev=(r[10], r[11]))
        exp = dict(types=(ty[0], ty[-1]), base=list(t["base"]), growth=GROWTH[t["growth"]], base_exp=t["base_exp"],
                   catch=t.get("catch_rate", 45), ev=want_ev)
        if got != exp:
            errs.append(f"{name}: {got} != {exp}")
        nl = r[14]
        lv, mv = list(r[16:16 + nl]), list(r[28:28 + nl])
        if [(a, MID[b]) for a, b in t["learn"]] != list(zip(lv, mv)) or any(not 1 <= m <= len(MOVE_NAMES) for m in mv):
            errs.append(f"{name}: learnset mismatch")
        if "evolve" in t and not 1 <= t["evolve"][0] <= 100:
            errs.append(f"{name}: bad evolve level")
    m = (files / "moves.bin").read_bytes()
    magic, ver, nm, nt, nc, off_m, off_t, off_c, size = struct.unpack_from("<4sBBBBIIII", m, 0)
    if magic != b"NMOV" or nm != len(MOVE_NAMES) + 1 or size != len(m):
        errs.append(f"moves.bin header n={nm}")
    for mid_, name in enumerate(MOVE_NAMES, 1):
        t = _M[name]
        r = m[off_m + 12 * mid_:off_m + 12 * (mid_ + 1)]
        ty, pw, ac, pp, ef, a0, a1, ch, pr, fl = struct.unpack_from("<BBBBBBbBbB", r, 0)
        eff = t.get("effect", "none")
        want_eff = {"none": 0, "stat_tgt": 1, "stat_self": 2, "status_tgt": 3, "focus": 4, "screen": 5, "flinch": 6,
                    "multi": 7, "fixed": 8, "ohko": 9, "recoil": 10, "drain": 11, "heal": 12, "suicide": 13, "charge": 14, "lock": 15,
                    "flee": 16, "stat_reset": 17, "mist": 18, "leech": 19, "counter": 20, "move_call": 21, "store_dmg": 22, "substitute": 23,
                    "recharge": 24, "crash": 25, "disable": 26, "convert": 27, "rage": 28, "transform": 29, "coin_hit": 30}[eff]  # contract 73 (+ coin_hit: contract 82)
        want_a = (t.get("arg0", 0), t.get("arg1", 0)) if want_eff >= 7 else \
            (STATIDX[t["stat"]], t["stages"]) if eff in ("stat_tgt", "stat_self") else \
            ({"sleep": 1, "poison": 2, "burn": 3, "freeze": 4, "paralysis": 5, "confusion": 6}[t["status"]], 1 if t.get("toxic") else 0) if eff == "status_tgt" else \
            ({"reflect": 0, "light_screen": 1}[t["screen"]], 0) if eff == "screen" else (0, 0)   # C3: the first move with the screen effect (Veil = Light Veil)
        pct = t.get("chance")
        want_ch = 0 if pct is None or pct >= 100 else min(255, -(-pct * 256 // 100))
        want_fl = (1 if t.get("high_crit") else 0) | (2 if t.get("never_miss") else 0) | \
            (0x10 if eff in ("stat_self", "heal", "screen", "stat_reset", "convert", "transform", "coin_hit") or (eff == "stat_tgt" and t["power"] == 0) else 0) | \
            (0x20 if eff == "fixed" or (eff == "charge" and t.get("arg0", 0) & 1) else 0)   # contract 82: AI modifier 2 encourages / modifier 3 counts as a better move
        got = (ty, pw, ac, pp, ef, (a0, a1), ch, pr, fl)
        exp = (TID[t["type"]], t["power"], t["acc"] * 255 // 100, t["pp"], want_eff, want_a, want_ch, t.get("priority", 0), want_fl)
        if got != exp:
            errs.append(f"move {name}: {got} != {exp}")
    # C2-S1 (contract 29): items.bin records (all item kinds) and the species technique-disc masks, decoded from the bytes
    it = (files / "items.bin").read_bytes()
    magic, ver, ni, irec, _, size = struct.unpack_from("<4sBBBBI", it, 0)
    if magic != b"NITM" or ni != len(ITEM_NAMES) + 1 or irec != 8 or size != len(it):
        errs.append(f"items.bin header {magic} n={ni} (want {len(ITEM_NAMES) + 1})")
    kinds = {"catch": 1, "heal": 2, "cure": 3, "key": 4, "repel": 5, "rope": 6, "disc": 7, "stone": 8, "vitamin": 9, "revive": 10, "restore": 11, "candy": 12, "elixir": 13, "battle": 14, "ppup": 15, "ether": 16}
    stones = {"moon": 0, "fire": 1, "water": 2, "thunder": 3, "leaf": 4}
    vstat = {"hp": 0, "atk": 1, "def": 2, "spd": 3, "spc": 4}
    balls = {"crystal": 0, "great": 1, "ultra": 2, "master": 3}
    stat = {"slp": 1, "psn": 2, "brn": 3, "frz": 4, "par": 5}
    disc_bit = {}
    for iid, name in enumerate(ITEM_NAMES, 1):
        x = _IT[name]
        xk = "stone" if x["kind"] == "treasure" else x["kind"]   # contract 80: a treasure (the Gold Chunk) is an IK_STONE with the stone index ST_NONE 7 (no species evolves with it)
        k, arg, aux, z, price, z2 = struct.unpack_from("<BBBBHH", it, 12 + 8 * iid)
        want_arg = {"catch": lambda: balls[x.get("ball", "crystal")], "heal": lambda: x["hp"], "cure": lambda: 0 if x["status"] == "all" else stat[x["status"]],
                    "repel": lambda: x["steps"], "disc": lambda: MID[x["move"]], "key": lambda: 0, "rope": lambda: 0,
                    "stone": lambda: 7 if x["kind"] == "treasure" else stones[x["stone"]], "vitamin": lambda: vstat[x["stat"]],
                    "revive": lambda: 255 if x.get("full") else 0, "restore": lambda: 0, "candy": lambda: 0, "elixir": lambda: x["pp"],
                    "battle": lambda: {"mist": 5, "focus": 6}[x["effect"]] if "effect" in x else STATIDX[x["stat"]], "ppup": lambda: 0, "ether": lambda: x["pp"]}[xk]()
        want_aux = 0
        if x["kind"] == "disc":
            want_aux = disc_bit[name] = len(disc_bit)
        if (k, arg, aux, z, price, z2) != (kinds[xk], want_arg, want_aux, 0, x.get("price", 0), 0):
            errs.append(f"item {name}: {(k, arg, aux, z, price, z2)} != {(kinds[xk], want_arg, want_aux, 0, x.get('price', 0), 0)}")
    tmb = (files / "tm.bin").read_bytes()
    magic, ver, ntm, trec, _, size = struct.unpack_from("<4sBBBBI", tmb, 0)
    if magic != b"NTM1" or ntm != len(SPECIES_NAMES) + 1 or trec != 8 or size != len(tmb) or len(disc_bit) > 64:
        errs.append(f"tm.bin header {magic} n={ntm} rec={trec}")
    for sid, name in enumerate(SPECIES_NAMES, 1):
        r = d[12 + 40 * sid:12 + 40 * (sid + 1)]
        got = r[12] | r[13] << 8 | r[15] << 16
        want = sum(1 << disc_bit[k] for k in _S[name].get("tm", []))
        if got != want & 0xFFFFFF:
            errs.append(f"{name}: tm mask {got:#x} != {want & 0xFFFFFF:#x}")
        got64 = struct.unpack_from("<Q", tmb, 12 + 8 * sid)[0]  # contract 64: tm.bin holds all the discs
        if got64 != want:
            errs.append(f"{name}: tm.bin mask {got64:#x} != {want:#x}")
    # C4-B (contract 36): evostone.bin = the species.toml `evolve_stone` tables, decoded from the bytes
    ev = (files / "evostone.bin").read_bytes()
    magic, ver, ne, erec, _, size = struct.unpack_from("<4sBBBBI", ev, 0)
    want_ev = []
    es_key = {"luna": "moon", "fuego": "fire", "agua": "water", "trueno": "thunder", "hoja": "leaf"}
    for sid, name in enumerate(SPECIES_NAMES, 1):
        for k, tgt in sorted(_S[name].get("evolve_stone", {}).items(), key=lambda kv: stones[es_key.get(kv[0], kv[0])]):
            want_ev.append((sid, stones[es_key.get(k, k)], SID[tgt], 0))
        if "evolve_trade" in _S[name]:  # IR LINK (contract 45): the trade evolution is the record with stone index 5
            want_ev.append((sid, 5, SID[_S[name]["evolve_trade"]], 0))
    got_ev = [struct.unpack_from("<BBBB", ev, 12 + 4 * i) for i in range(ne)]
    if magic != b"NEVS" or erec != 4 or size != len(ev) or got_ev != want_ev:
        errs.append(f"evostone.bin {magic} n={ne}: {got_ev} != {want_ev}")
    for e in errs:
        print("battle_ref data: " + e)
    print(f"battle_ref data: {len(SPECIES_NAMES)} species, {len(MOVE_NAMES)} moves, {len(ITEM_NAMES)} items: " + ("OK" if not errs else f"{len(errs)} ERRORS"))
    return 1 if errs else 0


if __name__ == "__main__":
    if len(sys.argv) >= 2 and sys.argv[1] == "data":
        sys.exit(data_check())
    if len(sys.argv) >= 3 and sys.argv[1] == "compare":
        sys.exit(compare(sys.argv[2]))
    if len(sys.argv) >= 2 and sys.argv[1] == "selftest":
        sys.exit(selftest())
    print(__doc__)
    sys.exit(2)
