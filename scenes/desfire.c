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

TransitCardInfo cards[44] = {
    { 0x010000, "Breeze (ATL)", "MARTA", true},
    { 0x002000, "Presto (YYZ)", "Metrolinx", true},
    { 0xF00000, "OMNY (JFK)", "MTA", true},
    { 0x422201, "Istanbulkart (IST)", "BELBIM", true},
    { 0x422202, "Istanbulkart (IST)", "BELBIM", true},
    { 0x422206, "Istanbulkart (IST)", "BELBIM", true},
    { 0x425301, "MRT SVC (BKK)", "BEM", true},
    { 0x425302, "MRT SVC (BKK)", "BEM", true},
    { 0x425303, "MRT SVC (BKK)", "BEM", true},
    { 0x425304, "MRT SVC (BKK)", "BEM", true},
    { 0x425305, "MRT SVC (BKK)", "BEM", true},
    { 0x425306, "MRT SVC (BKK)", "BEM", true},
    { 0x425307, "MRT SVC (BKK)", "BEM", true},
    { 0x425308, "MRT SVC (BKK)", "BEM", true},
    { 0x425309, "MRT SVC (BKK)", "BEM", true},
    { 0x42530A, "MRT SVC (BKK)", "BEM", true},
    { 0x42530B, "MRT SVC (BKK)", "BEM", true},
    { 0x42530C, "MRT SVC (BKK)", "BEM", true},
    { 0x42530D, "MRT SVC (BKK)", "BEM", true},
    { 0x42530E, "MRT SVC (BKK)", "BEM", true},
    { 0x42530F, "MRT SVC (BKK)", "BEM", true},
    { 0x425310, "MRT SVC (BKK)", "BEM", true},
    { 0x425311, "MRT SVC (BKK)", "BEM", true},
    { 0x9011F2, "Clipper (SFO)", "MTC", false},
    { 0x9111F2, "Clipper (SFO)", "MTC", false},
    { 0x050000, "T-mobilitat (BCN)", "TMB", true},
    { 0x314553, "Opal (SYD)", "TfNSW", false},
    { 0x0011F2, "myki (MEL)", "DoTP", false},
    { 0xF010F2, "myki (MEL)", "DoTP", false},
    { 0x1602A0, "ITSO (UK)", "ITSO", false},
    { 0x000001, "TTP (MAD)", "CRTM", true},
    { 0xF206B0, "metroCARD (ADL)", "ADL Metro", true},
    { 0x554000, "AT HOP (AKL)", "AKL Transport", true},
    { 0xF21050, "Metrocard (CHC)", "ECan", true},
    { 0xCC00CC, "Smartcard (CMH)", "COTA", true},
    { 0xD000D0, "Tapp Pay (DAY)", "RTA", true},
    { 0xDD00DD, "MyRide (DEN)", "RTD", true},
    { 0xAF1122, "Leap (DUB)", "TFI", true},
    { 0x9013F2, "Bee (DUD)", "Otago RC", true},
    { 0x31594F, "Oyster (LHR)", "TfL", true},
    { 0xE010F2, "Hop Fastpass (PDX)", "TriMet", true},
    { 0xF213F0, "ORCA (SEA)", "ORCA", true},
    { 0x9034CA, "Sofia City Card (SOF)", "UMC", true},
    { 0xF21201, "peggo (YWG)", "YWG Transit", true},
    { 0xFF30FF, "Presto (YYZ)", "Metrolinx", true},
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
