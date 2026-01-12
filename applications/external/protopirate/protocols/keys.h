#pragma once

#include <stdint.h>

// Mask  - hint THEPIRAT
#define KEY_OBFUSCATION_MASK 0x5448455049524154ULL

// Obfuscated keys
#define KIA_MF_KEY_OBF        0xFCBD9AACC4F81D8FULL
#define KIA_V6_KEYSTORE_A_OBF 0x37CE21F8C9F862A8ULL
#define KIA_V6_KEYSTORE_B_OBF 0x3FC629F0C1F06AA0ULL

static inline uint64_t get_kia_mf_key(void) {
    volatile uint64_t mask = KEY_OBFUSCATION_MASK;
    return KIA_MF_KEY_OBF ^ mask;
}

static inline uint64_t get_kia_v6_keystore_a(void) {
    volatile uint64_t mask = KEY_OBFUSCATION_MASK;
    return KIA_V6_KEYSTORE_A_OBF ^ mask;
}

static inline uint64_t get_kia_v6_keystore_b(void) {
    volatile uint64_t mask = KEY_OBFUSCATION_MASK;
    return KIA_V6_KEYSTORE_B_OBF ^ mask;
}
