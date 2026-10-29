#include "fuzz_party.h"

#include <pkb/engine.h>
#include <pkb/team.h>
#include <string.h>

#include "pb_battle.h"
#include "pb_const.h"
#include "pb_tables.h"

#define GB_TERMINATOR 0x50 /* "@" */

bool fuzz_party_items = true;
bool fuzz_party_beat_up = false;

static uint8_t supported_moves[NUM_ATTACKS];
static unsigned num_supported;

/* items with a held effect, and the rest */
static uint8_t effect_items[255], other_items[255];
static unsigned num_effect_items, num_other_items;

static void init_moves(void) {
    if(num_supported) return;
    for(unsigned m = 1; m <= NUM_ATTACKS; m++) {
        if(m == STRUGGLE || (m == BEAT_UP && !fuzz_party_beat_up)) continue;
        if(pkb_engine_move_filter(NULL, (uint8_t)m)) supported_moves[num_supported++] = (uint8_t)m;
    }
    for(unsigned i = 1; i < 0xFF; i++) {
        if(pb_item_held_effect[i - 1][0])
            effect_items[num_effect_items++] = (uint8_t)i;
        else
            other_items[num_other_items++] = (uint8_t)i;
    }
}

/* no item a third of the time, mostly one with a held effect otherwise */
static uint8_t fuzz_item(PkbPolicyRng* rng) {
    unsigned r = pkb_policy_rng_below(rng, 12);
    if(r < 4) return 0;
    if(r < 11) return effect_items[pkb_policy_rng_below(rng, num_effect_items)];
    return other_items[pkb_policy_rng_below(rng, num_other_items)];
}

static void put16(uint8_t* p, uint16_t v) {
    p[0] = (uint8_t)(v >> 8);
    p[1] = (uint8_t)v;
}

static void fuzz_mon(PkbPolicyRng* rng, uint8_t species, uint8_t mon[PKB_PARTY_STRUCT_LEN]) {
    const PbBaseStats* base = &pb_base_stats[species - 1];
    memset(mon, 0, PKB_PARTY_STRUCT_LEN);
    mon[MON_SPECIES] = species;
    if(fuzz_party_items) mon[MON_ITEM] = fuzz_item(rng);
    unsigned nmoves = 1 + pkb_policy_rng_below(rng, 4);
    for(unsigned i = 0; i < nmoves; i++) {
        uint8_t m;
        bool dup;
        do {
            m = supported_moves[pkb_policy_rng_below(rng, num_supported)];
            dup = false;
            for(unsigned j = 0; j < i; j++)
                dup |= mon[MON_MOVES + j] == m;
        } while(dup);
        mon[MON_MOVES + i] = m;
        uint8_t pp = pb_moves[m - 1].pp;
        /* now and then a move with little or no PP left (Struggle paths) */
        if(pkb_policy_rng_below(rng, 8) == 0) pp = (uint8_t)pkb_policy_rng_below(rng, 3);
        mon[MON_PP + i] = pp;
    }
    put16(&mon[MON_OT_ID], (uint16_t)pkb_policy_rng_next(rng));
    uint16_t exp[5];
    for(int i = 0; i < 5; i++) {
        exp[i] = (uint16_t)pkb_policy_rng_next(rng);
        put16(&mon[MON_STAT_EXP + 2 * i], exp[i]);
    }
    uint16_t dvs = (uint16_t)pkb_policy_rng_next(rng);
    put16(&mon[MON_DVS], dvs);
    mon[MON_HAPPINESS] = (uint8_t)pkb_policy_rng_next(rng);
    uint8_t level = (uint8_t)(2 + pkb_policy_rng_below(rng, 99));
    mon[MON_LEVEL] = level;
    uint8_t atk = dvs >> 12, def = (dvs >> 8) & 15, spd = (dvs >> 4) & 15, spc = dvs & 15;
    uint8_t hp_dv = (uint8_t)((atk & 1) << 3 | (def & 1) << 2 | (spd & 1) << 1 | (spc & 1));
    /* stat order: hp, atk, def, spd, sat, sdf; stat exp: hp, atk, def, spd, spc */
    const uint8_t dv[6] = {hp_dv, atk, def, spd, spc, spc};
    const uint16_t sx[6] = {exp[0], exp[1], exp[2], exp[3], exp[4], exp[4]};
    uint16_t stats[6];
    for(int i = 0; i < 6; i++)
        stats[i] = pkb_calc_stat(base->stats[i], dv[i], sx[i], true, level, i == 0);
    put16(&mon[MON_MAXHP], stats[0]);
    uint16_t hp = stats[0];
    if(pkb_policy_rng_below(rng, 4) == 0) hp = (uint16_t)(1 + pkb_policy_rng_below(rng, stats[0]));
    put16(&mon[MON_HP], hp);
    /* now and then a status from before the battle (sleep, poison, burn,
     * freeze, paralysis), which link battles carry over from the party */
    switch(pkb_policy_rng_below(rng, 12)) {
    case 0:
        mon[MON_STATUS] = (uint8_t)(1 + pkb_policy_rng_below(rng, 7));
        break; /* SLP turns */
    case 1:
        mon[MON_STATUS] = 1 << PSN;
        break;
    case 2:
        mon[MON_STATUS] = 1 << BRN;
        break;
    case 3:
        mon[MON_STATUS] = 1 << FRZ;
        break;
    case 4:
        mon[MON_STATUS] = 1 << PAR;
        break;
    default:
        break;
    }
    for(int i = 1; i < 6; i++)
        put16(&mon[MON_STATS + 2 * (i - 1)], stats[i]);
}

static void put_name(uint8_t out[PKB_NAME_LEN], const uint8_t* name, size_t len) {
    memset(out, GB_TERMINATOR, PKB_NAME_LEN);
    memcpy(out, name, len < PKB_NAME_LEN - 1 ? len : PKB_NAME_LEN - 1);
}

void fuzz_party(PkbPolicyRng* rng, PkbLinkPartyData* out) {
    static const uint8_t ot[] = {0x85, 0x94, 0x99, 0x99}; /* "FUZZ" */
    init_moves();
    memset(out, 0, sizeof(*out));
    put_name(out->player_name, ot, sizeof(ot));
    put16(out->player_id, (uint16_t)pkb_policy_rng_next(rng));
    out->count = (uint8_t)(1 + pkb_policy_rng_below(rng, PKB_PARTY_LENGTH));
    for(unsigned i = 0; i < out->count; i++) {
        uint8_t species = (uint8_t)(1 + pkb_policy_rng_below(rng, NUM_POKEMON));
        out->species[i] = species;
        fuzz_mon(rng, species, out->mons[i]);
        const uint8_t* name;
        size_t len = pkb_species_name_gb(species, &name);
        put_name(out->nicknames[i], name, len);
        put_name(out->ot_names[i], ot, sizeof(ot));
    }
    out->species[out->count] = 0xFF;
}
