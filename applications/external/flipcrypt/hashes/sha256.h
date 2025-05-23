#pragma once
#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint32_t state[8];
    uint64_t bitlen;
    uint8_t data[64];
    uint32_t datalen;
} Sha256Context;

void sha256_init(Sha256Context* ctx);
void sha256_update(Sha256Context* ctx, const uint8_t* data, size_t len);
void sha256_finalize(Sha256Context* ctx, uint8_t hash[32]);
void sha256_to_hex(const uint8_t hash[32], char* output);