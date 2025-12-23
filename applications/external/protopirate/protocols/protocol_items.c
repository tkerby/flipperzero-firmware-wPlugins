#include "protocol_items.h"
#include <string.h>

const SubGhzProtocol* protopirate_protocol_registry_items[] = {
    &kia_protocol_v0,
    &kia_protocol_v1,
    &kia_protocol_v2,
    &kia_protocol_v3_v4,
    &kia_protocol_v5,
    &ford_protocol_v0,
    &subaru_protocol,
    &suzuki_protocol,
    &vw_protocol,
};

const SubGhzProtocolRegistry protopirate_protocol_registry = {
    .items = protopirate_protocol_registry_items,
    .size = COUNT_OF(protopirate_protocol_registry_items),
};

// Protocol timing definitions - mirrors the SubGhzBlockConst in each protocol
static const ProtoPirateProtocolTiming protocol_timings[] = {
    // Kia V0: PWM encoding, 250/500µs
    {
        .name = "Kia V0",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 61,
    },
    // Kia V1: OOK PCM 800µs timing
    {
        .name = "Kia V1",
        .te_short = 800,
        .te_long = 1600,
        .te_delta = 200,
        .min_count_bit = 56,
    },
    // Kia V2: Manchester 500/1000µs
    {
        .name = "Kia V2",
        .te_short = 500,
        .te_long = 1000,
        .te_delta = 150,
        .min_count_bit = 51,
    },
    // Kia V3/V4: PWM 400/800µs
    {
        .name = "Kia V3/V4",
        .te_short = 400,
        .te_long = 800,
        .te_delta = 150,
        .min_count_bit = 64,
    },
    // Kia V5: PWM 400/800µs (same as V3/V4)
    {
        .name = "Kia V5",
        .te_short = 400,
        .te_long = 800,
        .te_delta = 150,
        .min_count_bit = 64,
    },
    // Ford V0: Manchester 250/500µs
    {
        .name = "Ford V0",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 64,
    },
    // Subaru: PWM 800/1600µs
    {
        .name = "Subaru",
        .te_short = 800,
        .te_long = 1600,
        .te_delta = 250,
        .min_count_bit = 64,
    },
    // Suzuki: PWM 250/500µs
    {
        .name = "Suzuki",
        .te_short = 250,
        .te_long = 500,
        .te_delta = 100,
        .min_count_bit = 64,
    },
    // VW: Manchester 500/1000µs
    {
        .name = "VW",
        .te_short = 500,
        .te_long = 1000,
        .te_delta = 120,
        .min_count_bit = 80,
    },
};

static const size_t protocol_timings_count = COUNT_OF(protocol_timings);

const ProtoPirateProtocolTiming* protopirate_get_protocol_timing(const char* protocol_name) {
    if(!protocol_name) return NULL;

    for(size_t i = 0; i < protocol_timings_count; i++) {
        // Check for exact match or if the protocol name contains our timing name
        if(strcmp(protocol_name, protocol_timings[i].name) == 0 ||
           strstr(protocol_name, protocol_timings[i].name) != NULL) {
            return &protocol_timings[i];
        }
    }

    // Try partial matching for version variants
    for(size_t i = 0; i < protocol_timings_count; i++) {
        // Match "Kia" protocols
        if(strstr(protocol_name, "Kia") != NULL || strstr(protocol_name, "KIA") != NULL ||
           strstr(protocol_name, "HYU") != NULL) {
            // Try to match version number
            if(strstr(protocol_name, "V0") != NULL &&
               strstr(protocol_timings[i].name, "V0") != NULL) {
                return &protocol_timings[i];
            }
            if(strstr(protocol_name, "V1") != NULL &&
               strstr(protocol_timings[i].name, "V1") != NULL) {
                return &protocol_timings[i];
            }
            if(strstr(protocol_name, "V2") != NULL &&
               strstr(protocol_timings[i].name, "V2") != NULL) {
                return &protocol_timings[i];
            }
            if((strstr(protocol_name, "V3") != NULL || strstr(protocol_name, "V4") != NULL) &&
               strstr(protocol_timings[i].name, "V3/V4") != NULL) {
                return &protocol_timings[i];
            }
            if(strstr(protocol_name, "V5") != NULL &&
               strstr(protocol_timings[i].name, "V5") != NULL) {
                return &protocol_timings[i];
            }
        }

        // Match Ford
        if(strstr(protocol_name, "Ford") != NULL &&
           strstr(protocol_timings[i].name, "Ford") != NULL) {
            return &protocol_timings[i];
        }

        // Match Subaru
        if(strstr(protocol_name, "Subaru") != NULL &&
           strstr(protocol_timings[i].name, "Subaru") != NULL) {
            return &protocol_timings[i];
        }

        // Match Suzuki
        if(strstr(protocol_name, "Suzuki") != NULL &&
           strstr(protocol_timings[i].name, "Suzuki") != NULL) {
            return &protocol_timings[i];
        }

        // Match VW
        if(strstr(protocol_name, "VW") != NULL && strstr(protocol_timings[i].name, "VW") != NULL) {
            return &protocol_timings[i];
        }
    }

    return NULL;
}

const ProtoPirateProtocolTiming* protopirate_get_protocol_timing_by_index(size_t index) {
    if(index >= protocol_timings_count) return NULL;
    return &protocol_timings[index];
}

size_t protopirate_get_protocol_timing_count(void) {
    return protocol_timings_count;
}
