#include <pkb/moveinfo.h>
#include "pb_tables.h"
#include "pb_battle.h"

uint16_t pkb_type_mult(uint8_t atk_type, uint8_t d1, uint8_t d2, bool foresight) {
    uint16_t mult = PKB_MULT_NEUTRAL;
    for(const uint8_t* p = pb_type_matchups; p[0] != 0xFF; p += 3) {
        if(p[0] == 0xFE) { /* -2: the Foresight-sensitive entries follow */
            if(foresight) break;
            p -= 2;
            continue;
        }
        if(p[0] == atk_type && (p[1] == d1 || p[1] == d2)) mult = (uint16_t)(mult * p[2] / 10);
    }
    return mult;
}

void pkb_hidden_power(const uint8_t dvs[2], uint8_t* type, uint8_t* power) {
    uint8_t atkdef = dvs[0], spdspc = dvs[1];
    uint8_t a = (uint8_t)(((atkdef >> 4) & 8) | ((atkdef & 8) >> 1) | ((spdspc >> 6) & 2) | ((spdspc & 8) >> 3));
    a = (uint8_t)(a * 5 + (spdspc & 3));
    *power = (uint8_t)((a >> 1) + 30 + 1);
    uint8_t t = (uint8_t)(((atkdef >> 4 & 3) << 2 | (atkdef & 3)) + 1); /* skip Normal */
    if(t >= BIRD) {
        t++;
        if(t >= UNUSED_TYPES) t += UNUSED_TYPES_END - UNUSED_TYPES;
    }
    *type = t;
}

uint8_t pkb_type_boost_item(uint8_t type) {
    for(const uint8_t* p = pb_type_boost_items; p[0] != 0xFF; p += 2) {
        if(p[1] != type) continue;
        for(int item = 1; item <= 255; item++)
            if(pb_item_held_effect[item - 1][0] == p[0]) return (uint8_t)item;
    }
    return 0;
}

bool pkb_move_high_crit(uint8_t move) {
    for(const uint8_t* p = pb_critical_hit_moves; *p != 0xFF; p++)
        if(*p == move) return true;
    return false;
}
