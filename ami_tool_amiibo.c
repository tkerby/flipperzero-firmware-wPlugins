#include "ami_tool_i.h"

#include <stdbool.h>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
#include <string.h>

#define AMIIBO_PAGE_COUNT (135U)
#define AMIIBO_TOTAL_BYTES (AMIIBO_PAGE_COUNT * MF_ULTRALIGHT_PAGE_SIZE)
#define AMIIBO_OFFSET_CAPABILITY (0x0CU)
#define AMIIBO_OFFSET_FIXED_A5 (0x10U)
#define AMIIBO_OFFSET_WRITE_COUNTER (0x11U)
#define AMIIBO_WRITE_COUNTER_SIZE (2U)
#define AMIIBO_OFFSET_TAG_CONFIG (0x14U)
#define AMIIBO_TAG_CONFIG_SIZE (32U)
#define AMIIBO_OFFSET_TAG_HASH (0x34U)
#define AMIIBO_HASH_SIZE (32U)
#define AMIIBO_OFFSET_MODEL_INFO (0x54U)
#define AMIIBO_MODEL_INFO_SIZE (12U)
#define AMIIBO_OFFSET_KEYGEN_SALT (0x60U)
#define AMIIBO_KEYGEN_SALT_SIZE (32U)
#define AMIIBO_OFFSET_DATA_HASH (0x80U)
#define AMIIBO_OFFSET_APP_DATA (0xA0U)
#define AMIIBO_APP_DATA_SIZE (360U)
#define AMIIBO_OFFSET_DYNAMIC_LOCK (0x208U)
#define AMIIBO_OFFSET_RESERVED (0x20BU)
#define AMIIBO_OFFSET_CONFIG (0x20CU)
#define AMIIBO_CONFIG_SIZE (16U)
#define AMIIBO_SIGNING_BUFFER_SIZE (480U)
#define AMIIBO_MAX_SEED_SIZE (480U)

static inline uint8_t* amiibo_bytes(MfUltralightData* tag_data) {
    return &tag_data->page[0].data[0];
}

static inline const uint8_t* amiibo_bytes_const(const MfUltralightData* tag_data) {
    return &tag_data->page[0].data[0];
}

static bool amiibo_has_full_dump(const MfUltralightData* tag_data) {
    return (
        tag_data && (tag_data->type == MfUltralightTypeNTAG215) &&
        (tag_data->pages_total >= AMIIBO_PAGE_COUNT));
}

static void amiibo_derive_step(
    bool* used,
    uint16_t* iteration,
    uint8_t* buffer,
    size_t buffer_size,
    mbedtls_md_context_t* hmac_context,
    uint8_t* output) {
    if(*used) {
        mbedtls_md_hmac_reset(hmac_context);
    } else {
        *used = true;
    }

    buffer[0] = (uint8_t)(*iteration >> 8);
    buffer[1] = (uint8_t)(*iteration);
    (*iteration)++;

    mbedtls_md_hmac_update(hmac_context, buffer, buffer_size);
    mbedtls_md_hmac_finish(hmac_context, output);
}

static RfidxStatus amiibo_randomize_uid(uint8_t* raw) {
    if(!raw) return RFIDX_ARGUMENT_ERROR;

    raw[0] = 0x04;
    uint8_t buffer[6];
    furi_hal_random_fill_buf(buffer, sizeof(buffer));

    raw[1] = buffer[0];
    raw[2] = buffer[1];

    for(size_t i = 0; i < 4; i++) {
        raw[4 + i] = buffer[i + 2];
    }

    raw[3] = raw[0] ^ raw[1] ^ raw[2] ^ 0x88;
    raw[8] = raw[4] ^ raw[5] ^ raw[6] ^ raw[7];

    return RFIDX_OK;
}

