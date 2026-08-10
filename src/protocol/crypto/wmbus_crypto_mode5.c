#include "wmbus_crypto_mode5.h"

#include <mbedtls/aes.h>
#include <string.h>

#define WMBUS_AES_BLOCK_LEN   16U

static bool wmbus_crypto_mode5_payload_has_check_bytes(const uint8_t* payload, size_t payload_len) {
    return payload_len >= 2U && payload[0] == 0x2F && payload[1] == 0x2F;
}

static void wmbus_crypto_mode5_build_iv(
    const uint8_t iv_address[8], uint8_t access_number, uint8_t iv[WMBUS_AES_BLOCK_LEN]) {
    memcpy(iv, iv_address, 8U);
    memset(&iv[8], access_number, 8U);
}

static bool wmbus_crypto_mode5_encrypted_length(
    size_t payload_len, uint16_t cfg, size_t* encrypted_len) {
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

WmBusMode5DecryptInfo wmbus_crypto_mode5_decrypt(
    const uint8_t* encrypted_payload,
    size_t payload_len,
    const uint8_t iv_address[8],
    uint8_t access_number,
    uint16_t cfg,
    const uint8_t key[WMBUS_AES_BLOCK_LEN],
    uint8_t* out_payload) {
    WmBusMode5DecryptInfo info = {
        .result = WmBusDecryptResultInvalidArgs,
        .has_check_bytes = false,
    };

    if(!encrypted_payload || !iv_address || !key || !out_payload) {
        return info;
    }
    size_t encrypted_len = 0U;
    if(!wmbus_crypto_mode5_encrypted_length(payload_len, cfg, &encrypted_len)) {
        return (WmBusMode5DecryptInfo){.result = WmBusDecryptResultInvalidEncryptedLength};
    }
    memcpy(out_payload, encrypted_payload, payload_len);
    if(encrypted_len == 0U) {
        info.result = WmBusDecryptResultOk;
        info.has_check_bytes = wmbus_crypto_mode5_payload_has_check_bytes(out_payload, payload_len);
        return info;
    }

    uint8_t iv[WMBUS_AES_BLOCK_LEN] = {0};
    wmbus_crypto_mode5_build_iv(iv_address, access_number, iv);

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    int result = mbedtls_aes_setkey_dec(&aes, key, WMBUS_AES_BLOCK_LEN * 8U);
    if(result == 0) {
        result = mbedtls_aes_crypt_cbc(
            &aes,
            MBEDTLS_AES_DECRYPT,
            encrypted_len,
            iv,
            encrypted_payload,
            out_payload);
    }
    mbedtls_aes_free(&aes);
    if(result != 0) {
        return (WmBusMode5DecryptInfo){.result = WmBusDecryptResultCryptoError};
    }

    info.result = WmBusDecryptResultOk;
    info.has_check_bytes = wmbus_crypto_mode5_payload_has_check_bytes(out_payload, payload_len);
    return info;
}
