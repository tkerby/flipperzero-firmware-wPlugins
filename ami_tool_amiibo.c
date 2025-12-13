#include "ami_tool_i.h"

#include <stdbool.h>
#include <mbedtls/md.h>
#include <mbedtls/aes.h>
#include <string.h>
#include <stdlib.h>
#include <nfc/protocols/iso14443_3a/iso14443_3a.h>

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
#define AMIIBO_OFFSET_APP_ID (0x1DCU)
#define AMIIBO_APP_ID_SIZE (8U)
#define AMIIBO_OFFSET_SETTING_FLAGS (0x2CU)
#define AMIIBO_OFFSET_COUNTRY_CODE (0x2DU)
#define AMIIBO_OFFSET_INIT_DATE (0x30U)
#define AMIIBO_OFFSET_MODIFIED_DATE (0x32U)
#define AMIIBO_OFFSET_TITLE_ID (0xACU)
#define AMIIBO_OFFSET_APP_WRITE_COUNTER (0xB4U)
#define AMIIBO_OFFSET_APP_DATA_APP_ID (0xB6U)
#define AMIIBO_APP_DATA_APP_ID_SIZE (4U)
#define AMIIBO_OFFSET_APP_DATA_CRC (0xDCU)
#define AMIIBO_OFFSET_APP_DATA_BLOCK (0xE0U)
#define AMIIBO_APP_DATA_BLOCK_SIZE (0xD4U)
#define AMIIBO_OFFSET_MII_CHECKSUM (0xFEU)
#define AMIIBO_MII_CHECKSUM_INPUT_SIZE (AMIIBO_OFFSET_MII_CHECKSUM - AMIIBO_OFFSET_APP_DATA)
#define AMIIBO_SETTING_FLAG_USER_DATA (1U << 4)
#define AMIIBO_SETTING_FLAG_APP_DATA (1U << 5)
#define AMIIBO_OFFSET_UID_COPY (0x1D4U)
#define AMIIBO_OFFSET_AUTH_MAGIC (0x218U)
#define AMIIBO_AUTH_MAGIC_SIZE (2U)
#define AMIIBO_OFFSET_DYNAMIC_LOCK (0x208U)
#define AMIIBO_OFFSET_RESERVED (0x20BU)
#define AMIIBO_OFFSET_CONFIG (0x20CU)
#define AMIIBO_CONFIG_SIZE (16U)
#define AMIIBO_SIGNING_BUFFER_SIZE (480U)
#define AMIIBO_MAX_SEED_SIZE (480U)

static size_t amiibo_strnlen_safe(const char* str, size_t max_len) {
    size_t len = 0;
    if(!str) {
        return 0;
    }
    while((len < max_len) && str[len]) {
        len++;
    }
    return len;
}

static void amiibo_update_uid_checksums(uint8_t* raw) {
    if(!raw) {
        return;
    }
    raw[3] = (uint8_t)(raw[0] ^ raw[1] ^ raw[2] ^ 0x88);
    raw[8] = (uint8_t)(raw[4] ^ raw[5] ^ raw[6] ^ raw[7]);
}

static void amiibo_store_uid_copy(uint8_t* raw) {
    if(!raw) {
        return;
    }
    memcpy(raw + AMIIBO_OFFSET_UID_COPY, raw, 4);
    memcpy(raw + AMIIBO_OFFSET_UID_COPY + 4, raw + 4, 4);
}

