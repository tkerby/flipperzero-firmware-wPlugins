#include "wmbus_selftest_i.h"

#include "../protocol/format/wmbus_record_formatter.h"

#include <string.h>

static bool wmbus_selftest_check_parser_qwaterv2_walkby(char* detail, size_t detail_len) {
    static const char* frame_hex =
        "5a449344965643353337637e78077996564335934433070dff5f3500301c82020000100007c1"
        "13ffff003900003feefb3c003900005f370039000000000000004c1e00000000000000000000"
        "000000000000ffff0000002f046d25114b380cfd109000819abc2414f0";
    static const char* expected_fields = "Volume=3.900 m3;Volume[S1]=3.900 m3;Date[S1]=2025-12-31;"
                                         "Volume[S17]=3.900 m3;Date[S17]=2026-07-31";
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0U;
    WmBusPacketRecord packet = {0};
    FuriString* fields = furi_string_alloc();
    if(!fields) {
        wmbus_selftest_set_detail(detail, detail_len, "alloc failed");
        return false;
    }

    const bool built = wmbus_selftest_hex_to_bytes(frame_hex, frame, sizeof(frame), &frame_len);
    const bool processed = built && frame_len == 105U &&
                           wmbus_selftest_process_phy_frame_record(
                               WmBusRxModeC, WmBusFrameFormatA, frame, frame_len, NULL, &packet);
    const bool formatted =
        processed && wmbus_record_formatter_format_joined(
                         packet.application.records, packet.application.record_count, ';', fields);
    const WmBusApplicationRecord* records = packet.application.records;
    const bool valid =
        formatted && packet.quality == WmBusPacketQualityParsed && packet.normalized_len == 91U &&
        packet.application.parser_id == WmBusParserIdQWaterV2 &&
        packet.application.record_count == 5U &&
        strcmp(packet.identity.manufacturer, "QDS") == 0 &&
        strcmp(packet.identity.meter_id, "35435696") == 0 &&
        records[0].quantity == WmBusApplicationQuantityVolume && records[0].storage_no == 0U &&
        records[0].value == 3900 && records[0].scale10 == -3 &&
        records[1].quantity == WmBusApplicationQuantityVolume && records[1].storage_no == 1U &&
        records[1].value == 3900 && records[2].quantity == WmBusApplicationQuantityDate &&
        records[2].storage_no == 1U && records[2].value_datetime.year == 2025U &&
        records[2].value_datetime.month == 12U && records[2].value_datetime.day == 31U &&
        records[3].quantity == WmBusApplicationQuantityVolume && records[3].storage_no == 17U &&
        records[3].value == 3900 && records[4].quantity == WmBusApplicationQuantityDate &&
        records[4].storage_no == 17U && records[4].value_datetime.year == 2026U &&
        records[4].value_datetime.month == 7U && records[4].value_datetime.day == 31U &&
        furi_string_equal_str(fields, expected_fields);
    if(!valid) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "built=%u processed=%u q=%u parser=%s records=%u fields=%s",
            built ? 1U : 0U,
            processed ? 1U : 0U,
            (unsigned int)packet.quality,
            wmbus_selftest_parser_name(&packet),
            (unsigned int)packet.application.record_count,
            furi_string_get_cstr(fields));
        furi_string_free(fields);
        return false;
    }

    furi_string_free(fields);
    wmbus_selftest_set_detail(detail, detail_len, "QDS 35435696 records=5 total=3.900 m3");
    return true;
}

