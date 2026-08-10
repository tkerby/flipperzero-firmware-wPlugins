#include "wmbus_packet.h"

#include <furi.h>
#include <string.h>

#include "wmbus_packet_decode.h"
#include "wmbus_packet_parser.h"
#include "wmbus_packet_security.h"

static const char* const wmbus_packet_quality_text[WmBusPacketQualityCount][2] = {
    [WmBusPacketQualityAnyCapture] = {"Received", "RX"},
    [WmBusPacketQualityHeaderOk] = {"Header OK", "HDR OK"},
    [WmBusPacketQualityFrameComplete] = {"Frame complete", "LEN OK"},
    [WmBusPacketQualityCrcOk] = {"CRC OK", "CRC OK"},
    [WmBusPacketQualityParsed] = {"Decoded", "DECODED"},
};

const char* wmbus_packet_quality_str(WmBusPacketQuality quality) {
    return wmbus_packet_quality_text[wmbus_packet_quality_clamp(quality)][0];
}

const char* wmbus_packet_quality_short_str(WmBusPacketQuality quality) {
    return wmbus_packet_quality_text[wmbus_packet_quality_clamp(quality)][1];
}

bool wmbus_packet_record_passes_policy(
    const WmBusPacketRecord* record,
    WmBusPacketQuality min_quality,
    int32_t min_rssi_dbm) {
    if(!record) return false;
    if(min_rssi_dbm < 0 && record->rssi < min_rssi_dbm) return false;
    return wmbus_packet_quality_meets(record->quality, min_quality);
}

bool wmbus_packet_process_phy_frame(
    const WmBusPhyFrame* phy_frame,
    const WmBusCryptoKeyStore* key_store,
    WmBusPacketRecord* record) {
    if(!phy_frame || !record) return false;

    memset(record, 0, sizeof(*record));
    record->mode = phy_frame->mode;
    record->format = phy_frame->format;
    record->wire_len = (uint16_t)((phy_frame->len > sizeof(record->wire_bytes)) ?
                                      sizeof(record->wire_bytes) :
                                      phy_frame->len);
    record->rssi = phy_frame->rssi;
    record->rx_tick = furi_get_tick();
    memcpy(record->wire_bytes, phy_frame->data, record->wire_len);

    uint8_t normalized[WMBUS_PHY_FRAME_MAX_BYTES] = {0};
    uint8_t decrypt_workspace[WMBUS_PHY_FRAME_MAX_BYTES] = {0};
    WmBusPacketPayloadView payload = {0};
    WmBusPacketDecodeState decode = {0};
    if(!wmbus_packet_decode_phy_frame(phy_frame, normalized, sizeof(normalized), &decode)) {
        return false;
    }

    record->quality = decode.quality;

    bool has_complete_frame =
        wmbus_packet_quality_meets(decode.quality, WmBusPacketQualityFrameComplete) &&
        decode.frame && decode.frame_len > 0U;
    if(has_complete_frame) {
        if(decode.wire_frame_len > 0U && decode.wire_frame_len < record->wire_len) {
            record->wire_len = (uint16_t)decode.wire_frame_len;
        }
        wmbus_packet_set_frame_metadata(record, decode.frame, decode.frame_len);
    }

    if(has_complete_frame &&
       wmbus_packet_quality_meets(decode.quality, WmBusPacketQualityCrcOk) &&
       wmbus_packet_resolve_application_payload(
            decode.frame,
            decode.frame_len,
            record,
            key_store,
            decrypt_workspace,
            sizeof(decrypt_workspace),
            &payload) &&
       wmbus_packet_parse_application(record, &payload)) {
        record->quality = WmBusPacketQualityParsed;
    }

    wmbus_packet_finalize_parser(record);
    return true;
}
