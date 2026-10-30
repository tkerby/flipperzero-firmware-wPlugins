#pragma once

#include <stdint.h>
#include "pb_const.h"

/* Index with species - 1. */
typedef struct {
    uint8_t stats[6]; /* hp, atk, def, spd, sat, sdf */
    uint8_t types[2];
    uint8_t gender; /* GENDER_*; 0xFF = GENDER_UNKNOWN */
    uint8_t growth_rate; /* GROWTH_* */
} PbBaseStats;
extern const PbBaseStats pb_base_stats[NUM_POKEMON];

/* Index with move - 1. The animation byte of the asm move struct always
 * equals the move id, so it is not stored. */
typedef struct {
    uint8_t effect;
    uint8_t power;
    uint8_t type;
    uint8_t accuracy;
    uint8_t pp;
    uint8_t effect_chance;
} PbMove;
extern const PbMove pb_moves[NUM_ATTACKS];

/* Packed names in the Game Boy charset, no terminators: name i (0-based)
 * spans [off[i], off[i + 1]). */
extern const uint8_t pb_species_names[];
extern const uint16_t pb_species_name_off[NUM_POKEMON + 1];
extern const uint8_t pb_move_names[];
extern const uint16_t pb_move_name_off[NUM_ATTACKS + 1];

/* ASCII -> Game Boy charset (0 = unmapped) and back ('?' = unmapped,
 * 0 for the '@' terminator). */
extern const uint8_t pb_ascii_to_gb[128];
extern const char pb_gb_to_ascii[256];

/* Item names (index item - 1), only for items with a held effect; the
 * others are empty. */
#define PB_ITEM_NAMES_LEN 255
extern const uint8_t pb_item_names[];
extern const uint16_t pb_item_name_off[PB_ITEM_NAMES_LEN + 1];

/* data/growth_rates.asm: exp(n) = a/b*n^3 + c*n^2 + d*n - e */
typedef struct {
    uint8_t a, b;
    int8_t c;
    uint8_t d, e;
} PbGrowthRate;
extern const PbGrowthRate pb_growth_rates[NUM_GROWTH_RATES];