static void amiibo_calculate_password(const uint8_t* manufacturer, uint8_t* password_out) {
    password_out[0] = manufacturer[1] ^ manufacturer[4] ^ 0xAA;
    password_out[1] = manufacturer[2] ^ manufacturer[5] ^ 0x55;
    password_out[2] = manufacturer[4] ^ manufacturer[6] ^ 0xAA;
    password_out[3] = manufacturer[5] ^ manufacturer[7] ^ 0x55;
}

RfidxStatus amiibo_derive_key(
    const DumpedKeySingle* input_key,
    const MfUltralightData* tag_data,
    DerivedKey* derived_key) {
    if(!input_key || !tag_data || !derived_key) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if(!amiibo_has_full_dump(tag_data)) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if((input_key->magicBytesSize != 14U) && (input_key->magicBytesSize != 16U)) {
        return RFIDX_ARGUMENT_ERROR;
    }

    const uint8_t* raw = amiibo_bytes_const(tag_data);

    uint8_t prepared_seed[AMIIBO_MAX_SEED_SIZE] = {0};
    size_t type_len = strnlen(input_key->typeString, sizeof(input_key->typeString));
    if(type_len >= sizeof(input_key->typeString)) {
        type_len = sizeof(input_key->typeString) - 1;
    }
    memcpy(prepared_seed, input_key->typeString, type_len);
    prepared_seed[type_len] = '\0';
    uint8_t* curr = prepared_seed + type_len + 1;

    const size_t leading_seed_bytes = 16U - input_key->magicBytesSize;
    memcpy(curr, raw + AMIIBO_OFFSET_WRITE_COUNTER, leading_seed_bytes);
    curr += leading_seed_bytes;
    memcpy(curr, input_key->magicBytes, input_key->magicBytesSize);
    curr += input_key->magicBytesSize;

    memcpy(curr, raw, 8);
    memcpy(curr + 8, raw, 8);
    curr += 16;

    for(size_t i = 0; i < AMIIBO_KEYGEN_SALT_SIZE; i++) {
        curr[i] = raw[AMIIBO_OFFSET_KEYGEN_SALT + i] ^ input_key->xorTable[i];
    }
    curr += AMIIBO_KEYGEN_SALT_SIZE;

    const size_t prepared_seed_size = curr - prepared_seed;
    uint8_t buffer[sizeof(uint16_t) + AMIIBO_MAX_SEED_SIZE] = {0};
    memcpy(buffer + sizeof(uint16_t), prepared_seed, prepared_seed_size);
    const size_t buffer_size = sizeof(uint16_t) + prepared_seed_size;

    mbedtls_md_context_t hmac_context;
    mbedtls_md_init(&hmac_context);
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_setup(&hmac_context, md_info, 1);
    mbedtls_md_hmac_starts(&hmac_context, input_key->hmacKey, sizeof(input_key->hmacKey));

    size_t remaining = sizeof(DerivedKey);
    uint8_t* out = (uint8_t*)derived_key;
    bool used = false;
    uint16_t iteration = 0;

    while(remaining > 0) {
        if(remaining < 32U) {
            uint8_t temp[32];
            amiibo_derive_step(&used, &iteration, buffer, buffer_size, &hmac_context, temp);
            memcpy(out, temp, remaining);
            remaining = 0;
        } else {
            amiibo_derive_step(&used, &iteration, buffer, buffer_size, &hmac_context, out);
            out += 32;
            remaining -= 32;
        }
    }

    mbedtls_md_free(&hmac_context);

    return RFIDX_OK;
}

RfidxStatus amiibo_cipher(const DerivedKey* data_key, MfUltralightData* tag_data) {
    if(!data_key || !tag_data) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if(!amiibo_has_full_dump(tag_data)) {
        return RFIDX_ARGUMENT_ERROR;
    }

    uint8_t* payload = amiibo_bytes(tag_data) + AMIIBO_OFFSET_TAG_CONFIG;
    const size_t payload_size = AMIIBO_TAG_CONFIG_SIZE + AMIIBO_APP_DATA_SIZE;

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, data_key->aesKey, 128);

    uint8_t nonce_counter[16] = {0};
    uint8_t stream_block[16] = {0};
    memcpy(nonce_counter, data_key->aesIV, sizeof(nonce_counter));
    size_t nc_offset = 0;

    mbedtls_aes_crypt_ctr(
        &aes, payload_size, &nc_offset, nonce_counter, stream_block, payload, payload);

    mbedtls_aes_free(&aes);

    return RFIDX_OK;
}

