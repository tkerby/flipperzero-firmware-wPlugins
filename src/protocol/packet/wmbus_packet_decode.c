#include "wmbus_packet_decode.h"

#include <string.h>

#include "../decode/wmbus_decode.h"
#include "../frame/wmbus_frame.h"
#include "wmbus_packet_security.h"

bool wmbus_packet_ci_has_ell(uint8_t ci) {
    switch(ci) {
    case 0x8C:
    case 0x8D:
    case 0x8E:
    case 0x8F:
        return true;
    default:
        return false;
    }
}

bool wmbus_packet_ell_has_session_fields(uint8_t ci) {
    return ci == 0x8DU || ci == 0x8FU;
}

static void wmbus_packet_populate_identity(WmBusPacketRecord* record, const uint8_t address[8]) {
    if(!record || !address) return;

    uint16_t m_field = (uint16_t)address[0] | ((uint16_t)address[1] << 8U);
    wmbus_frame_decode_mfg(m_field, record->identity.manufacturer);
    wmbus_frame_format_id(&address[2], record->identity.meter_id);
    record->identity.version = address[6];
    record->identity.dev_type = address[7];
}

static WmBusTplHeaderType wmbus_packet_ci_tpl_header_type(uint8_t ci) {
    switch(ci) {
    case 0x53:
    case 0x55:
    case 0x5B:
    case 0x5F:
    case 0x60:
    case 0x64:
    case 0x68:
    case 0x6C:
    case 0x6D:
    case 0x6F:
    case 0x72:
    case 0x75:
    case 0x7C:
    case 0x7E:
    case 0x80:
    case 0x82:
    case 0x87:
    case 0x8B:
    case 0x9F:
    case 0xC0:
    case 0xC2:
    case 0xC3:
    case 0xC5:
        return WmBusTplHeaderLong;
    case 0x56:
    case 0x57:
    case 0x5A:
    case 0x61:
    case 0x62:
    case 0x65:
    case 0x67:
    case 0x6E:
    case 0x74:
    case 0x7A:
    case 0x7D:
    case 0x7F:
    case 0x88:
    case 0x8A:
    case 0x92:
    case 0x93:
    case 0x9E:
    case 0xC1:
    case 0xC4:
    case 0xC6:
    case 0xC7:
        return WmBusTplHeaderShort;
    default:
        return WmBusTplHeaderNone;
    }
}

static void wmbus_packet_extract_tpl_info(
    const uint8_t* frame,
    size_t frame_len,
    size_t ci_pos,
    WmBusPacketRecord* record) {
    if(!frame || !record || ci_pos >= frame_len || ci_pos >= UINT8_MAX) {
        return;
    }

    record->tpl.ci_field = frame[ci_pos];
    record->tpl.header_len = 0U;

    WmBusTplHeaderType header_type = wmbus_packet_ci_tpl_header_type(record->tpl.ci_field);
    if(header_type == WmBusTplHeaderShort && frame_len >= ci_pos + 5U) {
        record->tpl.header_type = WmBusTplHeaderShort;
        record->tpl.header_len = (uint8_t)(ci_pos + 5U);
        memcpy(record->tpl.iv_address, &frame[2], sizeof(record->tpl.iv_address));
        record->tpl.acc = frame[ci_pos + 1U];
        record->tpl.cfg = (uint16_t)frame[ci_pos + 3U] | ((uint16_t)frame[ci_pos + 4U] << 8U);
        record->tpl.security_mode = wmbus_packet_tpl_security_mode(record->tpl.cfg);
    } else if(header_type == WmBusTplHeaderLong && frame_len >= ci_pos + 13U) {
        record->tpl.header_type = WmBusTplHeaderLong;
        record->tpl.header_len = (uint8_t)(ci_pos + 13U);
        memcpy(&record->tpl.iv_address[0], &frame[ci_pos + 5U], 2U);
        memcpy(&record->tpl.iv_address[2], &frame[ci_pos + 1U], 4U);
        record->tpl.iv_address[6] = frame[ci_pos + 7U];
        record->tpl.iv_address[7] = frame[ci_pos + 8U];
        record->tpl.acc = frame[ci_pos + 9U];
        record->tpl.cfg = (uint16_t)frame[ci_pos + 11U] | ((uint16_t)frame[ci_pos + 12U] << 8U);
        record->tpl.security_mode = wmbus_packet_tpl_security_mode(record->tpl.cfg);
        wmbus_packet_populate_identity(record, record->tpl.iv_address);
    } else if(header_type == WmBusTplHeaderNone) {
        record->tpl.header_len = (uint8_t)(ci_pos + 1U);
    }
}

