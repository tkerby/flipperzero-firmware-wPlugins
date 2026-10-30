#!/usr/bin/env python3
"""Generate the boss teams (lib/pkb_core/gen/pb_trainers.h, trainers.c).

Species, levels and moves come from data/trainers/parties.asm and the class
DVs from data/trainers/dvs.asm, exactly as in Crystal; the held items are the
hand-picked ones in boss_items.py. Trainer mons have no stat exp and
BASE_HAPPINESS (engine/pokemon/move_mon.asm); boss_team.c applies that.

Usage: python tools/gen/gen_trainers.py [--check]
"""
import argparse
import sys
from collections import Counter, defaultdict

import asmlib
import boss_items
import gen_tables
import legality

# (category, trainer class, party number, name sent to the GB, menu label)
BOSSES = [
    ("GYM", "FALKNER", 1, None, "Falkner"),
    ("GYM", "BUGSY", 1, None, "Bugsy"),
    ("GYM", "WHITNEY", 1, None, "Whitney"),
    ("GYM", "MORTY", 1, None, "Morty"),
    ("GYM", "CHUCK", 1, None, "Chuck"),
    ("GYM", "JASMINE", 1, None, "Jasmine"),
    ("GYM", "PRYCE", 1, None, "Pryce"),
    ("GYM", "CLAIR", 1, None, "Clair"),
    ("GYM", "BROCK", 1, None, "Brock"),
    ("GYM", "MISTY", 1, None, "Misty"),
    ("GYM", "LT_SURGE", 1, None, "Lt. Surge"),
    ("GYM", "ERIKA", 1, None, "Erika"),
    ("GYM", "JANINE", 1, None, "Janine"),
    ("GYM", "SABRINA", 1, None, "Sabrina"),
    ("GYM", "BLAINE", 1, None, "Blaine"),
    ("GYM", "BLUE", 1, None, "Blue"),
    ("E4", "WILL", 1, None, "Will"),
    ("E4", "KOGA", 1, None, "Koga"),
    ("E4", "BRUNO", 1, None, "Bruno"),
    ("E4", "KAREN", 1, None, "Karen"),
    ("CHAMPION", "CHAMPION", 1, None, "Lance"),
    # the final rival battle (Indigo Plateau, after the Elite Four), one party
    # per rival starter; the rival's name is the player's choice in the game
    ("RIVAL", "RIVAL2", 4, "SILVER", "Meganium"),
    ("RIVAL", "RIVAL2", 5, "SILVER", "Typhlosion"),
    ("RIVAL", "RIVAL2", 6, "SILVER", "Feraligatr"),
    ("RED", "RED", 1, None, "Red"),
]
# Not a Crystal party. DVs are Red's (Atk 15, Def 13, Spe 13, Spc 14).
CUSTOM = [
    (
        "OAK",
        "POKEMON_PROF",
        "OAK",
        "Prof. Oak",
        (0xFD, 0xDE),
        [
            (
                79,
                "TAUROS",
                "LEFTOVERS",
                ("DOUBLE_EDGE", "EARTHQUAKE", "REST", "SLEEP_TALK"),
            ),
            (
                80,
                "EXEGGUTOR",
                "MIRACLEBERRY",
                ("SLEEP_POWDER", "PSYCHIC_M", "GIGA_DRAIN", "EXPLOSION"),
            ),
            (
                81,
                "ARCANINE",
                "LEFTOVERS",
                ("FLAMETHROWER", "EXTREMESPEED", "CRUNCH", "MORNING_SUN"),
            ),
            (
                82,
                "GYARADOS",
                "MYSTIC_WATER",
                ("RAIN_DANCE", "SURF", "THUNDER", "HYPER_BEAM"),
            ),
            (
                83,
                "VENUSAUR",
                "LEFTOVERS",
                ("SUNNY_DAY", "GIGA_DRAIN", "SYNTHESIS", "SOLARBEAM"),
            ),
            (
                85,
                "MEW",
                "LEFTOVERS",
                ("SOFTBOILED", "PSYCHIC_M", "ICE_BEAM", "THUNDERBOLT"),
            ),
        ],
    ),
]
CATEGORIES = ("GYM", "E4", "RIVAL", "RED", "CHAMPION", "OAK")


