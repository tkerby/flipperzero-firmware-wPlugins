#pragma once

#include <stdint.h>

// Mask  - hint THEPIRAT
#define KEY_OBFUSCATION_MASK 0x5448455049524154ULL

// Obfuscated keys
#define KIA_MF_KEY_OBF 0xFCBD9AACC4F81D8FULL

static inline uint64_t get_kia_mf_key(void)
{
    volatile uint64_t mask = KEY_OBFUSCATION_MASK;
    return KIA_MF_KEY_OBF ^ mask;
}