#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Number of shared link-battle random numbers (wLinkBattleRNs). */
#define PKB_LINK_RN_COUNT 10

/*
 * Port of _BattleRandom (engine/battle/core.asm) for link battles.
 *
 * rns:   the 10-byte wLinkBattleRNs table, updated in place on wrap-around
 * count: wLinkBattleRNCount
 *
 * Returns RN[count++] while count stays below 9. The 9th pull returns RN[8],
 * resets count to 0 and advances every seed with a = a*5 + 1 (mod 256);
 * RN[9] is never returned directly.
 */
uint8_t pkb_link_battle_random(uint8_t rns[PKB_LINK_RN_COUNT], uint8_t* count);

#ifdef __cplusplus
}
#endif