def parse_parties(data):
    """{class name: [(name, [(level, species, item, [moves])])]} in party order."""
    a = data.asm
    groups = [
        rest
        for w, rest in legality._statements(
            data.root / "data/trainers/party_pointers.asm"
        )
        if w == "dw"
    ]
    classes = [
        w_rest.split()[0]
        for w, w_rest in legality._statements(
            data.root / "constants/trainer_constants.asm"
        )
        if w == "trainerclass"
    ][
        1:
    ]  # skip TRAINER_NONE
    assert len(groups) == len(classes), (len(groups), len(classes))
    by_label = dict(zip(groups, classes))

    out, cur, party, ttype = {}, None, None, None
    for word, rest in legality._statements(data.root / "data/trainers/parties.asm"):
        if word == "label":
            cur = by_label.get(rest)
            if cur:
                out.setdefault(cur, [])
            continue
        if word != "db" or not cur:
            continue
        f = asmlib.split_args(rest)
        if f[0].startswith('"'):
            name = asmlib.unquote(f[0]).rstrip("@")
            ttype = f[1]
            party = (name, [])
            out[cur].append(party)
        elif a.eval(f[0]) in (-1, 0xFF):
            party = None
        elif party is not None:
            v = [a.eval(x) for x in f]
            level, species = v[0], v[1]
            item, moves = 0, [0, 0, 0, 0]
            if ttype == "TRAINERTYPE_ITEM":
                item = v[2]
            elif ttype == "TRAINERTYPE_MOVES":
                moves = v[2:6]
            elif ttype == "TRAINERTYPE_ITEM_MOVES":
                item, moves = v[2], v[3:7]
            party[1].append((level, species, item, moves))
    return out, classes


def parse_dvs(data, classes):
    a = data.asm
    rows = [
        [a.eval(x) for x in asmlib.split_args(rest)]
        for w, rest in legality._statements(data.root / "data/trainers/dvs.asm")
        if w == "dn"
    ]
    assert len(rows) >= len(classes), (len(rows), len(classes))
    return {c: ((r[0] << 4 | r[1]), (r[2] << 4 | r[3])) for c, r in zip(classes, rows)}


def build(data):
    a = data.asm
    parties, classes = parse_parties(data)
    dvs = parse_dvs(data, classes)
    held = [
        a.eval(asmlib.split_args(rest)[1]) & 0xFF
        for w, rest in legality._statements(data.root / "data/items/attributes.asm")
        if w == "item_attribute"
    ]
    species_items = {a.sym[n] for n in boss_items.SPECIES_ITEMS}
    bosses = []
    for cat, cls, num, sent_name, label in BOSSES:
        name, mons = parties[cls][num - 1]
        items = boss_items.BOSS_ITEMS[(cls, num)]
        if len(items) != len(mons):
            raise SystemExit(f"{cls} {num}: {len(mons)} mons but {len(items)} items")
        out_mons = []
        for (level, species, item, moves), item_name in zip(mons, items):
            assert item == 0, f"{cls} {num} already holds an item"
            if any(m == 0 for m in moves):  # NO_MOVE pads short movesets
                moves = [m for m in moves if m] + [0] * moves.count(0)
            item = a.sym[item_name]
            if not held[item - 1] and item not in species_items:
                raise SystemExit(f"{cls} {num}: {item_name} has no held effect")
            out_mons.append((level, species, item, moves))
        sent = sent_name or name
        assert 1 <= len(sent) <= 10, sent
        # trainerclass order, TRAINER_NONE skipped, matches TrainerPicPointers
        class_id = classes.index(cls) + 1
        bosses.append((cat, cls, num, sent, label, dvs[cls], out_mons, class_id))
    for cat, cls, sent, label, dv, mons in CUSTOM:
        assert 1 <= len(sent) <= 10, sent
        out_mons = []
        for level, species_name, item_name, move_names in mons:
            species = a.sym[species_name]
            item = a.sym[item_name]
            moves = [a.sym[m] for m in move_names]
            if not held[item - 1] and item not in species_items:
                raise SystemExit(f"{label}: {item_name} has no held effect")
            out_mons.append((level, species, item, moves))
        class_id = classes.index(cls) + 1
        bosses.append((cat, cls, 0, sent, label, dv, out_mons, class_id))
    return bosses


