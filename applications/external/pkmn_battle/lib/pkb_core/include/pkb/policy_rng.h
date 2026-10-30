#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Policy PRNG (xoshiro128**) for the Flipper's own choices: team generation,
 * move selection, replacements. Completely separate from the shared battle
 * RNG (pkb/random.h), which must never be consumed for these decisions.
 */
typedef struct {
    uint32_t s[4];
} PkbPolicyRng;

/* Seed from a 64-bit value (expanded with splitmix64; never all-zero). */
void pkb_policy_rng_seed(PkbPolicyRng* rng, uint64_t seed);

uint32_t pkb_policy_rng_next(PkbPolicyRng* rng);

/* Uniform integer in [0, n), n > 0, without modulo bias. */
uint32_t pkb_policy_rng_below(PkbPolicyRng* rng, uint32_t n);

#ifdef __cplusplus
}
#endif
