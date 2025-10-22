#include <lib/nfc/protocols/mf_desfire/mf_desfire.h>
#include "../metroflip_i.h"
#include "desfire.h"
#include <lib/toolbox/strint.h>
#include <stdio.h>
#include <string.h> // memcmp, strcmp

bool app_id_matches(uint32_t input, const MfDesfireApplicationId* expected) {
    if(!expected) return false; // don't check expected->data (it's a fixed array)
    uint8_t bytes[MF_DESFIRE_APP_ID_SIZE] = {
        (input >> 16) & 0xFF, (input >> 8) & 0xFF, input & 0xFF};
    return memcmp(bytes, expected->data, MF_DESFIRE_APP_ID_SIZE) == 0;
}

typedef struct {
    uint32_t aid;
    const char* card_name;
    const char* company;
    bool locked;
} TransitCardInfo;

TransitCardInfo cards[88] = {
    {0x000001, "TTP (MAD) / beep (MNL)", "CRTM / AFPI", true},
    {0x000002, "beep (MNL)", "AFPI", true},
    {0x000003, "beep (MNL)", "AFPI", true},
    {0x000004, "beep (MNL)", "AFPI", true},
    {0x0011F2, "myki", "TV", false},
    {0x002000, "Presto (YYZ)", "Metrolinx", true},
    {0x004048, "Mi Movilidad (GDL)", "SITEUR", true},
    {0x004055, "AT HOP (AKL)", "Auckland Transport", true},
    {0x004063, "Travel Pass (DOH)", "Qatar Rail", true},
    {0x004078, "nol", "RTA", false},
    {0x008057, "NORTIC", "NRPA", true},
    {0x010000,
     "Breeze / Compass / EASY / FREEDOM / Urbana",
     "MARTA / TransLink / MIA County / PATCO / LPP",
     true},
    {0x012340, "motion (ECN)", "MoTCW", true},
    {0x012350, "motion (ECN)", "MoTCW", true},
    {0x012360, "motion (ECN)", "MoTCW", true},
    {0x018057, "NORTIC", "NRPA", true},
    {0x0112F2, "Tap-N-Go / peggo", "GBMT / YWG Transit", true},
    {0x014D44, "DMTC (DEL)", "DMRCL", true},
    {0x020000, "Urbana (LJU)", "LPP", true},
    {0x0212F2, "Tap-N-Go (GRB)", "GBM Transit", true},
    {0x024D44, "DMTC (DEL)", "DMRCL", true},
    {0x034D44, "DMTC (DEL)", "DMRCL", true},
    {0x044D44, "DMTC (DEL)", "DMRCL", true},
    {0x050000, "T-mobilitat (BCN) / Urbana (LJU)", "TMB / LPP", true},
    {0x054D44, "DMTC (DEL)", "DMRCL", true},
    {0x064D44, "DMTC (DEL)", "DMRCL", true},
    {0x074D44, "DMTC (DEL)", "DMRCL", true},
    {0x1101F4, "itso", "ITSO (UK)", false},
    {0x1120EF, "HSL (HEL)", "HRT", true},
    {0x1201F4, "itso", "ITSO (UK)", false},
    {0x1301F4, "itso", "ITSO (UK)", false},
    {0x1401F4, "itso", "ITSO (UK)", false},
    {0x1602A0, "itso", "ITSO (UK)", false},
    {0x171108, "TRIPKO (MNL)", "JourneyTech", true},
    {0x227508, "Umo", "Cubic", true},
    {0x3010F2, "ORCA (SEA)", "ORCA", true},
    {0x314553, "opal", "Opal", false},
    {0x315441, "ATH.ENA (ATH)", "OASA", true},
    {0x31594F, "Oyster (LHR)", "TfL", true},
    {0x4012F2, "Connect (SMF)", "SACOG", true},
    {0x422201, "Istanbulkart (IST)", "BELBIM", true},
    {0x422202, "Istanbulkart (IST)", "BELBIM", true},
    {0x422206, "Istanbulkart (IST)", "BELBIM", true},
    {0x425301, "MRT SVC (BKK) / Rabbit (BKK)", "BEM / BTS", true},
    {0x425302, "MRT SVC (BKK) / Rabbit (BKK)", "BEM / BTS", true},
    {0x425303, "MRT SVC (BKK) / Rabbit (BKK)", "BEM / BTS", true},
    {0x425304, "MRT SVC (BKK) / Rabbit (BKK)", "BEM / BTS", true},
    {0x425305, "MRT SVC (BKK) / Rabbit (BKK)", "BEM / BTS", true},
    {0x425306, "MRT SVC (BKK) / Rabbit (BKK)", "BEM / BTS", true},
    {0x425307, "MRT SVC (BKK) / Rabbit (BKK)", "BEM / BTS", true},
    {0x425308, "MRT SVC (BKK) / Rabbit (BKK)", "BEM / BTS", true},
    {0x425309, "MRT SVC (BKK) / Rabbit (BKK)", "BEM / BTS", true},
    {0x42530A, "MRT SVC (BKK)", "BEM", true},
    {0x42530B, "MRT SVC (BKK)", "BEM", true},
    {0x42530C, "MRT SVC (BKK)", "BEM", true},
    {0x42530D, "MRT SVC (BKK)", "BEM", true},
    {0x42530E, "MRT SVC (BKK)", "BEM", true},
    {0x42530F, "MRT SVC (BKK)", "BEM", true},
    {0x425310, "MRT SVC (BKK)", "BEM", true},
    {0x425311, "MRT SVC (BKK)", "BEM", true},
    {0x5010F2, "Metrocard (CHC)", "ECan", true},
    {0x5011F2, "Litacka Opencard", "Haguess", true},
    {0x6013F2, "HOLO", "Honolulu County", true},
    {0x7A007A, "TAP & GO (LAS)", "RTC", true},
    {0x7D23A4, "Umo", "Cubic", true},
    {0x805BC6, "Umo", "Cubic", true},
    {0x8E7F67, "Umo", "Cubic", true},
    {0x8113F2, "Ventra (ORD)", "CTA", true},
    {0x9011F2, "clipper", "Clipper", false},
    {0x9013F2, "Bee (DUD)", "Otago RC", true},
    {0x9111F2, "clipper", "Clipper", false},
    {0xA012F2, "Go CT", "CTtransit", true},
    {0xA013F2, "Wave (PVD)", "RIPTA", true},
    {0xAF1122, "Leap (DUB)", "TFI", true},
    {0xB006F2, "metroCARD (ADL)", "Adelaide Metro", true},
    {0xB52C99, "Umo", "Cubic", true},
    {0xCA3490, "City Card (SOF)", "UMC", true},
    {0xCC00CC, "Smartcard (CMH)", "COTA", true},
    {0xD000D0, "Tapp Pay (DAY)", "RTA", true},
    {0xDD00DD, "MyRide (DEN)", "RTD", true},
    {0xD001F0, "BAT (VIT)", "Euskotren", true},
    {0xE010F2, "Hop Fastpass (PDX)", "TriMet", true},
    {0xF00000, "OMNY (JFK)", "MTA", true},
    {0xF010F2, "myki", "myki", false},
    {0xF18301, "URBANCARD (WRO)", "UTS", true},
    {0xF18302, "URBANCARD (WRO)", "UTS", true},
    {0xF18303, "URBANCARD (WRO)", "UTS", true},
    {0xFF30FF, "Presto (YYZ)", "Metrolinx", true},
};

int num_cards = sizeof(cards) / sizeof(cards[0]);

const char* desfire_type(const MfDesfireData* data) {
    if(!data) return "Unknown Card";
    if(!data->application_ids) return "Unknown Card";

    for(int i = 0; i < num_cards; i++) {
        const uint32_t card_app_id_count = simple_array_get_count(data->application_ids);
        for(uint32_t j = 0; j < card_app_id_count; ++j) {
            const MfDesfireApplicationId* app_ptr =
                (const MfDesfireApplicationId*)simple_array_cget(data->application_ids, j);
            if(!app_ptr) continue; // defensive: skip invalid entries

            if(app_id_matches(cards[i].aid, app_ptr)) {
                FURI_LOG_I("Metroflip:DesfireManager", "matches with %s!", cards[i].card_name);
                return cards[i].card_name;
            }
        }
    }
    return "Unknown Card";
}

bool is_desfire_locked(const char* card_name) {
    if(!card_name) return true; // treat null/unknown as locked (conservative)
    for(int i = 0; i < num_cards; i++) {
        if(strcmp(card_name, cards[i].card_name) == 0) {
            return cards[i].locked;
        }
    }
    return true;
}
