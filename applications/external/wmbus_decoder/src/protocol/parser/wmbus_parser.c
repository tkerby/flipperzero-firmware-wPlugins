#include "wmbus_parser.h"

#include <stddef.h>

static const char* const wmbus_parser_names[] = {
    [WmBusParserIdUnknown] = "Unknown",
    [WmBusParserIdRaw] = "Raw",
    [WmBusParserIdHeader] = "Header",
    [WmBusParserIdEll] = "ELL",
    [WmBusParserIdShortTpl] = "Short TPL",
    [WmBusParserIdLongTpl] = "Long TPL",
    [WmBusParserIdDifVif] = "DifVif",
    [WmBusParserIdApator162] = "Apator162",
    [WmBusParserIdQWaterV2] = "QWaterV2",
};

const char* wmbus_parser_id_name(WmBusParserId parser_id) {
    size_t index = (size_t)parser_id;
    if(index >= sizeof(wmbus_parser_names) / sizeof(wmbus_parser_names[0]) ||
       !wmbus_parser_names[index]) {
        return wmbus_parser_names[WmBusParserIdUnknown];
    }
    return wmbus_parser_names[index];
}
