/* Side-effect-free move and type helpers, shared by the set builder and the
 * AI. They read only the const generated tables, never an engine. */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Neutral damage multiplier on the PKB_MULT_NEUTRAL scale (x2 = 200, x0.5 =
 * 50, immune = 0) of an attack type against a defender with types d1/d2,
 * from TypeMatchups. foresight skips the entries after the $FE marker (the
 * Ghost immunities). A defender with d1 == d2 counts once, as in the game. */
#define PKB_MULT_NEUTRAL 100
uint16_t pkb_type_mult(uint8_t atk_type, uint8_t d1, uint8_t d2, bool foresight);

/* engine/battle/hidden_power.asm with the party struct DV bytes */
void pkb_hidden_power(const uint8_t dvs[2], uint8_t* type, uint8_t* power);

/* The held item that boosts `type` (PINK_BOW for Normal...), 0 if none. */
uint8_t pkb_type_boost_item(uint8_t type);

/* Moves on the CriticalHitMoves list (Slash, Karate Chop...). */
bool pkb_move_high_crit(uint8_t move);

/* Physical vs special: in Gen 2 the move's type decides. */
static inline bool pkb_type_special(uint8_t type) {
    return type >= 0x14; /* SPECIAL */
}

#ifdef __cplusplus
}
#endif
