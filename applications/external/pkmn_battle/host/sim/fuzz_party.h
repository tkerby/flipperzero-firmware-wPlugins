#pragma once

/* Random parties for fuzzing the GB side: any species, level 2-100, random
 * DVs and stat exp, 1-4 moves whose effects the engine supports, random held items.
 * Stats are computed as CalcMonStats does. */

#include <stdbool.h>

#include <pkb/party_block.h>
#include <pkb/policy_rng.h>

/* false: no held items and no random draws for them, so seeds give the
 * parties they gave before held items were added (the regression corpus) */
extern bool fuzz_party_items;
/* true: Beat Up joins the move pool (off by default so the seeds
 * keep giving the parties they gave before) */
extern bool fuzz_party_beat_up;

void fuzz_party(PkbPolicyRng* rng, PkbLinkPartyData* out);
