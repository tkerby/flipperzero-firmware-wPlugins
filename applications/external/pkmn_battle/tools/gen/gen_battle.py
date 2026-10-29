"""Battle-engine outputs for gen_tables.py.

  pb_wram.h        WRAM/HRAM addresses from pokecrystal.sym (symbols branch),
                   so the engine's shadow memory uses the game's real layout
  pb_battle.h      battle command ids and declarations for battle_tables.c
  battle_tables.c  type matchups, critical-hit, stat/accuracy multiplier and
                   priority tables, held-item effects, and every move effect
                   script as the command bytes DoMove copies into
                   wBattleScriptBuffer
"""

import pathlib
import re

import asmlib
import fetch_pokecrystal
from legality import _statements

ITEM_TABLE_LEN = 255  # GetItemHeldEffect indexes with item - 1


def _db_bytes(a, rest):
    return [a.eval(x) & 0xFF for x in asmlib.split_args(rest)]


def _labelled_dbs(a, path):
    """{label: [bytes...]} for a file of labels followed by db lines."""
    out, cur = {}, None
    for word, rest in _statements(path):
        if word == "label":
            cur = out.setdefault(rest, [])
        elif word == "db":
            if cur is None:
                cur = out.setdefault("", [])
            cur += _db_bytes(a, rest)
    return out


def gen_wram(data, header):
    sym = fetch_pokecrystal.symbols_dir() / "pokecrystal.sym"
    rows = []
    for line in sym.read_text(encoding="utf-8").splitlines():
        m = re.fullmatch(r"([0-9a-f]{2}):([0-9a-f]{4}) (\S+)", line.strip())
        if not m or "." in m.group(3):
            continue
        bank, addr, name = int(m.group(1), 16), int(m.group(2), 16), m.group(3)
        wram0 = bank == 0 and 0xC000 <= addr < 0xD000
        wramx = bank == 1 and 0xD000 <= addr < 0xE000
        hram = bank == 0 and 0xFF80 <= addr <= 0xFFFF and name.startswith("h")
        if wram0 or wramx or hram:
            rows.append((addr, name))
    rows.sort()
    lines = [
        header("pokecrystal.sym from the symbols branch, WRAM0/WRAMX bank 1/HRAM"),
        "#pragma once\n",
        "/* WRAM $C000-$DFFF (bank 1 at $D000) and HRAM $FF80-$FFFF, as the game lays them out. */",
        "#define PB_WRAM_START 0xC000",
        "#define PB_WRAM_SIZE 0x2000",
        "#define PB_HRAM_START 0xFF80",
        "#define PB_HRAM_SIZE 0x80\n",
    ]
    lines += [f"#define {name} 0x{addr:04X}" for addr, name in rows]
    return "\n".join(lines) + "\n"


def _effect_scripts(data):
    a, root = data.asm, data.root
    stream, labels = [], {}
    for word, rest in _statements(root / "data/moves/effects.asm"):
        if word == "label":
            labels[rest] = len(stream)
        elif word in ("INCLUDE",):
            continue
        else:
            cmd = f"{word}_command"
            if cmd not in a.sym:
                raise SystemExit(f"effects.asm: unknown command {word}")
            stream.append(a.sym[cmd] & 0xFF)
    pointers = [
        rest
        for word, rest in _statements(root / "data/moves/effects_pointers.asm")
        if word == "dw"
    ]
    blob, offs = bytearray(), []
    for name in pointers:
        start = labels[name]
        end = stream.index(a.sym["endmove_command"] & 0xFF, start)
        offs.append(len(blob))
        blob += bytes(stream[start : end + 1])
    offs.append(len(blob))
    return pointers, blob, offs


def _commands(data):
    names = [
        rest.split()[0]
        for word, rest in _statements(data.root / "macros/scripts/battle_commands.asm")
        if word == "command"
    ]
    return [(n, data.asm.sym[f"{n}_command"] & 0xFF) for n in names]


