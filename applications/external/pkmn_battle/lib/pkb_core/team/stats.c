#include <pkb/team.h>
#include "pb_tables.h"

/* engine/math/get_square_root.asm: smallest b with b*b >= x, capped at 255.
 * Note x = 0 yields 1, as in the asm. */
static uint8_t get_square_root(uint16_t x) {
    uint8_t b = 0;
    for(;;) {
        b++;
        if(b == 255) return b;
        if((uint32_t)b * b >= x) return b;
    }
}

/* engine/pokemon/move_mon.asm: CalcMonStatC */
uint16_t pkb_calc_stat(uint8_t base, uint8_t dv, uint16_t stat_exp, bool use_stat_exp,
                       uint8_t level, bool is_hp) {
    uint8_t sqrt_b = use_stat_exp ? get_square_root(stat_exp) : 0;
    uint16_t de = (uint16_t)(base + dv);
    de = (uint16_t)(de << 1);
    de = (uint16_t)(de + (sqrt_b >> 2));
    /* Multiply 24-bit by level, then Divide by 100 */
    uint32_t q = ((uint32_t)de * level) / 100;
    q += is_hp ? (uint32_t)level + 10 : 5;
    if(q > 999) q = 999;
    return (uint16_t)q;
}

/* engine/pokemon/experience.asm: CalcExpAtLevel. The asm keeps 24 bits;
 * levels >= 2 never underflow. */
uint32_t pkb_exp_at_level(uint8_t growth_rate, uint8_t level) {
    const PbGrowthRate* g = &pb_growth_rates[growth_rate];
    uint32_t n = level;
    int32_t c = g->c;
    uint32_t cubic = n * n * n * g->a / g->b;
    int32_t v = (int32_t)cubic + c * (int32_t)(n * n) + (int32_t)(g->d * n) - g->e;
    return (uint32_t)v & 0xFFFFFF;
}

uint8_t pkb_hp_dv(const uint8_t dvs[2]) {
    return (uint8_t)(((dvs[0] >> 4) & 1) << 3 | (dvs[0] & 1) << 2 | ((dvs[1] >> 4) & 1) << 1 |
                     (dvs[1] & 1));
}

void pkb_team_mon_init(
    PkbTeamMon* mon,
    uint8_t species,
    uint8_t level,
    const uint8_t moves[PKB_NUM_MOVES]) {
    mon->species = species;
    mon->level = level;
    mon->item = 0;
    for(int i = 0; i < PKB_NUM_MOVES; i++) mon->moves[i] = moves[i];
    mon->dvs[0] = mon->dvs[1] = (PKB_TEAM_DV << 4) | PKB_TEAM_DV;
    mon->stat_exp = PKB_TEAM_STAT_EXP;
    mon->happiness = PKB_TEAM_HAPPINESS;
    pkb_team_mon_calc(mon);
}

void pkb_team_mon_calc(PkbTeamMon* mon) {
    const PbBaseStats* b = &pb_base_stats[mon->species - 1];
    /* DV per stat in party order hp, atk, def, spd, sat, sdf (spc for both) */
    const uint8_t dv[6] = {
        pkb_hp_dv(mon->dvs),
        mon->dvs[0] >> 4,
        mon->dvs[0] & 15,
        mon->dvs[1] >> 4,
        mon->dvs[1] & 15,
        mon->dvs[1] & 15};

    for(int i = 0; i < PKB_NUM_MOVES; i++) {
        mon->pp[i] = mon->moves[i] ? pb_moves[mon->moves[i] - 1].pp : 0;
    }
    mon->exp = pkb_exp_at_level(b->growth_rate, mon->level);
    mon->max_hp = pkb_calc_stat(b->stats[0], dv[0], mon->stat_exp, true, mon->level, true);
    for(int s = 0; s < PKB_NUM_BATTLE_STATS; s++) {
        mon->stats[s] =
            pkb_calc_stat(b->stats[s + 1], dv[s + 1], mon->stat_exp, true, mon->level, false);
    }
}