def trainer_class_labels(data, classes):
    """Menu text for trainer classes 1..N, in TrainerPicPointers order."""
    texts = [
        asmlib.unquote(rest)
        .replace("\u2642", "M")
        .replace("\u2640", "F")
        .replace("<PKMN>", "PKMN")
        for w, rest in legality._statements(data.root / "data/trainers/class_names.asm")
        if w == "li"
    ]
    if len(texts) != len(classes):
        raise SystemExit(f"class names {len(texts)} != classes {len(classes)}")
    bosses = defaultdict(list)
    for _cat, cls, _num, _sent, label in BOSSES:
        bosses[cls].append(label)
    for _cat, cls, _sent, label, _dv, _mons in CUSTOM:
        bosses[cls].append(label)
    shared = Counter(texts)
    labels = []
    for cls, text in zip(classes, texts):
        if cls == "RIVAL1":
            labels.append("Rival")
        elif cls == "RIVAL2":
            labels.append("Silver")
        elif cls in bosses and len(set(bosses[cls])) == 1:
            labels.append(bosses[cls][0])
        elif shared[text] == 1:
            labels.append(text)
        else:
            labels.append(f"{text} ({cls})")
    return labels


def gen_header(data, n_bosses, n_classes):
    return (
        gen_tables.header(data, "data/trainers/parties.asm, dvs.asm + boss_items.py")
        + """#pragma once

#include <stdint.h>

typedef enum {
"""
        + "".join(f"    PB_BOSS_{c},\n" for c in CATEGORIES)
        + """    PB_NUM_BOSS_CATEGORIES,
} PbBossCategory;

typedef struct {
    uint8_t level, species, item;
    uint8_t moves[4]; /* 0 = no move */
} PbBossMon;

typedef struct {
    const char* name;  /* sent to the Game Boy as the trainer and OT name */
    const char* label; /* menu text */
    uint8_t category;      /* PbBossCategory */
    uint8_t trainer_class; /* Crystal class id, TrainerPicPointers index */
    uint8_t dvs[2];        /* trainer class DVs, party struct order */
    uint8_t count;
    PbBossMon mon[6];
} PbBoss;

"""
        + f"#define PB_NUM_BOSSES {n_bosses}\n"
        + "extern const PbBoss pb_bosses[PB_NUM_BOSSES];\n"
        + f"#define PB_NUM_TRAINER_CLASSES {n_classes}\n"
        + "/* TrainerPicPointers order, class id 1 at index 0. */\n"
        + "extern const char* const pb_trainer_class_label[PB_NUM_TRAINER_CLASSES];\n"
    )


def gen_c(data, bosses, labels):
    out = [
        gen_tables.header(data, "data/trainers/parties.asm, dvs.asm + boss_items.py"),
        '#include "pb_trainers.h"\n#include "pb_const.h"\n',
        "const PbBoss pb_bosses[PB_NUM_BOSSES] = {",
    ]
    moves = data.moves_by_id
    for cat, cls, num, sent, label, dv, mons, class_id in bosses:
        out.append(
            f'    {{"{sent}", "{label}", PB_BOSS_{cat}, {class_id}, '
            f"{{0x{dv[0]:02X}, 0x{dv[1]:02X}}}, "
            f"{len(mons)}, {{ /* {cls} {num} */"
        )
        for level, species, item, mv in mons:
            ms = ", ".join(moves[m - 1] if m else "0" for m in mv)
            item_name = next(
                n
                for n, v in data.asm.sym.items()
                if v == item
                and data.asm.def_file.get(n) == "constants/item_constants.asm"
            )
            out.append(
                f"        {{{level}, {data.species[species - 1]}, {item_name}, {{{ms}}}}},"
            )
        out.append("    }},")
    out.append("};")
    out.append("\nconst char* const pb_trainer_class_label[PB_NUM_TRAINER_CLASSES] = {")
    for label in labels:
        out.append(f'    "{label}",')
    out.append("};\n")
    return "\n".join(out)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true", help="fail if outputs are stale")
    args = ap.parse_args()
    data = gen_tables.load()
    bosses = build(data)
    _parties, classes = parse_parties(data)
    labels = trainer_class_labels(data, classes)
    stale = gen_tables.write_or_check(
        gen_tables.OUT,
        {
            "pb_trainers.h": gen_header(data, len(bosses), len(labels)),
            "trainers.c": gen_c(data, bosses, labels),
        },
        args.check,
    )
    if args.check and stale:
        sys.exit(f"stale generated files: {stale}")
    print(
        ("checked" if args.check else "wrote"),
        "2 files;",
        "stale:" if stale else "no changes",
        *stale,
    )


if __name__ == "__main__":
    main()
