#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../core/wmbus_types.h"

WmBusDecryptResult wmbus_crypto_mode5_decrypt(
    const uint8_t* encrypted_payload,
    size_t payload_len,
    const uint8_t iv_address[8],
    uint8_t access_number,
    uint16_t cfg,
    const uint8_t key[16],
    uint8_t* out_payload);
