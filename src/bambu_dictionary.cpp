#include "bambu_dictionary.h"

#include "bambu_crypto.h"

#include <toolbox/keys_dict.h>

#include <stdlib.h>
#include <string.h>

namespace {
#define BR_MF_CLASSIC_USER_DICT_PATH EXT_PATH("nfc/assets/mf_classic_dict_user.nfc")

bool key_in_set(const uint8_t* set, size_t count, const uint8_t key[MF_CLASSIC_KEY_SIZE]) {
    for(size_t i = 0; i < count; ++i) {
        if(memcmp(set + i * MF_CLASSIC_KEY_SIZE, key, MF_CLASSIC_KEY_SIZE) == 0) return true;
    }
    return false;
}

bool remember_key(
    KeysDict* dict,
    uint8_t* known_keys,
    size_t known_capacity,
    size_t* known_count,
    const uint8_t key[MF_CLASSIC_KEY_SIZE]) {
    if(key_in_set(known_keys, *known_count, key)) return true;
    if(*known_count >= known_capacity) return false;
    if(!keys_dict_add_key(dict, key, MF_CLASSIC_KEY_SIZE)) return false;

    memcpy(
        known_keys + (*known_count) * MF_CLASSIC_KEY_SIZE,
        key,
        MF_CLASSIC_KEY_SIZE);
    ++(*known_count);
    return true;
}
} // namespace

bool br_add_uid_batch_to_user_dictionary(
    const uint8_t (*uids)[BR_UID_SIZE],
    size_t uid_count) {
    if(!uids || uid_count == 0) return true;

    // All tag processing is finished before this function is called. From here
    // on, the only source data needed is the compact UID batch.
    KeysDict* dict = keys_dict_alloc(
        BR_MF_CLASSIC_USER_DICT_PATH, KeysDictModeOpenAlways, sizeof(MfClassicKey));
    if(!dict) return false;

    const size_t existing_count = keys_dict_get_total_keys(dict);
    const size_t derived_max = uid_count * BR_SECTOR_COUNT * 2U;
    const size_t known_capacity = existing_count + derived_max;
    uint8_t* known_keys = nullptr;
    if(known_capacity) {
        known_keys = static_cast<uint8_t*>(malloc(known_capacity * MF_CLASSIC_KEY_SIZE));
        if(!known_keys) {
            keys_dict_free(dict);
            return false;
        }
    }

    // Read the existing dictionary exactly once. Avoid per-key dictionary lookups,
    // which would repeatedly rescan the SD-card file for every derived key.
    size_t known_count = 0;
    for(size_t i = 0; i < existing_count; ++i) {
        uint8_t* dst = known_keys + known_count * MF_CLASSIC_KEY_SIZE;
        if(!keys_dict_get_next_key(dict, dst, MF_CLASSIC_KEY_SIZE)) break;
        ++known_count;
    }

    bool ok = true;
    for(size_t uid_index = 0; uid_index < uid_count && ok; ++uid_index) {
        MfClassicDeviceKeys keys{};
        if(!br_derive_sector_keys(uids[uid_index], &keys)) {
            ok = false;
            break;
        }

        for(uint8_t sector = 0; sector < BR_SECTOR_COUNT; ++sector) {
            if(!remember_key(
                   dict,
                   known_keys,
                   known_capacity,
                   &known_count,
                   keys.key_a[sector].data) ||
               !remember_key(
                   dict,
                   known_keys,
                   known_capacity,
                   &known_count,
                   keys.key_b[sector].data)) {
                ok = false;
                break;
            }
        }
    }

    free(known_keys);
    keys_dict_free(dict);
    return ok;
}
