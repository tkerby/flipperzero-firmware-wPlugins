#include "wmbus_parser_registry.h"

#include "wmbus_parser_apator162.h"
#include "wmbus_parser_dif_vif.h"
#include "wmbus_parser_qwaterv2.h"

typedef struct {
    WmBusParserId parser_id;
    bool (*parse)(const WmBusParserPacketView* packet, WmBusPacketApplicationData* application);
} WmBusRegisteredParser;

/* First success wins, so specialized parsers must precede generic fallbacks. */
static const WmBusRegisteredParser wmbus_parsers[] = {
    {
        .parser_id = WmBusParserIdQWaterV2,
        .parse = wmbus_parser_qwaterv2_parse,
    },
    {
        .parser_id = WmBusParserIdApator162,
        .parse = wmbus_parser_apator162_parse,
    },
    {
        .parser_id = WmBusParserIdDifVif,
        .parse = wmbus_parser_dif_vif_parse,
    },
};

bool wmbus_parser_registry_parse(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application) {
    if(!packet || !out_application) return false;

    for(size_t index = 0U; index < sizeof(wmbus_parsers) / sizeof(wmbus_parsers[0]); ++index) {
        const WmBusRegisteredParser* parser = &wmbus_parsers[index];
        WmBusPacketApplicationData application = {0};
        if(!parser->parse(packet, &application)) continue;

        application.parser_id = parser->parser_id;
        *out_application = application;
        return true;
    }

    return false;
}
