#pragma once
// Solar / Lunar affinity (contract 66): our two-version split inside one game. PartyState.aff (party.h AFF_*) is chosen at the start (UI plugin, scene_name.c) and decides which wild
// creature a map's wild table gives: the tables list the SOLAR form of each pair of assets/data/affinity.toml and a LUNAR player meets its counterpart in the same slot. Everything else
// (gifts, trainers, in-game trades, fossils, the casino, evolutions, the Registro) is the same for both.
//
// files/aff.bin (tools/regc.py emit_affinity): a 12-byte header ("NAFF", version, n, pairs, 0, size) and then one byte per species id: the species a LUNAR player meets where the table says this
// one (the same id when it has no counterpart: the view is total, so the reader needs no zero test). Header-inline like bc_stone_evo: the battle plugin reads ONE byte from the card (read_at,
// which it already imports) when a wild battle opens, so there is no resident code, no table in RAM and no API-table entry. The Solar view is the identity (not stored), and so is an unset
// affinity (an old save before its one-time choice): both meet the table as written. A missing file leaves the species as it is.
#include "party.h"
#include "assets.h"

#define AFF_FILE "aff.bin" // (short on purpose: the string sits in the battle plugin's RAM)
static inline uint8_t aff_wild(uint8_t species) {
    uint8_t r = species;
    if(party_state()->aff == AFF_LUNAR) read_at(AFF_FILE, 12u + species, &r, 1);
    return r;
}