static const uint32_t amiibo_crc32_table[256] = {
    0x00000000, 0x77073096, 0xEE0E612C, 0x990951BA, 0x076DC419, 0x706AF48F, 0xE963A535,
    0x9E6495A3, 0x0EDB8832, 0x79DCB8A4, 0xE0D5E91E, 0x97D2D988, 0x09B64C2B, 0x7EB17CBD,
    0xE7B82D07, 0x90BF1D91, 0x1DB71064, 0x6AB020F2, 0xF3B97148, 0x84BE41DE, 0x1ADAD47D,
    0x6DDDE4EB, 0xF4D4B551, 0x83D385C7, 0x136C9856, 0x646BA8C0, 0xFD62F97A, 0x8A65C9EC,
    0x14015C4F, 0x63066CD9, 0xFA0F3D63, 0x8D080DF5, 0x3B6E20C8, 0x4C69105E, 0xD56041E4,
    0xA2677172, 0x3C03E4D1, 0x4B04D447, 0xD20D85FD, 0xA50AB56B, 0x35B5A8FA, 0x42B2986C,
    0xDBBBC9D6, 0xACBCF940, 0x32D86CE3, 0x45DF5C75, 0xDCD60DCF, 0xABD13D59, 0x26D930AC,
    0x51DE003A, 0xC8D75180, 0xBFD06116, 0x21B4F4B5, 0x56B3C423, 0xCFBA9599, 0xB8BDA50F,
    0x2802B89E, 0x5F058808, 0xC60CD9B2, 0xB10BE924, 0x2F6F7C87, 0x58684C11, 0xC1611DAB,
    0xB6662D3D, 0x76DC4190, 0x01DB7106, 0x98D220BC, 0xEFD5102A, 0x71B18589, 0x06B6B51F,
    0x9FBFE4A5, 0xE8B8D433, 0x7807C9A2, 0x0F00F934, 0x9609A88E, 0xE10E9818, 0x7F6A0DBB,
    0x086D3D2D, 0x91646C97, 0xE6635C01, 0x6B6B51F4, 0x1C6C6162, 0x856530D8, 0xF262004E,
    0x6C0695ED, 0x1B01A57B, 0x8208F4C1, 0xF50FC457, 0x65B0D9C6, 0x12B7E950, 0x8BBEB8EA,
    0xFCB9887C, 0x62DD1DDF, 0x15DA2D49, 0x8CD37CF3, 0xFBD44C65, 0x4DB26158, 0x3AB551CE,
    0xA3BC0074, 0xD4BB30E2, 0x4ADFA541, 0x3DD895D7, 0xA4D1C46D, 0xD3D6F4FB, 0x4369E96A,
    0x346ED9FC, 0xAD678846, 0xDA60B8D0, 0x44042D73, 0x33031DE5, 0xAA0A4C5F, 0xDD0D7CC9,
    0x5005713C, 0x270241AA, 0xBE0B1010, 0xC90C2086, 0x5768B525, 0x206F85B3, 0xB966D409,
    0xCE61E49F, 0x5EDEF90E, 0x29D9C998, 0xB0D09822, 0xC7D7A8B4, 0x59B33D17, 0x2EB40D81,
    0xB7BD5C3B, 0xC0BA6CAD, 0xEDB88320, 0x9ABFB3B6, 0x03B6E20C, 0x74B1D29A, 0xEAD54739,
    0x9DD277AF, 0x04DB2615, 0x73DC1683, 0xE3630B12, 0x94643B84, 0x0D6D6A3E, 0x7A6A5AA8,
    0xE40ECF0B, 0x9309FF9D, 0x0A00AE27, 0x7D079EB1, 0xF00F9344, 0x8708A3D2, 0x1E01F268,
    0x6906C2FE, 0xF762575D, 0x806567CB, 0x196C3671, 0x6E6B06E7, 0xFED41B76, 0x89D32BE0,
    0x10DA7A5A, 0x67DD4ACC, 0xF9B9DF6F, 0x8EBEEFF9, 0x17B7BE43, 0x60B08ED5, 0xD6D6A3E8,
    0xA1D1937E, 0x38D8C2C4, 0x4FDFF252, 0xD1BB67F1, 0xA6BC5767, 0x3FB506DD, 0x48B2364B,
    0xD80D2BDA, 0xAF0A1B4C, 0x36034AF6, 0x41047A60, 0xDF60EFC3, 0xA867DF55, 0x316E8EEF,
    0x4669BE79, 0xCB61B38C, 0xBC66831A, 0x256FD2A0, 0x5268E236, 0xCC0C7795, 0xBB0B4703,
    0x220216B9, 0x5505262F, 0xC5BA3BBE, 0xB2BD0B28, 0x2BB45A92, 0x5CB36A04, 0xC2D7FFA7,
    0xB5D0CF31, 0x2CD99E8B, 0x5BDEAE1D, 0x9B64C2B0, 0xEC63F226, 0x756AA39C, 0x026D930A,
    0x9C0906A9, 0xEB0E363F, 0x72076785, 0x05005713, 0x95BF4A82, 0xE2B87A14, 0x7BB12BAE,
    0x0CB61B38, 0x92D28E9B, 0xE5D5BE0D, 0x7CDCEFB7, 0x0BDBDF21, 0x86D3D2D4, 0xF1D4E242,
    0x68DDB3F8, 0x1FDA836E, 0x81BE16CD, 0xF6B9265B, 0x6FB077E1, 0x18B74777, 0x88085AE6,
    0xFF0F6A70, 0x66063BCA, 0x11010B5C, 0x8F659EFF, 0xF862AE69, 0x616BFFD3, 0x166CCF45,
    0xA00AE278, 0xD70DD2EE, 0x4E048354, 0x3903B3C2, 0xA7672661, 0xD06016F7, 0x4969474D,
    0x3E6E77DB, 0xAED16A4A, 0xD9D65ADC, 0x40DF0B66, 0x37D83BF0, 0xA9BCAE53, 0xDEBB9EC5,
    0x47B2CF7F, 0x30B5FFE9, 0xBDBDF21C, 0xCABAC28A, 0x53B39330, 0x24B4A3A6, 0xBAD03605,
    0xCDD70693, 0x54DE5729, 0x23D967BF, 0xB3667A2E, 0xC4614AB8, 0x5D681B02, 0x2A6F2B94,
    0xB40BBE37, 0xC30C8EA1, 0x5A05DF1B, 0x2D02EF8D};

