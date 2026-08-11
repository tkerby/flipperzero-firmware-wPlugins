#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../capture/wmbus_capture.h"
#include "../crypto/wmbus_crypto_key_store.h"
#include "../model/wmbus_application_types.h"
#include "../../core/wmbus_types.h"
#include "wmbus_packet_parts.h"

typedef struct {
    WmBusPacketQuality quality;
    WmBusRxMode mode;
    WmBusFrameFormat format;
    uint16_t wire_len;
    /** Normalized frame length after removing link-layer CRC bytes. */
    uint16_t normalized_len;
    int rssi;
    uint32_t rx_tick;
    uint8_t wire_bytes[WMBUS_PHY_FRAME_MAX_BYTES];
    WmBusPacketDllData dll;
    WmBusPacketEllData ell;
    WmBusPacketTplData tpl;
    WmBusPacketIdentityData identity;
    WmBusPacketApplicationData application;
} WmBusPacketRecord;

bool wmbus_packet_process_phy_frame(
    const WmBusPhyFrame* phy_frame,
    const WmBusCryptoKeyStore* key_store,
    WmBusPacketRecord* record);

const char* wmbus_packet_quality_str(WmBusPacketQuality quality);
const char* wmbus_packet_quality_short_str(WmBusPacketQuality quality);
bool wmbus_packet_record_passes_policy(
    const WmBusPacketRecord* record,
    WmBusPacketQuality min_quality,
    int32_t min_rssi_dbm);
