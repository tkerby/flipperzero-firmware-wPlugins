#include <pkb/policy_rng.h>

static uint64_t splitmix64(uint64_t* x) {
    uint64_t z = (*x += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

void pkb_policy_rng_seed(PkbPolicyRng* rng, uint64_t seed) {
    uint64_t a = splitmix64(&seed);
    uint64_t b = splitmix64(&seed);
    rng->s[0] = (uint32_t)a;
    rng->s[1] = (uint32_t)(a >> 32);
    rng->s[2] = (uint32_t)b;
    rng->s[3] = (uint32_t)(b >> 32);
    if(!(rng->s[0] | rng->s[1] | rng->s[2] | rng->s[3])) rng->s[0] = 1;
}

static inline uint32_t rotl(uint32_t x, int k) {
    return (x << k) | (x >> (32 - k));
}

uint32_t pkb_policy_rng_next(PkbPolicyRng* rng) {
    uint32_t* s = rng->s;
    uint32_t result = rotl(s[1] * 5, 7) * 9;
    uint32_t t = s[1] << 9;
    s[2] ^= s[0];
    s[3] ^= s[1];
    s[1] ^= s[2];
    s[0] ^= s[3];
    s[2] ^= t;
    s[3] = rotl(s[3], 11);
    return result;
}

uint32_t pkb_policy_rng_below(PkbPolicyRng* rng, uint32_t n) {
    /* Lemire's nearly-divisionless method with rejection */
    uint64_t m = (uint64_t)pkb_policy_rng_next(rng) * n;
    uint32_t low = (uint32_t)m;
    if(low < n) {
        uint32_t threshold = (uint32_t)(-n) % n;
        while(low < threshold) {
            m = (uint64_t)pkb_policy_rng_next(rng) * n;
            low = (uint32_t)m;
        }
    }
    return (uint32_t)(m >> 32);
}
