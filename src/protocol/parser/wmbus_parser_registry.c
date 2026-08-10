#include "wmbus_parser_registry.h"

#include "wmbus_parser_apator162.h"
#include "wmbus_parser_dif_vif.h"

typedef struct {
    WmBusParserId parser_id;
    bool validates_decrypt;
    bool (*probe)(const WmBusParserPacketView* packet);
    bool (*parse)(const WmBusParserPacketView* packet, WmBusPacketApplicationData* application);
} WmBusRegisteredParser;

/* First success wins, so specialized parsers must precede generic fallbacks. */
static const WmBusRegisteredParser wmbus_parsers[] = {
    {
        .parser_id = WmBusParserIdApator162,
        .validates_decrypt = true,
        .probe = wmbus_parser_apator162_probe,
        .parse = wmbus_parser_apator162_parse,
    },
    {
        .parser_id = WmBusParserIdDifVif,
        .validates_decrypt = false,
        .probe = wmbus_parser_dif_vif_probe,
        .parse = wmbus_parser_dif_vif_parse,
    },
};

static bool wmbus_parser_registry_try(
    const WmBusParserPacketView* packet,
    bool validate_decrypt_only,
    WmBusPacketApplicationData* out_application) {
    if(!packet) return false;

    for(size_t index = 0U; index < sizeof(wmbus_parsers) / sizeof(wmbus_parsers[0]); ++index) {
        const WmBusRegisteredParser* parser = &wmbus_parsers[index];
        if(validate_decrypt_only && !parser->validates_decrypt) continue;
        if(!parser->probe(packet)) continue;

        WmBusPacketApplicationData application = {.parser_id = parser->parser_id};
        if(!parser->parse(packet, &application)) continue;

        if(out_application) *out_application = application;
        return true;
    }

    return false;
}

bool wmbus_parser_registry_parse(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application) {
    return out_application && wmbus_parser_registry_try(packet, false, out_application);
}

bool wmbus_parser_registry_validate_decrypt(const WmBusParserPacketView* packet) {
    return wmbus_parser_registry_try(packet, true, NULL);
}