static bool wmbus_selftest_check_parser_qwaterv2_scope(char* detail, size_t detail_len) {
    static const char* standard_frame_hex =
        "3C449344965643353337879C729656433593443307030000200C1300A33F3900004C13003900"
        "00426C3F3CCC0813F5C900390000C2086C5F37326CFFFF046D25353B114B38D3E1";
    static const char* non_water_wds_frame_hex =
        "494493448520512735081458780DFF5F350082A10000820007B06EFFB684FF000000003F3C00"
        "0000005F37000000DB3400000000000000000000000000000000FFFF0000000000000000002F"
        "046D00064B38AE61";
    const char* frames[] = {standard_frame_hex, non_water_wds_frame_hex};

    for(size_t i = 0U; i < COUNT_OF(frames); i++) {
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t frame_len = 0U;
        WmBusPacketRecord packet = {0};
        if(!wmbus_selftest_hex_to_bytes(frames[i], frame, sizeof(frame), &frame_len) ||
           !wmbus_selftest_process_phy_frame_record(
               WmBusRxModeC, WmBusFrameFormatA, frame, frame_len, NULL, &packet) ||
           packet.application.parser_id != WmBusParserIdDifVif) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "case=%u parser=%s ci=%02X ver=%02X type=%02X",
                (unsigned int)i,
                wmbus_selftest_parser_name(&packet),
                packet.tpl.ci_field,
                packet.identity.version,
                packet.identity.dev_type);
            return false;
        }

        if(i == 0U) {
            uint32_t total_m3_x1000 = 0U;
            if(packet.tpl.ci_field != 0x72U || packet.identity.version != 0x33U ||
               packet.identity.dev_type != 0x07U || packet.application.record_count != 7U ||
               !wmbus_selftest_find_total_volume(&packet, &total_m3_x1000) ||
               total_m3_x1000 != 3900U) {
                wmbus_selftest_set_detail(
                    detail,
                    detail_len,
                    "standard ci=%02X ver=%02X type=%02X records=%u total=%lu",
                    packet.tpl.ci_field,
                    packet.identity.version,
                    packet.identity.dev_type,
                    (unsigned int)packet.application.record_count,
                    (unsigned long)total_m3_x1000);
                return false;
            }
        } else if(
            packet.tpl.ci_field != 0x78U || packet.identity.version != 0x35U ||
            packet.identity.dev_type != 0x08U) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "non-water ci=%02X ver=%02X type=%02X",
                packet.tpl.ci_field,
                packet.identity.version,
                packet.identity.dev_type);
            return false;
        }
    }

    wmbus_selftest_set_detail(detail, detail_len, "CI72 water and CI78 type08 remain DifVif");
    return true;
}

static bool check_qwater_record_boundaries(char* detail, size_t detail_len) {
    /* A valid WDS record, then the same signature hidden inside another record. */
    uint8_t wds[57] = {0x0D, 0xFF, 0x5F, 0x35, 0x00, 0x82};
    wds[11] = 0x07;
    wds[12] = 0xC1;
    wds[13] = 0x13;
    wds[20] = wds[21] = wds[26] = wds[27] = 0xFF;
    for(unsigned variant = 0; variant < 5; variant++) {
        uint8_t normalized[100] = {0, 0x44, 0x93, 0x44, 1, 2, 3, 4, 0x33, 0x37, 0x78};
        size_t pos = 11;
        if(variant == 1) { // filler is valid at record boundaries
            normalized[pos++] = 0x2F;
        } else if(variant == 2) { // signature inside a variable text value
            normalized[pos++] = 0x0D;
            normalized[pos++] = 0x7C;
            normalized[pos++] = 0;
            normalized[pos++] = sizeof(wds);
        } else if(variant == 3) { // invalid prefix, never resynchronize
            normalized[pos++] = 0x08;
        }
        memcpy(&normalized[pos], wds, sizeof(wds));
        pos += sizeof(wds);
        if(variant == 4) normalized[pos++] = 0x04; // malformed trailing record
        normalized[0] = pos - 1U;
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX];
        size_t len = 0;
        WmBusPacketRecord packet;
        bool expected = variant < 2;
        if(!wmbus_selftest_frame_build_format_a(normalized, pos, frame, sizeof(frame), &len) ||
           !wmbus_selftest_process_phy_frame_record(
               WmBusRxModeC, WmBusFrameFormatA, frame, len, NULL, &packet) ||
           (packet.application.parser_id == WmBusParserIdQWaterV2) != expected) {
            wmbus_selftest_set_detail(
                detail, detail_len, "variant=%u expected QWater=%u", variant, expected);
            return false;
        }
    }
    wmbus_selftest_set_detail(detail, detail_len, "WDS accepted only on valid record boundaries");
    return true;
}

static const WmBusSelftestCheck wmbus_selftest_checks_parser_qwaterv2[] = {
    {"check_qwater_record_boundaries", check_qwater_record_boundaries},

    {"check_parser_qwaterv2_walkby", wmbus_selftest_check_parser_qwaterv2_walkby},
    {"check_parser_qwaterv2_scope", wmbus_selftest_check_parser_qwaterv2_scope},
};

const WmBusSelftestCheck* wmbus_selftest_parser_qwaterv2_checks(size_t* count) {
    if(count) *count = COUNT_OF(wmbus_selftest_checks_parser_qwaterv2);
    return wmbus_selftest_checks_parser_qwaterv2;
}
