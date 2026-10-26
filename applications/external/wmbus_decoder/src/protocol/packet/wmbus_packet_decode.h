#pragma once

#include "wmbus_packet.h"

typedef struct {
    const uint8_t* frame;
    size_t frame_len;
    size_t wire_frame_len;
    WmBusPacketQuality quality;
} WmBusPacketDecodeState;

bool wmbus_packet_ci_has_ell(uint8_t ci);
bool wmbus_packet_ell_has_session_fields(uint8_t ci);

bool wmbus_packet_decode_phy_frame(
    const WmBusPhyFrame* phy_frame,
    uint8_t* frame_buf,
    size_t frame_buf_max,
    WmBusPacketDecodeState* out);

void wmbus_packet_set_frame_metadata(
    WmBusPacketRecord* record,
    const uint8_t* frame,
    size_t frame_len);