RfidxStatus amiibo_generate_signature(
    const DerivedKey* tag_key,
    const DerivedKey* data_key,
    const MfUltralightData* tag_data,
    uint8_t* tag_hash,
    uint8_t* data_hash) {
    if(!tag_key || !data_key || !tag_data || !tag_hash || !data_hash) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if(!amiibo_has_full_dump(tag_data)) {
        return RFIDX_ARGUMENT_ERROR;
    }

    uint8_t signing_buffer[AMIIBO_SIGNING_BUFFER_SIZE] = {0};
    const uint8_t* raw = amiibo_bytes_const(tag_data);

    memcpy(signing_buffer, raw + AMIIBO_OFFSET_FIXED_A5, 36);
    memcpy(signing_buffer + 36, raw + AMIIBO_OFFSET_APP_DATA, AMIIBO_APP_DATA_SIZE);
    memcpy(signing_buffer + 428, raw, 8);
    memcpy(signing_buffer + 436, raw + AMIIBO_OFFSET_MODEL_INFO, AMIIBO_MODEL_INFO_SIZE);
    memcpy(signing_buffer + 448, raw + AMIIBO_OFFSET_KEYGEN_SALT, AMIIBO_KEYGEN_SALT_SIZE);

    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    mbedtls_md_hmac(
        md_info, tag_key->hmacKey, sizeof(tag_key->hmacKey), signing_buffer + 428, 52, tag_hash);

    memcpy(signing_buffer + 396, tag_hash, AMIIBO_HASH_SIZE);

    mbedtls_md_hmac(
        md_info, data_key->hmacKey, sizeof(data_key->hmacKey), signing_buffer + 1, 479, data_hash);

    return RFIDX_OK;
}

RfidxStatus amiibo_validate_signature(
    const DerivedKey* tag_key,
    const DerivedKey* data_key,
    const MfUltralightData* tag_data) {
    if(!tag_key || !data_key || !tag_data) {
        return RFIDX_ARGUMENT_ERROR;
    }
    uint8_t tag_hash[AMIIBO_HASH_SIZE];
    uint8_t data_hash[AMIIBO_HASH_SIZE];

    RfidxStatus status =
        amiibo_generate_signature(tag_key, data_key, tag_data, tag_hash, data_hash);
    if(status != RFIDX_OK) {
        return status;
    }

    const uint8_t* raw = amiibo_bytes_const(tag_data);
    if(memcmp(tag_hash, raw + AMIIBO_OFFSET_TAG_HASH, AMIIBO_HASH_SIZE) != 0) {
        return RFIDX_AMIIBO_HMAC_VALIDATION_ERROR;
    }
    if(memcmp(data_hash, raw + AMIIBO_OFFSET_DATA_HASH, AMIIBO_HASH_SIZE) != 0) {
        return RFIDX_AMIIBO_HMAC_VALIDATION_ERROR;
    }

    return RFIDX_OK;
}

RfidxStatus amiibo_sign_payload(
    const DerivedKey* tag_key,
    const DerivedKey* data_key,
    MfUltralightData* tag_data) {
    if(!tag_key || !data_key || !tag_data) {
        return RFIDX_ARGUMENT_ERROR;
    }
    uint8_t tag_hash[AMIIBO_HASH_SIZE];
    uint8_t data_hash[AMIIBO_HASH_SIZE];

    RfidxStatus status =
        amiibo_generate_signature(tag_key, data_key, tag_data, tag_hash, data_hash);
    if(status != RFIDX_OK) {
        return status;
    }

    uint8_t* raw = amiibo_bytes(tag_data);
    memcpy(raw + AMIIBO_OFFSET_TAG_HASH, tag_hash, AMIIBO_HASH_SIZE);
    memcpy(raw + AMIIBO_OFFSET_DATA_HASH, data_hash, AMIIBO_HASH_SIZE);

    return RFIDX_OK;
}

