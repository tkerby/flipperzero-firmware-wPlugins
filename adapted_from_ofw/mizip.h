#pragma once

#include "../mizip_balance_editor_i.h"
#include <bit_lib/bit_lib.h>
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>
#include <nfc/protocols/mf_classic/mf_classic.h>

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

void mizip_generate_key(uint8_t* uid, uint8_t keyA[5][KEY_LENGTH], uint8_t keyB[5][KEY_LENGTH]);
bool mizip_get_card_config(MizipCardConfig* config, MfClassicType type);
bool mizip_verify(Nfc* nfc);
bool mizip_read(void* context);
bool mizip_parse(void* context);
