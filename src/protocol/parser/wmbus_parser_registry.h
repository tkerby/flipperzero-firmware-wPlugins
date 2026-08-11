#pragma once

#include <stdbool.h>

#include "../model/wmbus_application_types.h"
#include "wmbus_parser_packet.h"

bool wmbus_parser_registry_parse(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application);