RfidxStatus amiibo_format_dump(MfUltralightData* tag_data, Ntag21xMetadataHeader* header) {
    if(!tag_data) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if(!amiibo_has_full_dump(tag_data)) {
        return RFIDX_ARGUMENT_ERROR;
    }

    uint8_t* raw = amiibo_bytes(tag_data);

    raw[9] = 0x48;
    raw[10] = 0x0F;
    raw[11] = 0xE0;

    static const uint8_t capability_defaults[4] = {0xF1, 0x10, 0xFF, 0xEE};
    memcpy(raw + AMIIBO_OFFSET_CAPABILITY, capability_defaults, sizeof(capability_defaults));

    raw[AMIIBO_OFFSET_FIXED_A5] = 0xA5;
    raw[AMIIBO_OFFSET_DYNAMIC_LOCK + 0] = 0x01;
    raw[AMIIBO_OFFSET_DYNAMIC_LOCK + 1] = 0x00;
    raw[AMIIBO_OFFSET_DYNAMIC_LOCK + 2] = 0x0F;
    raw[AMIIBO_OFFSET_RESERVED] = 0xBD;

    static const uint8_t cfg0[4] = {0x00, 0x00, 0x00, 0x04};
    static const uint8_t cfg1[4] = {0x5F, 0x00, 0x00, 0x00};
    memcpy(raw + AMIIBO_OFFSET_CONFIG, cfg0, sizeof(cfg0));
    memcpy(raw + AMIIBO_OFFSET_CONFIG + 4, cfg1, sizeof(cfg1));

    uint8_t password[4];
    amiibo_calculate_password(raw, password);
    memcpy(raw + AMIIBO_OFFSET_CONFIG + 8, password, sizeof(password));
    raw[AMIIBO_OFFSET_CONFIG + 12] = 0x80;
    raw[AMIIBO_OFFSET_CONFIG + 13] = 0x80;
    raw[AMIIBO_OFFSET_CONFIG + 14] = 0x00;
    raw[AMIIBO_OFFSET_CONFIG + 15] = 0x00;

    if(header) {
        memset(header, 0, sizeof(*header));
        static const uint8_t version[8] = {0x00, 0x04, 0x04, 0x02, 0x01, 0x00, 0x11, 0x03};
        memcpy(header->version, version, sizeof(version));
        header->memory_max = 134;
    }

    return RFIDX_OK;
}

RfidxStatus amiibo_generate(
    const uint8_t* uuid,
    MfUltralightData* tag_data,
    Ntag21xMetadataHeader* header) {
    if(!uuid || !tag_data || !header) {
        return RFIDX_ARGUMENT_ERROR;
    }

    mf_ultralight_reset(tag_data);
    tag_data->type = MfUltralightTypeNTAG215;
    tag_data->pages_total = AMIIBO_PAGE_COUNT;
    tag_data->pages_read = AMIIBO_PAGE_COUNT;

    uint8_t* raw = amiibo_bytes(tag_data);
    memset(raw, 0, AMIIBO_TOTAL_BYTES);

    furi_hal_random_fill_buf(raw + AMIIBO_OFFSET_KEYGEN_SALT, AMIIBO_KEYGEN_SALT_SIZE);
    memset(raw + AMIIBO_OFFSET_MODEL_INFO, 0, AMIIBO_MODEL_INFO_SIZE);
    memcpy(raw + AMIIBO_OFFSET_MODEL_INFO, uuid, 8);

    RfidxStatus status = amiibo_randomize_uid(raw);
    if(status != RFIDX_OK) {
        return status;
    }

    return amiibo_format_dump(tag_data, header);
}
