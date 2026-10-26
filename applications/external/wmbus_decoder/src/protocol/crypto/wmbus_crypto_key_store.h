#pragma once

#include <stddef.h>
#include <stdint.h>

#define WMBUS_CRYPTO_KEY_BYTES     16U
#define WMBUS_CRYPTO_KEY_STORE_MAX 16U

typedef struct {
    uint8_t count;
    uint8_t keys[WMBUS_CRYPTO_KEY_STORE_MAX][WMBUS_CRYPTO_KEY_BYTES];
} WmBusCryptoKeyStore;