static uint32_t amiibo_crc32_calc(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFFu;
    for(size_t i = 0; i < length; i++) {
        const uint8_t index = (uint8_t)((crc ^ data[i]) & 0xFFu);
        crc = (crc >> 8) ^ amiibo_crc32_table[index];
    }
    return crc ^ 0xFFFFFFFFu;
}

static uint16_t amiibo_crc16_calc(const uint8_t* data, size_t length) {
    uint16_t crc = 0;
    for(size_t i = 0; i < length; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for(size_t bit = 0; bit < 8; bit++) {
            if(crc & 0x8000u) {
                crc = (crc << 1) ^ 0x1021u;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

static void amiibo_store_date(uint8_t* target, uint16_t year, uint8_t month, uint8_t day) {
    if(year < 2000) {
        year = 2000;
    }
    if(year > 2127) {
        year = 2127;
    }
    if(month < 1) {
        month = 1;
    }
    if(month > 12) {
        month = 12;
    }
    if(day < 1) {
        day = 1;
    }
    if(day > 31) {
        day = 31;
    }
    const uint16_t encoded =
        (uint16_t)(((year - 2000) & 0x7Fu) << 9) | (uint16_t)((month & 0x0Fu) << 5) |
        (uint16_t)(day & 0x1Fu);
    target[0] = (uint8_t)(encoded >> 8);
    target[1] = (uint8_t)(encoded & 0xFFu);
}

static void amiibo_write_checksums(uint8_t* raw) {
    const uint16_t crc16 =
        amiibo_crc16_calc(raw + AMIIBO_OFFSET_APP_DATA, AMIIBO_MII_CHECKSUM_INPUT_SIZE);
    raw[AMIIBO_OFFSET_MII_CHECKSUM] = (uint8_t)(crc16 >> 8);
    raw[AMIIBO_OFFSET_MII_CHECKSUM + 1] = (uint8_t)(crc16 & 0xFFu);

    const uint32_t crc32 =
        amiibo_crc32_calc(raw + AMIIBO_OFFSET_APP_DATA_BLOCK, AMIIBO_APP_DATA_BLOCK_SIZE);
    raw[AMIIBO_OFFSET_APP_DATA_CRC] = (uint8_t)(crc32 >> 24);
    raw[AMIIBO_OFFSET_APP_DATA_CRC + 1] = (uint8_t)(crc32 >> 16);
    raw[AMIIBO_OFFSET_APP_DATA_CRC + 2] = (uint8_t)(crc32 >> 8);
    raw[AMIIBO_OFFSET_APP_DATA_CRC + 3] = (uint8_t)(crc32 & 0xFFu);
}

static void amiibo_initialize_user_area(uint8_t* raw, const uint8_t* uuid) {
    raw[AMIIBO_OFFSET_SETTING_FLAGS] =
        (uint8_t)(raw[AMIIBO_OFFSET_SETTING_FLAGS] | AMIIBO_SETTING_FLAG_USER_DATA |
                  AMIIBO_SETTING_FLAG_APP_DATA);
    raw[AMIIBO_OFFSET_COUNTRY_CODE] = 0;
    amiibo_store_date(raw + AMIIBO_OFFSET_INIT_DATE, 2000, 1, 1);
    amiibo_store_date(raw + AMIIBO_OFFSET_MODIFIED_DATE, 2000, 1, 1);
    memset(raw + AMIIBO_OFFSET_TITLE_ID, 0, 4);
    raw[AMIIBO_OFFSET_APP_WRITE_COUNTER] = 0;
    raw[AMIIBO_OFFSET_APP_WRITE_COUNTER + 1] = 0;
    memset(raw + AMIIBO_OFFSET_APP_DATA_APP_ID, 0, AMIIBO_APP_DATA_APP_ID_SIZE);

    if(uuid) {
        memcpy(raw + AMIIBO_OFFSET_APP_ID, uuid, AMIIBO_APP_ID_SIZE);
    }

    amiibo_update_uid_checksums(raw);
    amiibo_store_uid_copy(raw);

    amiibo_write_checksums(raw);
}

static inline uint8_t* amiibo_bytes(MfUltralightData* tag_data) {
    return &tag_data->page[0].data[0];
}

static inline const uint8_t* amiibo_bytes_const(const MfUltralightData* tag_data) {
    return &tag_data->page[0].data[0];
}

static void amiibo_configure_rf_interface(MfUltralightData* tag_data) {
    if(!tag_data) {
        return;
    }
    uint8_t* raw = amiibo_bytes(tag_data);
    uint8_t uid[ISO14443_3A_UID_7_BYTES];
    uid[0] = raw[0];
    uid[1] = raw[1];
    uid[2] = raw[2];
    uid[3] = raw[4];
    uid[4] = raw[5];
    uid[5] = raw[6];
    uid[6] = raw[7];
    mf_ultralight_set_uid(tag_data, uid, sizeof(uid));

    Iso14443_3aData* iso = mf_ultralight_get_base_data(tag_data);
    if(iso) {
        static const uint8_t atqa[2] = {0x44, 0x00};
        iso14443_3a_set_atqa(iso, atqa);
        iso14443_3a_set_sak(iso, 0x04);
    }
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
    amiibo_update_uid_checksums(raw);

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

    uint8_t* prepared_seed = malloc(AMIIBO_MAX_SEED_SIZE);
    if(!prepared_seed) {
        return RFIDX_ARGUMENT_ERROR;
    }
    memset(prepared_seed, 0, AMIIBO_MAX_SEED_SIZE);
    size_t type_len = amiibo_strnlen_safe(input_key->typeString, sizeof(input_key->typeString));
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
    uint8_t* buffer = malloc(sizeof(uint16_t) + AMIIBO_MAX_SEED_SIZE);
    if(!buffer) {
        free(prepared_seed);
        return RFIDX_ARGUMENT_ERROR;
    }
    memset(buffer, 0, sizeof(uint16_t) + AMIIBO_MAX_SEED_SIZE);
    memcpy(buffer + sizeof(uint16_t), prepared_seed, prepared_seed_size);
    const size_t buffer_size = sizeof(uint16_t) + prepared_seed_size;

    mbedtls_md_context_t hmac_context;
    mbedtls_md_init(&hmac_context);
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    if(mbedtls_md_setup(&hmac_context, md_info, 1) != 0) {
        free(buffer);
        free(prepared_seed);
        mbedtls_md_free(&hmac_context);
        return RFIDX_ARGUMENT_ERROR;
    }
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
    free(buffer);
    free(prepared_seed);

    return RFIDX_OK;
}

RfidxStatus amiibo_cipher(const DerivedKey* data_key, MfUltralightData* tag_data) {
    if(!data_key || !tag_data) {
        return RFIDX_ARGUMENT_ERROR;
    }
    if(!amiibo_has_full_dump(tag_data)) {
        return RFIDX_ARGUMENT_ERROR;
    }

    uint8_t* raw = amiibo_bytes(tag_data);
    uint8_t* tag_cfg = raw + AMIIBO_OFFSET_TAG_CONFIG;
    uint8_t* app_data = raw + AMIIBO_OFFSET_APP_DATA;

    mbedtls_aes_context aes;
    mbedtls_aes_init(&aes);
    mbedtls_aes_setkey_enc(&aes, data_key->aesKey, 128);

    uint8_t nonce_counter[16] = {0};
    uint8_t stream_block[16] = {0};
    memcpy(nonce_counter, data_key->aesIV, sizeof(nonce_counter));
    size_t nc_offset = 0;

    // CTR stream must cover both encrypted regions consecutively even though they are
    // non-contiguous in raw tag memory.
    mbedtls_aes_crypt_ctr(
        &aes, AMIIBO_TAG_CONFIG_SIZE, &nc_offset, nonce_counter, stream_block, tag_cfg, tag_cfg);
    mbedtls_aes_crypt_ctr(
        &aes, AMIIBO_APP_DATA_SIZE, &nc_offset, nonce_counter, stream_block, app_data, app_data);

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

    uint8_t* signing_buffer = malloc(AMIIBO_SIGNING_BUFFER_SIZE);
    if(!signing_buffer) {
        return RFIDX_ARGUMENT_ERROR;
    }
    memset(signing_buffer, 0, AMIIBO_SIGNING_BUFFER_SIZE);
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

    free(signing_buffer);

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
    memset(raw + AMIIBO_OFFSET_AUTH_MAGIC, 0, AMIIBO_AUTH_MAGIC_SIZE);

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

    status = amiibo_format_dump(tag_data, header);
    if(status != RFIDX_OK) {
        return status;
    }

    amiibo_configure_rf_interface(tag_data);
    amiibo_initialize_user_area(raw, uuid);

    return RFIDX_OK;
}
