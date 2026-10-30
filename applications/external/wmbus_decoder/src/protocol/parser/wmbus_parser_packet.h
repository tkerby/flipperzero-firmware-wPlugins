#pragma once

#include "../packet/wmbus_packet_parts.h"

typedef struct {
    const WmBusPacketDllData* dll;
    const WmBusPacketTplData* tpl;
    const WmBusPacketPayloadView* payload;
    const WmBusPacketIdentityData* identity;
} WmBusParserPacketView;