BATTLE_H = """\
#pragma once

#include <stdint.h>
#include "pb_const.h"

/* Battle command ids (macros/scripts/battle_commands.asm). */
{commands}

/* data/types/type_matchups.asm: (attacker, defender, multiplier) triples;
 * $FE marks the Foresight-sensitive tail, $FF the end. */
extern const uint8_t pb_type_matchups[];
/* data/moves/critical_hit_moves.asm, $FF-terminated */
extern const uint8_t pb_critical_hit_moves[];
/* data/battle/critical_hit_chances.asm */
extern const uint8_t pb_critical_hit_chances[{crit_len}];
/* data/battle/stat_multipliers.asm and accuracy_multipliers.asm: (num, den) for -6..+6 */
extern const uint8_t pb_stat_level_multipliers[13][2];
extern const uint8_t pb_accuracy_level_multipliers[13][2];
/* data/moves/effects_priorities.asm: (effect, priority) pairs, $FF-terminated */
extern const uint8_t pb_move_effect_priorities[];
/* data/battle/weather_modifiers.asm: (weather, type or effect, multiplier), $FF-terminated */
extern const uint8_t pb_weather_type_modifiers[];
extern const uint8_t pb_weather_move_modifiers[];
/* data/types/type_boost_items.asm: (held effect, type) pairs, $FF-terminated */
extern const uint8_t pb_type_boost_items[];
/* data/battle/held_heal_status.asm: (held effect, status mask) pairs, $FF-terminated */
extern const uint8_t pb_held_heal_status[];
/* data/battle/held_stat_up.asm: the held effects (their commands are
 * BattleCommand_AttackUp..EvasionUp in order), $FF-terminated */
extern const uint8_t pb_held_stat_up[];
/* data/battle/held_consumables.asm: the held effects ConsumeHeldItem removes, $FF-terminated */
extern const uint8_t pb_held_consumables[];
/* data/items/attributes.asm: held effect and parameter of item id - 1 */
extern const uint8_t pb_item_held_effect[{items}][2];
/* data/moves/flail_reversal_power.asm: (HP bar pixels, power) pairs */
extern const uint8_t pb_flail_reversal_power[{flail}];
/* data/moves/magnitude_power.asm: (chance, power, magnitude) triples */
extern const uint8_t pb_magnitude_power[{magnitude}];
/* data/moves/metronome_exception_moves.asm, $FF-terminated */
extern const uint8_t pb_metronome_excepts[];
/* data/moves/present_power.asm: (chance, power) pairs, $FF-terminated */
extern const uint8_t pb_present_power[];
/* data/items/mail_items.asm, $FF-terminated */
extern const uint8_t pb_mail_items[];
/* Move effect scripts: effect e's commands are
 * pb_move_effect_scripts[pb_move_effect_script_off[e] ..], ending with endmove. */
extern const uint8_t pb_move_effect_scripts[];
extern const uint16_t pb_move_effect_script_off[NUM_MOVE_EFFECTS + 1];
"""


def gen_battle_h(data, header):
    cmds = "\n".join(
        f"#define PB_CMD_{n.upper()} 0x{v:02X}" for n, v in _commands(data)
    )
    crit = _labelled_dbs(data.asm, data.root / "data/battle/critical_hit_chances.asm")[
        "CriticalHitChances"
    ]
    return header("battle commands, tables") + BATTLE_H.format(
        commands=cmds,
        crit_len=len(crit),
        items=ITEM_TABLE_LEN,
        flail=len(_flail(data)),
        magnitude=len(_magnitude(data)),
    )


def _flail(data):
    return _labelled_dbs(data.asm, data.root / "data/moves/flail_reversal_power.asm")[
        "FlailReversalPower"
    ]


def _magnitude(data):
    return _labelled_dbs(data.asm, data.root / "data/moves/magnitude_power.asm")[
        "MagnitudePower"
    ]


def _carray(ctype, name, values, dims=""):
    body = ", ".join(f"0x{v:02X}" for v in values)
    wrapped = "\n".join(
        "    " + body[i : i + 96].strip() for i in range(0, len(body), 96)
    )
    return f"const {ctype} {name}[]{dims} = {{\n{wrapped}\n}};\n"


