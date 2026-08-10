#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <storage/storage.h>

#include "../protocol/crypto/wmbus_crypto_key_store.h"
#include "wmbus_paths.h"

#define WMBUS_KEY_BYTES WMBUS_CRYPTO_KEY_BYTES

typedef struct {
    bool file_present;
    bool loaded;
    char status[24];
    WmBusCryptoKeyStore key_store;
} WmBusKeyring;

bool wmbus_keyring_load(Storage* storage, WmBusKeyring* keyring);
bool wmbus_keyring_append(
    Storage* storage,
    WmBusKeyring* keyring,
    const uint8_t key[WMBUS_KEY_BYTES]);
