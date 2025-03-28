#include "mizip.h"
#include <flipper_application/flipper_application.h>
#include <nfc/nfc_device.h>
#include <bit_lib/bit_lib.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

#define TAG "MiZIP"

#define KEY_LENGTH       6
#define MIZIP_KEY_TO_GEN 5
#define UID_LENGTH       4

typedef struct {
    uint64_t a;
    uint64_t b;
} MfClassicKeyPair;

typedef struct {
    MfClassicKeyPair* keys;
    uint32_t verify_sector;
} MizipCardConfig;

static MfClassicKeyPair mizip_1k_keys[] = {
    {.a = 0xa0a1a2a3a4a5, .b = 0xb4c132439eef}, // 000
    {.a = 0x000000000000, .b = 0x000000000000}, // 001
    {.a = 0x000000000000, .b = 0x000000000000}, // 002
    {.a = 0x000000000000, .b = 0x000000000000}, // 003
    {.a = 0x000000000000, .b = 0x000000000000}, // 004
    {.a = 0x0222179AB995, .b = 0x13321774F9B5}, // 005
    {.a = 0xB25CBD76A7B4, .b = 0x7571359B4274}, // 006
    {.a = 0xDA857B4907CC, .b = 0xD26B856175F7}, // 007
    {.a = 0x16D85830C443, .b = 0x8F790871A21E}, // 008
    {.a = 0x88BD5098FC82, .b = 0xFCD0D77745E4}, // 009
    {.a = 0x983349449D78, .b = 0xEA2631FBDEDD}, // 010
    {.a = 0xC599F962F3D9, .b = 0x949B70C14845}, // 011
    {.a = 0x72E668846BE8, .b = 0x45490B5AD707}, // 012
    {.a = 0xBCA105E5685E, .b = 0x248DAF9D674D}, // 013
    {.a = 0x4F6FE072D1FD, .b = 0x4250A05575FA}, // 014
    {.a = 0x56438ABE8152, .b = 0x59A45912B311}, // 015
};

static MfClassicKeyPair mizip_mini_keys[] = {
    {.a = 0xa0a1a2a3a4a5, .b = 0xb4c132439eef}, // 000
    {.a = 0x000000000000, .b = 0x000000000000}, // 001
    {.a = 0x000000000000, .b = 0x000000000000}, // 002
    {.a = 0x000000000000, .b = 0x000000000000}, // 003
    {.a = 0x000000000000, .b = 0x000000000000}, // 004
};

//KDF
void mizip_generate_key(uint8_t* uid, uint8_t keyA[5][KEY_LENGTH], uint8_t keyB[5][KEY_LENGTH]) {
    // Static XOR table for key generation
    static const uint8_t xor_table_keyA[4][6] = {
        {0x09, 0x12, 0x5A, 0x25, 0x89, 0xE5},
        {0xAB, 0x75, 0xC9, 0x37, 0x92, 0x2F},
        {0xE2, 0x72, 0x41, 0xAF, 0x2C, 0x09},
        {0x31, 0x7A, 0xB7, 0x2F, 0x44, 0x90}};

    static const uint8_t xor_table_keyB[4][6] = {
        {0xF1, 0x2C, 0x84, 0x53, 0xD8, 0x21},
        {0x73, 0xE7, 0x99, 0xFE, 0x32, 0x41},
        {0xAA, 0x4D, 0x13, 0x76, 0x56, 0xAE},
        {0xB0, 0x13, 0x27, 0x27, 0x2D, 0xFD}};

    // Permutation table for rearranging elements in uid
    static const uint8_t xorOrderA[6] = {0, 1, 2, 3, 0, 1};
    static const uint8_t xorOrderB[6] = {2, 3, 0, 1, 2, 3};

    // Generate key based on uid and XOR table
    for(uint8_t j = 1; j < 5; j++) {
        for(uint8_t i = 0; i < 6; i++) {
            keyA[j][i] = uid[xorOrderA[i]] ^ xor_table_keyA[j - 1][i];
            keyB[j][i] = uid[xorOrderB[i]] ^ xor_table_keyB[j - 1][i];
        }
    }
}

static bool mizip_get_card_config(MizipCardConfig* config, MfClassicType type) {
    bool success = true;

    if(type == MfClassicType1k) {
        config->verify_sector = 0;
        config->keys = mizip_1k_keys;
    } else if(type == MfClassicTypeMini) {
        config->verify_sector = 0;
        config->keys = mizip_mini_keys;
    } else {
        success = false;
    }

    return success;
}

bool mizip_verify(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    const MfClassicData* data = app->mf_classic_data;

    bool is_mizip = false;

    do {
        // Verify card type
        MizipCardConfig cfg = {};
        if(!mizip_get_card_config(&cfg, data->type)) break;

        // Verify key
        MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(data, cfg.verify_sector);
        uint64_t key = bit_lib_bytes_to_num_be(sec_tr->key_b.data, 6);
        if(key != cfg.keys[cfg.verify_sector].b) return false;
        is_mizip = true;
    } while(false);

    return is_mizip;
}