def gen_battle_c(data, header):
    a, root = data.asm, data.root
    out = [
        header("data/types, data/moves, data/battle, data/items"),
        '#include "pb_battle.h"\n',
    ]

    tm = _labelled_dbs(a, root / "data/types/type_matchups.asm")["TypeMatchups"]
    out.append(_carray("uint8_t", "pb_type_matchups", tm))
    out.append(
        _carray(
            "uint8_t",
            "pb_critical_hit_moves",
            _labelled_dbs(a, root / "data/moves/critical_hit_moves.asm")[
                "CriticalHitMoves"
            ],
        )
    )
    crit = _labelled_dbs(a, root / "data/battle/critical_hit_chances.asm")[
        "CriticalHitChances"
    ]
    out.append(
        f"const uint8_t pb_critical_hit_chances[{len(crit)}] = {{"
        + ", ".join(str(v) for v in crit)
        + "};\n"
    )
    for name, rel in (
        ("pb_stat_level_multipliers", "data/battle/stat_multipliers.asm"),
        ("pb_accuracy_level_multipliers", "data/battle/accuracy_multipliers.asm"),
    ):
        vals = sum(_labelled_dbs(a, root / rel).values(), [])
        assert len(vals) == 26, (rel, len(vals))
        rows = ", ".join(f"{{{vals[i]}, {vals[i + 1]}}}" for i in range(0, 26, 2))
        out.append(f"const uint8_t {name}[13][2] = {{{rows}}};\n")
    out.append(
        _carray(
            "uint8_t",
            "pb_move_effect_priorities",
            _labelled_dbs(a, root / "data/moves/effects_priorities.asm")[
                "MoveEffectPriorities"
            ],
        )
    )
    wm = _labelled_dbs(a, root / "data/battle/weather_modifiers.asm")
    out.append(
        _carray("uint8_t", "pb_weather_type_modifiers", wm["WeatherTypeModifiers"])
    )
    out.append(
        _carray("uint8_t", "pb_weather_move_modifiers", wm["WeatherMoveModifiers"])
    )
    out.append(
        _carray(
            "uint8_t",
            "pb_type_boost_items",
            _labelled_dbs(a, root / "data/types/type_boost_items.asm")[
                "TypeBoostItems"
            ],
        )
    )

    out.append(
        _carray(
            "uint8_t",
            "pb_held_heal_status",
            _labelled_dbs(a, root / "data/battle/held_heal_status.asm")[
                "HeldStatusHealingEffects"
            ],
        )
    )
    stat_up = [
        a.eval(asmlib.split_args(rest)[0]) & 0xFF
        for word, rest in _statements(root / "data/battle/held_stat_up.asm")
        if word == "dbw"
    ]
    out.append(_carray("uint8_t", "pb_held_stat_up", stat_up + [0xFF]))
    out.append(
        _carray(
            "uint8_t",
            "pb_held_consumables",
            _labelled_dbs(a, root / "data/battle/held_consumables.asm")[
                "ConsumableEffects"
            ],
        )
    )

    items = []
    for word, rest in _statements(root / "data/items/attributes.asm"):
        if word == "item_attribute":
            f = asmlib.split_args(rest)
            items.append((a.eval(f[1]) & 0xFF, a.eval(f[2]) & 0xFF))
    items = items[:ITEM_TABLE_LEN]
    assert len(items) == ITEM_TABLE_LEN, len(items)
    rows = ",\n".join(
        "    " + ", ".join(f"{{{e}, {p}}}" for e, p in items[i : i + 8])
        for i in range(0, len(items), 8)
    )
    out.append(
        f"const uint8_t pb_item_held_effect[{ITEM_TABLE_LEN}][2] = {{\n{rows},\n}};\n"
    )
    out.append(_carray("uint8_t", "pb_flail_reversal_power", _flail(data)))
    out.append(_carray("uint8_t", "pb_magnitude_power", _magnitude(data)))
    out.append(
        _carray(
            "uint8_t",
            "pb_metronome_excepts",
            _labelled_dbs(a, root / "data/moves/metronome_exception_moves.asm")[
                "MetronomeExcepts"
            ],
        )
    )
    out.append(
        _carray(
            "uint8_t",
            "pb_present_power",
            _labelled_dbs(a, root / "data/moves/present_power.asm")["PresentPower"],
        )
    )
    out.append(
        _carray(
            "uint8_t",
            "pb_mail_items",
            _labelled_dbs(a, root / "data/items/mail_items.asm")["MailItems"],
        )
    )

    names, blob, offs = _effect_scripts(data)
    assert len(names) == a.sym["NUM_MOVE_EFFECTS"], (
        len(names),
        a.sym["NUM_MOVE_EFFECTS"],
    )
    out.append(_carray("uint8_t", "pb_move_effect_scripts", blob))
    rows = ",\n".join(
        "    " + ", ".join(str(o) for o in offs[i : i + 12])
        for i in range(0, len(offs), 12)
    )
    out.append(
        f"const uint16_t pb_move_effect_script_off[NUM_MOVE_EFFECTS + 1] = {{\n{rows},\n}};\n"
    )
    return "\n".join(out)