static void wmbus_packet_extract_dll_tpl_info(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record) {
    if(!frame || !record || frame_len < 11U) return;

    record->dll.m_field = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);
    record->dll.ci_field = frame[10];
    wmbus_packet_populate_identity(record, &frame[2]);
    record->tpl.header_len = 0U;
    record->tpl.security_mode = 0U;
    record->ell.header_len = 11U;

    if(wmbus_packet_ci_has_ell(frame[10])) {
        record->ell.has_ell = true;
        if(frame_len < 13U) return;
        size_t pos = 11U;
        WmBusPacketEllData ell = {
            .has_ell = true,
            .header_len = 11U,
        };
        const uint8_t ell_ci = record->dll.ci_field;
        pos += 2U;

        if(ell_ci == 0x8EU || ell_ci == 0x8FU) {
            if(frame_len < pos + 8U) {
                return;
            }
            pos += 8U;
        }

        if(wmbus_packet_ell_has_session_fields(ell_ci)) {
            if(frame_len < pos + 6U) {
                return;
            }
            ell.has_session = true;
            uint32_t session_number = (uint32_t)frame[pos] | ((uint32_t)frame[pos + 1U] << 8U) |
                                      ((uint32_t)frame[pos + 2U] << 16U) |
                                      ((uint32_t)frame[pos + 3U] << 24U);
            ell.security_mode = wmbus_packet_ell_security_mode(session_number);
            pos += 4U;
            pos += 2U;
        }

        if(pos <= UINT8_MAX) {
            ell.header_len = (uint8_t)pos;
            record->ell = ell;
        }
    }

    size_t tpl_ci_pos = 10U;
    if(record->ell.has_ell) {
        if(record->ell.has_session && record->ell.security_mode != 0U) {
            record->tpl.header_len = record->ell.header_len;
            return;
        }
        tpl_ci_pos = record->ell.header_len;
    }

    wmbus_packet_extract_tpl_info(frame, frame_len, tpl_ci_pos, record);
}

bool wmbus_packet_decode_phy_frame(
    const WmBusPhyFrame* phy_frame,
    uint8_t* frame_buf,
    size_t frame_buf_max,
    WmBusPacketDecodeState* out) {
    if(!phy_frame || phy_frame->len > sizeof(phy_frame->data) || !frame_buf ||
       frame_buf_max == 0U || !out) {
        return false;
    }
    if((phy_frame->mode == WmBusRxModeT && phy_frame->format != WmBusFrameFormatA) ||
       (phy_frame->mode == WmBusRxModeC && phy_frame->format != WmBusFrameFormatA &&
        phy_frame->format != WmBusFrameFormatB) ||
       (phy_frame->mode != WmBusRxModeT && phy_frame->mode != WmBusRxModeC)) {
        return false;
    }

    memset(out, 0, sizeof(*out));
    if(!wmbus_decode_is_plausible_frame(phy_frame->data, phy_frame->len)) return true;

    out->quality = WmBusPacketQualityHeaderOk;

    WmBusFrameNormalizeResult normalized_result = {0};
    if(wmbus_frame_normalize(
           phy_frame->format,
           phy_frame->data,
           phy_frame->len,
           frame_buf,
           frame_buf_max,
           &normalized_result)) {
        out->frame = frame_buf;
        out->frame_len = normalized_result.normalized_len;
        out->wire_frame_len = normalized_result.computed_len;
        out->quality = normalized_result.crc_ok ? WmBusPacketQualityCrcOk :
                                                  WmBusPacketQualityFrameComplete;
    }

    return true;
}

void wmbus_packet_set_frame_metadata(
    WmBusPacketRecord* record,
    const uint8_t* frame,
    size_t frame_len) {
    if(!record || !frame || frame_len == 0U) return;

    record->normalized_len = (uint16_t)(frame_len > UINT16_MAX ? UINT16_MAX : frame_len);
    wmbus_packet_extract_dll_tpl_info(frame, frame_len, record);
}
