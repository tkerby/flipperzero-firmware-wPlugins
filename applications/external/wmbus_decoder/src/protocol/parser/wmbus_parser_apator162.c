#include "wmbus_parser_apator162.h"

#include "../model/wmbus_application_record.h"

#include <string.h>

#define WMBUS_APATOR162_MFG_OLD    0x8614U
#define WMBUS_APATOR162_META_LEN   8U
#define WMBUS_APATOR162_STATUS_LEN 7U

/**
 * Returns the payload size for a single Apator register body.
 *
 * The size excludes the register byte itself. Unknown register codes return -1.
 */
int wmbus_parser_apator162_register_size(uint8_t reg) {
    if(reg >= 0x71U && reg <= 0x7BU) {
        return 1 + 4 * (reg - 0x6FU);
    }
    if(reg >= 0xB6U && reg <= 0xC7U) {
        return 3;
    }

    switch(reg) {
    case 0xA2:
    case 0xA5:
    case 0xA9:
    case 0xAF:
        return 1;
    case 0x11:
    case 0x41:
    case 0x43:
    case 0xA7:
    case 0xA8:
    case 0xAA:
    case 0xAB:
    case 0xAC:
    case 0xAD:
    case 0xB4:
        return 2;
    case 0x01:
    case 0x44:
    case 0xA6:
    case 0xD0:
    case 0xD3:
        return 3;
    case 0x00:
    case 0x10:
    case 0x42:
    case 0xA0:
    case 0xA1:
    case 0xA4:
    case 0xF0:
        return 4;
    case 0xB0:
        return 5;
    case 0x40:
    case 0x8B:
    case 0x8C:
        return 6;
    case 0x8E:
    case 0xA3:
        return 7;
    case 0xB1:
    case 0xB3:
        return 8;
    case 0x8A:
        return 9;
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x86:
    case 0x87:
        return 10;
    case 0x85:
    case 0x88:
    case 0x8F:
        return 11;
    case 0xB2:
    case 0xB5:
        return 16;
    default:
        return -1;
    }
}

static uint32_t wmbus_parser_apator162_read_u32_le(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void wmbus_parser_apator162_store_total_record(
    WmBusPacketApplicationData* application,
    uint32_t total_m3_x1000) {
    WmBusApplicationRecord* app_record = NULL;
    if(!application || !wmbus_application_record_append(application, &app_record)) return;

    app_record->quantity = WmBusApplicationQuantityVolume;
    app_record->scale10 = -3;
    app_record->data_len = 4U;
    wmbus_application_record_set_value(app_record, (int64_t)total_m3_x1000);
}

static void wmbus_parser_apator162_store_status_record(
    WmBusPacketApplicationData* application,
    const uint8_t* status,
    size_t status_len) {
    WmBusApplicationRecord* app_record = NULL;
    if(!application || !status || status_len == 0U ||
       !wmbus_application_record_append(application, &app_record)) {
        return;
    }

    app_record->quantity = WmBusApplicationQuantityStatus;
    const uint8_t data_len = (uint8_t)((status_len > UINT8_MAX) ? UINT8_MAX : status_len);
    if(!wmbus_application_record_set_raw_bytes(app_record, status, data_len)) {
        app_record->value_type = WmBusApplicationValueRaw;
    }
}

/**
 * Locates the first Apator register in an application payload.
 *
 * Leading 0x2F filler bytes are skipped.
 *
 * The upstream `wmbusmeters` XMQ grammar models Apator 162 payloads as:
 * `2F* <1-byte proprietary marker> <7-byte status block> <register stream> FF*`
 */
static bool wmbus_parser_apator162_locate_fields(
    const uint8_t* payload,
    size_t payload_len,
    size_t* fields_pos) {
    if(!payload || !fields_pos || payload_len == 0U) {
        return false;
    }

    size_t pos = 0U;
    while(pos < payload_len && payload[pos] == 0x2FU) {
        pos++;
    }

    if(pos >= payload_len) {
        return false;
    }

    if((payload_len - pos) < WMBUS_APATOR162_META_LEN) {
        return false;
    }

    const size_t first_register_pos = pos + WMBUS_APATOR162_META_LEN;
    if(first_register_pos >= payload_len) {
        return false;
    }

    *fields_pos = first_register_pos;
    return true;
}

static bool wmbus_parser_apator162_scan_stream(
    const uint8_t* payload,
    size_t payload_len,
    size_t fields_pos,
    uint32_t* total_m3_x1000) {
    if(!payload || fields_pos >= payload_len) {
        return false;
    }

    bool have_total = false;
    size_t pos = fields_pos;

    while(pos < payload_len) {
        uint8_t reg = payload[pos++];
        if(reg == 0xFFU) {
            break;
        }

        int reg_size = wmbus_parser_apator162_register_size(reg);
        if(reg_size < 0 || pos + (size_t)reg_size > payload_len) {
            return false;
        }

        if(!have_total && reg_size >= 4 && (reg == 0x10U || reg == 0xA1U)) {
            *total_m3_x1000 = wmbus_parser_apator162_read_u32_le(&payload[pos]);
            have_total = true;
        }

        pos += (size_t)reg_size;
    }

    return have_total;
}

/**
 * Checks whether the record is likely an Apator 162 payload that this parser can handle.
 *
 * Requires short-TPL CI=0x7A and the known Apator 162 identity fields:
 * version 0x05, device type 0x06 or 0x07, and manufacturer "APA" or legacy 0x8614.
 */
static bool wmbus_parser_apator162_probe(const WmBusParserPacketView* packet) {
    if(!packet || !packet->dll || !packet->tpl || !packet->identity || !packet->payload ||
       !packet->payload->application_bytes || packet->payload->application_len == 0U) {
        return false;
    }
    if(packet->tpl->header_type != WmBusTplHeaderShort || packet->tpl->ci_field != 0x7AU) {
        return false;
    }
    if(packet->identity->version != 0x05U ||
       (packet->identity->dev_type != 0x06U && packet->identity->dev_type != 0x07U)) {
        return false;
    }
    if(strcmp(packet->identity->manufacturer, "APA") != 0 &&
       packet->dll->m_field != WMBUS_APATOR162_MFG_OLD) {
        return false;
    }
    return true;
}

/**
 * Populates reusable application records from an Apator 162 payload.
 */
bool wmbus_parser_apator162_parse(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application) {
    if(!wmbus_parser_apator162_probe(packet) || !out_application) {
        return false;
    }

    size_t fields_pos = 0U;
    if(!wmbus_parser_apator162_locate_fields(
           packet->payload->application_bytes, packet->payload->application_len, &fields_pos)) {
        return false;
    }

    uint32_t total_m3_x1000 = 0U;
    if(!wmbus_parser_apator162_scan_stream(
           packet->payload->application_bytes,
           packet->payload->application_len,
           fields_pos,
           &total_m3_x1000)) {
        return false;
    }

    const size_t status_pos = fields_pos - WMBUS_APATOR162_STATUS_LEN;
    wmbus_parser_apator162_store_status_record(
        out_application,
        &packet->payload->application_bytes[status_pos],
        WMBUS_APATOR162_STATUS_LEN);
    wmbus_parser_apator162_store_total_record(out_application, total_m3_x1000);

    return true;
}
