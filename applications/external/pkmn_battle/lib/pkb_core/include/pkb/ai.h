/*
 * The Flipper's battle AI. It plugs into PkbEngineIo.choose_action and
 * choose_replacement and only reads the engine (pkb_engine_peek): it never
 * touches the shared battle random numbers or the WRAM shadow, so the
 * battle stays in lockstep whatever it decides. Its own randomness (picking
 * between near-equal options) comes from a separate policy RNG.
 *
 * It knows the Game Boy's whole party, as the link hands it over (the
 * in-game trainer AI also knows the player's moves). Each turn it estimates
 * the damage of every move both ways with the game's formula, then:
 *  - goes for KOs, preferring priority moves when the foe would KO first;
 *  - uses status, boosting, recovery and utility moves when they help (no
 *    sleep on a statused target, no boosting into a KO, Sleep Talk only
 *    while asleep, Dream Eater only on a sleeping target...);
 *  - switches out of clearly lost matchups to a teammate that takes the
 *    foe's best hit and threatens back, at most every other turn;
 *  - picks the best replacement after a faint or for Baton Pass.
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <pkb/engine.h>
#include <pkb/policy_rng.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    PkbPolicyRng rng;
    uint32_t turn; /* battle menu choices made */
    uint32_t last_switch; /* turn of the last voluntary switch, 0 = none */
} PkbAi;

void pkb_ai_init(PkbAi* ai, uint64_t seed);

/* PkbEngineIo hooks; ctx is the PkbAi. */
void pkb_ai_choose_action(void* ctx, const PkbEngine* E, PkbChoice* choice);
uint8_t pkb_ai_choose_replacement(void* ctx, const PkbEngine* E);

/* The damage estimate of one move between the two active mons (player =
 * true: ours on the Game Boy's), without a critical hit: the game's
 * min and max roll for a hit that lands, and the chance it lands (x256).
 * Fixed-damage moves give their damage; moves without damage give 0. */
typedef struct {
    uint16_t min, max;
    uint16_t hit; /* 0-256 */
    uint8_t hits; /* expected number of hits (multi-hit moves) */
} PkbAiDamage;
void pkb_ai_active_damage(const PkbEngine* E, bool player, uint8_t move, PkbAiDamage* out);

#ifdef __cplusplus
}
#endif
