#include "wmbus_crypto_mode5.h"

#include <mbedtls/aes.h>
#include <string.h>

#define WMBUS_AES_BLOCK_LEN 16U

static void wmbus_crypto_mode5_build_iv(
    const uint8_t iv_address[8],
    uint8_t access_number,
    uint8_t iv[WMBUS_AES_BLOCK_LEN]) {
    memcpy(iv, iv_address, 8U);
    memset(&iv[8], access_number, 8U);
}

static bool
    wmbus_crypto_mode5_encrypted_length(size_t payload_len, uint16_t cfg, size_t* encrypted_len) {
    if(!encrypted_len) {
        return false;
    }

    uint8_t encrypted_blocks = (uint8_t)((cfg >> 4) & 0x0FU);
    if(encrypted_blocks == 0U) {
        *encrypted_len = 0U;
        return true;
    }
    if(encrypted_blocks == 0x0FU) {
        if(payload_len % WMBUS_AES_BLOCK_LEN != 0U) {
            return false;
        }
        *encrypted_len = payload_len;
        return true;
    }

    *encrypted_len = (size_t)encrypted_blocks * WMBUS_AES_BLOCK_LEN;
    return *encrypted_len <= payload_len;
}

WmBusDecryptResult wmbus_crypto_mode5_decrypt(
    const uint8_t* encrypted_payload,
    size_t payload_len,
    const uint8_t iv_address[8],
    uint8_t access_number,
    uint16_t cfg,
    const uint8_t key[WMBUS_AES_BLOCK_LEN],
    uint8_t* out_payload) {
    if(!encrypted_payload || !iv_address || !key || !out_payload) {
        return WmBusDecryptResultInvalidArgs;
    }
    size_t encrypted_len = 0U;
    if(!wmbus_crypto_mode5_encrypted_length(payload_len, cfg, &encrypted_len)) {
        return WmBusDecryptResultInvalidEncryptedLength;
    }
    memcpy(out_payload, encrypted_payload, payload_len);
    if(encrypted_len == 0U) {
        return WmBusDecryptResultOk;
    }

    uint8_t iv[WMBUS_AES_BLOCK_LEN] = {0};
    wmbus_crypto_mode5_build_iv(iv_address, access_number, iv);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int result = mbedtls_aes_setkey_dec(&aes, key, WMBUS_AES_BLOCK_LEN * 8U);
    if(result == 0) {
        result = mbedtls_aes_crypt_cbc(
            &aes, MBEDTLS_AES_DECRYPT, encrypted_len, iv, encrypted_payload, out_payload);
    }
    mbedtls_aes_free(&aes);
    if(result != 0) {
        return WmBusDecryptResultCryptoError;
    }

    return WmBusDecryptResultOk;
}
