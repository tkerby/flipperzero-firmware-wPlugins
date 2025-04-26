#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>
#include "../metroflip_i.h"
#include "desfire.h"
#include <lib/toolbox/strint.h>
#include <stdio.h>

bool app_id_matches(uint32_t input, const MfDesfireApplicationId* expected) {
    uint8_t bytes[3] = {
        (input >> 16) & 0xFF,
        (input >> 8) & 0xFF,
        input & 0xFF
    };
    return memcmp(bytes, expected->data, 3) == 0;
}

typedef struct {
    uint32_t aid;
    const char* card_name;
    const char* company;
    bool locked;
} TransitCardInfo;

TransitCardInfo cards[9] = {
    { 0x9011F2, "clipper", "Clipper", false},
    { 0x9111F2, "clipper", "Clipper",false},
    { 0x422201, "istanbulkart", "Istanbulkart", true},
    { 0x050000, "tmb", "T-mobilitat Barcelona", true},
    { 0x314553, "opal", "Opal", false},
    { 0x0011f2, "myki", "myki", false},
    { 0x1602a0, "itso", "ITSO", false},
    { 0x000001, "crtm", "CRTM Madrid", true},
    { 0x010000, "marta", "MARTA Atlanta", true},
};

int num_cards = sizeof(cards) / sizeof(cards[0]);

const char* desfire_type(const MfDesfireData* data) {
    for(int i = 0; i < num_cards; i++) {  // Outer loop for cards
        const uint32_t card_app_id_count = simple_array_get_count(data->application_ids);

        for(uint32_t j = 0; j < card_app_id_count; ++j) {  // Inner loop for app_ids
            const MfDesfireApplicationId app_id = *(const MfDesfireApplicationId*)simple_array_cget(data->application_ids, j);
            if(app_id_matches(cards[i].aid, &app_id)) {
                FURI_LOG_I("Metroflip:DesfireManager", "matches with %s!", cards[i].company);
                if(cards[i].locked) {
                    return cards[i].company;
                }
                return cards[i].card_name;
            } 
        }
    }
    return "Unknown Card";
}

bool is_desfire_locked(const char* card_name) {
    for(int i = 0; i < num_cards; i++) {
        if(strcmp(card_name, cards[i].card_name) == 0) {
            return cards[i].locked; //return card locked state
        } 
    }
    return true; // if it's not found, return true (so locked)
}