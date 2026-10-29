#include <pkb/random.h>

/* pokecrystal engine/battle/core.asm: _BattleRandom (link-mode path) */
uint8_t pkb_link_battle_random(uint8_t rns[PKB_LINK_RN_COUNT], uint8_t* count) {
    uint8_t i = *count;
    uint8_t a = rns[i];

    *count = (uint8_t)(i + 1);
    if(*count < PKB_LINK_RN_COUNT - 1) return a;

    *count = 0;
    for(int k = 0; k < PKB_LINK_RN_COUNT; k++) {
        rns[k] = (uint8_t)(rns[k] * 5 + 1);
    }
    return a;
}
