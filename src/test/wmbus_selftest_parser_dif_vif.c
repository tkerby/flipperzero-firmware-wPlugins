#include "wmbus_selftest_i.h"

#include "../protocol/format/wmbus_record_formatter.h"
#include "../protocol/parser/wmbus_parser_dif_vif.h"

#include <string.h>

static bool wmbus_selftest_check_parser_itw_type_i_datetime(char* detail, size_t detail_len) {
    static const char* frame_hex =
        "2E449726120200520D087A8A0000A00B6E000000066D281B474B38004B6E010000426C3F3C067F2C"
        "007529070C92A3";
    static const char* expected_fields = "HCA=0;DateTime=2026-08-11 07:27:40;Data=13224899837996;"
                                         "HCA[S1]=1;Date[S1]=2025-12-31";
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0U;
    WmBusPacketRecord record = {0};
    FuriString* fields = furi_string_alloc();
    if(!fields) {
        wmbus_selftest_set_detail(detail, detail_len, "alloc failed");
        return false;
    }

    bool built = wmbus_selftest_hex_to_bytes(frame_hex, frame, sizeof(frame), &frame_len) &&
                 frame_len == 47U;
    bool processed = built &&
                     wmbus_selftest_process_phy_frame_record(
                         WmBusRxModeC, WmBusFrameFormatB, frame, frame_len, NULL, &record);
    bool formatted = processed &&
                     wmbus_record_formatter_format_joined(
                         record.application.records, record.application.record_count, ';', fields);
    bool valid =
        formatted && record.quality == WmBusPacketQualityParsed && record.normalized_len == 45U &&
        strcmp(record.identity.manufacturer, "ITW") == 0 &&
        strcmp(record.identity.meter_id, "52000212") == 0 &&
        record.application.parser_id == WmBusParserIdDifVif &&
        record.application.record_count == 5U && furi_string_equal_str(fields, expected_fields);
    if(!valid) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "built=%u processed=%u quality=%u parser=%s records=%u fields=%s",
            built ? 1U : 0U,
            processed ? 1U : 0U,
            (unsigned int)record.quality,
            wmbus_selftest_parser_name(&record),
            (unsigned int)record.application.record_count,
            furi_string_get_cstr(fields));
        furi_string_free(fields);
        return false;
    }

    furi_string_free(fields);
    wmbus_selftest_set_detail(detail, detail_len, "ITW/52000212 Type I decoded with seconds");
    return true;
}

static bool wmbus_selftest_check_dif_vif_decode_basic(char* detail, size_t detail_len) {
    static const uint8_t payload[] = {
        0x04, 0x13, 0x40, 0xE2, 0x01, 0x00, 0x0C, 0x03, 0x56, 0x34, 0x12, 0x00, 0x84, 0x01,
        0x80, 0x13, 0x78, 0x56, 0x34, 0x12, 0x04, 0x38, 0x4E, 0x61, 0xBC, 0x00, 0x02, 0x5A,
        0xD7, 0x00, 0x02, 0x5E, 0xA0, 0x00, 0x02, 0x62, 0x37, 0x00, 0x02, 0x6C, 0x2C, 0x33,
        0x04, 0x6D, 0x25, 0x10, 0x2C, 0x33, 0x02, 0xFD, 0x17, 0x34, 0x12,
    };
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX] = {0};
    uint8_t count = 0U;
    FuriString* fields = furi_string_alloc();
    if(!fields) {
        wmbus_selftest_set_detail(detail, detail_len, "alloc failed");
        return false;
    }

    if(!wmbus_packet_decode_application_records(
           payload, sizeof(payload), records, COUNT_OF(records), &count)) {
        wmbus_selftest_set_detail(detail, detail_len, "decode failed");
        furi_string_free(fields);
        return false;
    }

    wmbus_record_formatter_format_joined(records, count, ';', fields);
    const char* fields_text = furi_string_get_cstr(fields);

    if(count != 10U || records[0].quantity != WmBusApplicationQuantityVolume ||
       records[0].value_type != WmBusApplicationValueScalar || records[0].value != 123456 ||
       !strstr(fields_text, "Volume=123.456 m3") ||
       records[1].quantity != WmBusApplicationQuantityEnergy ||
       records[1].value_type != WmBusApplicationValueScalar || records[1].value != 123456 ||
       !strstr(fields_text, "Energy=123456 Wh") || records[2].storage_no != 2U ||
       records[3].quantity != WmBusApplicationQuantityVolumeFlow ||
       !strstr(fields_text, "Flow=12.345678 m3/h") ||
       records[4].quantity != WmBusApplicationQuantityFlowTemperature ||
       records[4].value_type != WmBusApplicationValueScalar || records[4].value != 215 ||
       records[4].scale10 != -1 ||
       records[5].quantity != WmBusApplicationQuantityReturnTemperature ||
       records[5].value_type != WmBusApplicationValueScalar || records[5].value != 160 ||
       records[5].scale10 != -1 ||
       records[6].quantity != WmBusApplicationQuantityTemperatureDifference ||
       records[6].value_type != WmBusApplicationValueScalar || records[6].value != 55 ||
       records[6].scale10 != -1 || records[7].quantity != WmBusApplicationQuantityDate ||
       records[7].value_type != WmBusApplicationValueDateTime ||
       records[7].value_datetime.year != 2025U || records[7].value_datetime.month != 3U ||
       records[7].value_datetime.day != 12U ||
       records[7].value_datetime.precision != WmBusApplicationDateTimePrecisionDate ||
       !strstr(fields_text, "Date=2025-03-12") ||
       records[8].quantity != WmBusApplicationQuantityDateTime ||
       records[8].value_type != WmBusApplicationValueDateTime ||
       records[8].value_datetime.year != 2025U || records[8].value_datetime.month != 3U ||
       records[8].value_datetime.day != 12U ||
       records[8].value_datetime.precision != WmBusApplicationDateTimePrecisionMinute ||
       records[8].value_datetime.hour != 16U || records[8].value_datetime.minute != 37U ||
       !strstr(fields_text, "DateTime=2025-03-12 16:37") ||
       records[9].quantity != WmBusApplicationQuantityStatus ||
       records[9].value_type != WmBusApplicationValueRaw || !strstr(fields_text, "Status=3412")) {
        wmbus_selftest_set_detail(
            detail, detail_len, "count=%u fields=%s", (unsigned int)count, fields_text);
        furi_string_free(fields);
        return false;
    }

    furi_string_free(fields);
    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "records=%u common_types=volume,energy,flow,temp,date,status",
        (unsigned int)count);
    return true;
}

static bool wmbus_selftest_check_dif_vif_datetime_type_i(char* detail, size_t detail_len) {
    static const uint8_t payload[] = {0x06U, 0x6DU, 0x28U, 0x1BU, 0x47U, 0x4BU, 0x38U, 0x00U};
    WmBusApplicationRecord records[1] = {0};
    uint8_t count = 0U;
    FuriString* fields = furi_string_alloc();
    if(!fields) {
        wmbus_selftest_set_detail(detail, detail_len, "alloc failed");
        return false;
    }

    bool decoded = wmbus_packet_decode_application_records(
        payload, sizeof(payload), records, COUNT_OF(records), &count);
    bool formatted = decoded && wmbus_record_formatter_format_joined(records, count, ';', fields);
    const WmBusApplicationDateTime* date_time = &records[0].value_datetime;
    bool valid = formatted && count == 1U &&
                 records[0].quantity == WmBusApplicationQuantityDateTime &&
                 records[0].value_type == WmBusApplicationValueDateTime &&
                 date_time->precision == WmBusApplicationDateTimePrecisionSecond &&
                 date_time->year == 2026U && date_time->month == 8U && date_time->day == 11U &&
                 date_time->hour == 7U && date_time->minute == 27U && date_time->second == 40U &&
                 furi_string_equal_str(fields, "DateTime=2026-08-11 07:27:40");
    if(!valid) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "decoded=%u count=%u value_type=%u fields=%s",
            decoded ? 1U : 0U,
            (unsigned int)count,
            (unsigned int)records[0].value_type,
            furi_string_get_cstr(fields));
        furi_string_free(fields);
        return false;
    }

    furi_string_free(fields);
    wmbus_selftest_set_detail(detail, detail_len, "Type I=2026-08-11 07:27:40");
    return true;
}

static bool wmbus_selftest_check_dif_vif_datetime_type_i_invalid(char* detail, size_t detail_len) {
    static const struct {
        const char* name;
        uint8_t payload[8];
        size_t payload_len;
    } cases[] = {
        {"invalid flag", {0x06U, 0x6DU, 0x28U, 0x9BU, 0x47U, 0x4BU, 0x38U, 0x00U}, 8U},
        {"second", {0x06U, 0x6DU, 0x3CU, 0x1BU, 0x47U, 0x4BU, 0x38U, 0x00U}, 8U},
        {"minute", {0x06U, 0x6DU, 0x28U, 0x3CU, 0x47U, 0x4BU, 0x38U, 0x00U}, 8U},
        {"hour", {0x06U, 0x6DU, 0x28U, 0x1BU, 0x58U, 0x4BU, 0x38U, 0x00U}, 8U},
        {"day", {0x06U, 0x6DU, 0x28U, 0x1BU, 0x47U, 0x40U, 0x38U, 0x00U}, 8U},
        {"month", {0x06U, 0x6DU, 0x28U, 0x1BU, 0x47U, 0x4BU, 0x30U, 0x00U}, 8U},
        {"reserved year", {0x06U, 0x6DU, 0x28U, 0x1BU, 0x47U, 0x8BU, 0xC8U, 0x00U}, 8U},
        {"unspecified year", {0x06U, 0x6DU, 0x28U, 0x1BU, 0x47U, 0xEBU, 0xF8U, 0x00U}, 8U},
        {"unsupported length", {0x03U, 0x6DU, 0x28U, 0x1BU, 0x47U}, 5U},
    };

    for(size_t i = 0; i < COUNT_OF(cases); i++) {
        WmBusApplicationRecord record = {0};
        uint8_t count = 0U;
        if(!wmbus_packet_decode_application_records(
               cases[i].payload, cases[i].payload_len, &record, 1U, &count) ||
           count != 1U || record.quantity != WmBusApplicationQuantityDateTime ||
           record.value_type != WmBusApplicationValueUnavailable) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "%s accepted count=%u type=%u",
                cases[i].name,
                (unsigned int)count,
                (unsigned int)record.value_type);
            return false;
        }
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "Type I invalid/reserved cases=%u", (unsigned int)COUNT_OF(cases));
    return true;
}

static bool wmbus_selftest_check_dif_vif_signed_integer_widths(char* detail, size_t detail_len) {
    static const uint8_t payload[] = {
        0x01U, 0x5BU, 0x80U, 0x02U, 0x5BU, 0x00U, 0x80U, 0x03U, 0x5BU, 0x00U, 0x00U, 0x80U,
        0x04U, 0x5BU, 0x00U, 0x00U, 0x00U, 0x80U, 0x06U, 0x5BU, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x80U, 0x07U, 0x5BU, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x80U,
    };
    static const uint8_t widths[] = {1U, 2U, 3U, 4U, 6U, 8U};
    static const int64_t expected[] = {
        -INT64_C(128),
        -INT64_C(32768),
        -INT64_C(8388608),
        -INT64_C(2147483648),
        -INT64_C(140737488355328),
        INT64_MIN,
    };
    WmBusApplicationRecord records[COUNT_OF(widths)] = {0};
    uint8_t count = 0U;

    if(!wmbus_packet_decode_application_records(
           payload, sizeof(payload), records, COUNT_OF(records), &count) ||
       count != COUNT_OF(records)) {
        wmbus_selftest_set_detail(detail, detail_len, "decode failed count=%u", count);
        return false;
    }

    for(uint8_t i = 0U; i < count; i++) {
        char value_text[32] = {0};
        char expected_text[32] = {0};
        snprintf(expected_text, sizeof(expected_text), "%lld C", (long long)expected[i]);
        if(records[i].data_len != widths[i] ||
           records[i].value_type != WmBusApplicationValueScalar ||
           records[i].value != expected[i] ||
           !wmbus_record_formatter_format_value_text(&records[i], value_text, sizeof(value_text)) ||
           strcmp(value_text, expected_text) != 0) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "width=%u type=%u value=%lld text=%s",
                widths[i],
                (unsigned int)records[i].value_type,
                (long long)records[i].value,
                value_text);
            return false;
        }
    }

    wmbus_selftest_set_detail(detail, detail_len, "signed widths=1,2,3,4,6,8");
    return true;
}

static bool wmbus_selftest_check_dif_vif_lvar_classes(char* detail, size_t detail_len) {
    static const uint8_t text_payload[] = {
        0x0DU,
        0x13U,
        0x03U,
        'a',
        'b',
        'c',
        0x01U,
        0x13U,
        0x01U,
    };
    WmBusApplicationRecord records[2] = {0};
    uint8_t count = 0U;

    if(!wmbus_packet_decode_application_records(
           text_payload, sizeof(text_payload), records, COUNT_OF(records), &count) ||
       count != 2U || records[0].data_len != 3U ||
       records[0].value_type != WmBusApplicationValueRaw ||
       records[1].value_type != WmBusApplicationValueScalar || records[1].value != 1) {
        wmbus_selftest_set_detail(detail, detail_len, "text LVAR decode failed count=%u", count);
        return false;
    }

    static const uint8_t numeric_payload[] = {
        0x0DU, 0x13U, 0xC2U, 0x34U, 0x12U, 0x0DU, 0x13U, 0xD2U, 0x34U, 0x12U,
        0x0DU, 0x13U, 0xE3U, 0xFEU, 0xFFU, 0xFFU, 0x0DU, 0x13U, 0xC9U, 0x99U,
        0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U, 0x99U,
    };
    WmBusApplicationRecord numeric_records[4] = {0};
    if(!wmbus_packet_decode_application_records(
           numeric_payload,
           sizeof(numeric_payload),
           numeric_records,
           COUNT_OF(numeric_records),
           &count) ||
       count != COUNT_OF(numeric_records) || numeric_records[0].value != 1234 ||
       numeric_records[1].value != -1234 || numeric_records[2].value != -2 ||
       numeric_records[3].value != INT64_C(999999999999999999)) {
        wmbus_selftest_set_detail(
            detail, detail_len, "numeric LVAR decode failed count=%u", count);
        return false;
    }

    static const uint8_t large_binary_payload[] = {
        0x0DU, 0x13U, 0xF0U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U,
        0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x00U, 0x01U, 0x13U, 0x2AU,
    };
    if(!wmbus_packet_decode_application_records(
           large_binary_payload, sizeof(large_binary_payload), records, COUNT_OF(records), &count) ||
       count != 2U || records[0].data_len != 16U ||
       records[0].value_type != WmBusApplicationValueRaw || records[1].value != 42) {
        wmbus_selftest_set_detail(detail, detail_len, "large LVAR framing failed count=%u", count);
        return false;
    }

    for(uint16_t lvar = 0xCAU; lvar <= 0xFFU; lvar++) {
        if((lvar >= 0xD0U && lvar <= 0xD9U) || (lvar >= 0xE0U && lvar <= 0xEFU) ||
           (lvar >= 0xF0U && lvar <= 0xF6U)) {
            continue;
        }
        uint8_t reserved_payload[3] = {0x0DU, 0x13U, (uint8_t)lvar};
        if(wmbus_packet_decode_application_records(
               reserved_payload, sizeof(reserved_payload), records, COUNT_OF(records), &count)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "reserved LVAR=%02X accepted", (unsigned int)lvar);
            return false;
        }
    }

    wmbus_selftest_set_detail(detail, detail_len, "LVAR text/BCD/binary framed to 64 bytes");
    return true;
}

static bool wmbus_selftest_check_dif_vif_extensions(char* detail, size_t detail_len) {
    static const uint8_t payload[] = {
        0x04U,
        0x93U,
        0x00U,
        0x01U,
        0x00U,
        0x00U,
        0x00U,
        0x01U,
        0xFDU,
        0x97U,
        0x00U,
        0xA5U,
        0x02U,
        0xFDU,
        0x46U,
        0xD0U,
        0x0AU,
    };
    WmBusApplicationRecord records[3] = {0};
    uint8_t count = 0U;

    if(!wmbus_packet_decode_application_records(
           payload, sizeof(payload), records, COUNT_OF(records), &count) ||
       count != 3U || records[0].quantity != WmBusApplicationQuantityVolume ||
       records[0].scale10 != -3 || records[0].value != 1 ||
       records[1].quantity != WmBusApplicationQuantityStatus || records[1].data_len != 1U ||
       records[1].value_type != WmBusApplicationValueRaw || records[1].raw_bytes[0] != 0xA5U ||
       records[2].quantity != WmBusApplicationQuantityVoltage || records[2].scale10 != -3 ||
       records[2].value != 2768) {
        wmbus_selftest_set_detail(
            detail, detail_len, "VIF extension decode failed count=%u", count);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "primary, chained, and FD46 voltage VIFs");
    return true;
}

static bool wmbus_selftest_check_dif_vif_qcaloric_version(char* detail, size_t detail_len) {
    /* Application payload from QDS 27200022, version 35, device type 08. */
    static const uint8_t payload[] = {
        0x04U, 0xEDU, 0x39U, 0x20U, 0x08U, 0xB4U, 0x21U, 0x01U, 0xFDU, 0x0CU, 0x10U,
        0x04U, 0x6DU, 0x00U, 0x16U, 0x4AU, 0x38U, 0x02U, 0xFDU, 0x3CU, 0xC2U, 0x01U,
    };
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX] = {0};
    uint8_t count = 0U;
    FuriString* fields = furi_string_alloc();
    if(!fields) {
        wmbus_selftest_set_detail(detail, detail_len, "alloc failed");
        return false;
    }

    bool decoded = wmbus_packet_decode_application_records(
        payload, sizeof(payload), records, COUNT_OF(records), &count);
    bool formatted = decoded && wmbus_record_formatter_format_joined(records, count, ';', fields);
    const char* fields_text = furi_string_get_cstr(fields);
    bool valid = formatted && count == 4U &&
                 records[1].quantity == WmBusApplicationQuantityVersion &&
                 records[1].value_type == WmBusApplicationValueRaw && records[1].data_len == 1U &&
                 records[1].raw_bytes[0] == 0x10U && strstr(fields_text, "Version=10") != NULL &&
                 strstr(fields_text, "DateTime=2026-08-10 22:00") != NULL;
    if(!valid) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "decoded=%u count=%u fields=%s",
            decoded ? 1U : 0U,
            (unsigned int)count,
            fields_text);
        furi_string_free(fields);
        return false;
    }

    furi_string_free(fields);
    wmbus_selftest_set_detail(detail, detail_len, "QDS FD0C Version=10");
    return true;
}

static bool wmbus_selftest_check_dif_vif_plain_text_units(char* detail, size_t detail_len) {
    static const uint8_t payload[] = {
        0x01U,
        0x7CU,
        0x02U,
        'm',
        '3',
        0x2AU,
        0x01U,
        0xFCU,
        0x01U,
        'x',
        0x00U,
        0x2BU,
        0x01U,
        0x13U,
        0x2CU,
    };
    WmBusApplicationRecord records[3] = {0};
    uint8_t count = 0U;

    if(!wmbus_packet_decode_application_records(
           payload, sizeof(payload), records, COUNT_OF(records), &count) ||
       count != 3U || records[0].value != 42 || records[1].value != 43 ||
       records[2].quantity != WmBusApplicationQuantityVolume || records[2].value != 44) {
        wmbus_selftest_set_detail(
            detail, detail_len, "plain-text VIF framing failed count=%u", count);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "VIF=7C/FC unit strings consumed");
    return true;
}

static bool wmbus_selftest_check_dif_vif_hca_records(char* detail, size_t detail_len) {
    static const uint8_t payload[] = {
        0x0BU, 0x6EU, 0x19U, 0x03U, 0x00U, 0x4BU, 0x6EU, 0x06U, 0x12U, 0x00U, 0x42U, 0x6CU,
        0x3FU, 0x3CU, 0xCBU, 0x08U, 0x6EU, 0x19U, 0x03U, 0x00U, 0xC2U, 0x08U, 0x6CU, 0x5FU,
        0x37U, 0x32U, 0x6CU, 0xFFU, 0xFFU, 0x04U, 0x6DU, 0x0DU, 0x17U, 0x45U, 0x38U,
    };
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX] = {0};
    uint8_t count = 0U;
    FuriString* unavailable = furi_string_alloc();
    FuriString* fields = furi_string_alloc();
    if(!unavailable || !fields) {
        wmbus_selftest_set_detail(detail, detail_len, "alloc failed");
        if(unavailable) furi_string_free(unavailable);
        if(fields) furi_string_free(fields);
        return false;
    }

    bool decoded = wmbus_packet_decode_application_records(
        payload, sizeof(payload), records, COUNT_OF(records), &count);
    if(decoded) {
        wmbus_record_formatter_format_joined(records, count, ';', fields);
    }
    bool valid = decoded && count == 7U &&
                 records[0].quantity == WmBusApplicationQuantityHeatCostAllocation &&
                 records[0].value == 319 &&
                 records[1].quantity == WmBusApplicationQuantityHeatCostAllocation &&
                 records[1].storage_no == 1U && records[1].value == 1206 &&
                 records[2].quantity == WmBusApplicationQuantityDate &&
                 records[2].storage_no == 1U && records[2].value_datetime.year == 2025U &&
                 records[2].value_datetime.month == 12U && records[2].value_datetime.day == 31U &&
                 records[3].quantity == WmBusApplicationQuantityHeatCostAllocation &&
                 records[3].storage_no == 17U && records[3].value == 319 &&
                 records[4].quantity == WmBusApplicationQuantityDate &&
                 records[4].storage_no == 17U && records[4].value_datetime.year == 2026U &&
                 records[4].value_datetime.month == 7U && records[4].value_datetime.day == 31U &&
                 records[5].quantity == WmBusApplicationQuantityDate &&
                 records[5].measurement_type == WmBusApplicationMeasurementTypeAtError &&
                 records[5].value_type == WmBusApplicationValueUnavailable &&
                 wmbus_record_formatter_format_joined(&records[5], 1U, ';', unavailable) &&
                 furi_string_equal_str(unavailable, "Date[err]=-") &&
                 records[6].quantity == WmBusApplicationQuantityDateTime &&
                 records[6].value_datetime.year == 2026U &&
                 records[6].value_datetime.month == 8U && records[6].value_datetime.day == 5U &&
                 records[6].value_datetime.hour == 23U &&
                 records[6].value_datetime.minute == 13U &&
                 furi_string_equal_str(
                     fields,
                     "HCA=319;Date[err]=-;DateTime=2026-08-05 23:13;"
                     "HCA[S1]=1206;Date[S1]=2025-12-31;"
                     "HCA[S17]=319;Date[S17]=2026-07-31");
    if(!valid) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "count=%u hca=%lld error_type=%u error=%s",
            (unsigned int)count,
            (long long)records[0].value,
            (unsigned int)records[5].value_type,
            furi_string_get_cstr(unavailable));
        furi_string_free(unavailable);
        furi_string_free(fields);
        return false;
    }

    furi_string_free(unavailable);
    furi_string_free(fields);
    wmbus_selftest_set_detail(
        detail, detail_len, "HCA current/storage history and date contexts retained");
    return true;
}

static bool wmbus_selftest_check_dif_vif_extension_limits(char* detail, size_t detail_len) {
    static const uint8_t max_dife_payload[] = {
        0x81U,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0x7FU,
        0x13U,
        0x01U,
    };
    static const uint8_t too_many_dife_payload[] = {
        0x81U,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0xFFU,
        0x7FU,
        0x13U,
        0x01U,
    };
    static const uint8_t too_many_vife_payload[] = {
        0x01U,
        0x80U,
        0x80U,
        0x80U,
        0x80U,
        0x80U,
        0x80U,
        0x80U,
        0x80U,
        0x80U,
        0x80U,
        0x80U,
        0x00U,
        0x01U,
    };
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX] = {0};
    uint8_t count = 0U;

    if(!wmbus_packet_decode_application_records(
           max_dife_payload, sizeof(max_dife_payload), records, COUNT_OF(records), &count) ||
       count != 1U || records[0].storage_no != UINT64_C(0x1FFFFFFFFFE) ||
       records[0].tariff != UINT32_C(0xFFFFF) || records[0].subunit != UINT16_C(0x3FF) ||
       records[0].quantity != WmBusApplicationQuantityVolume || records[0].value != 1) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "max DIFE decode failed count=%u storage=%llu tariff=%lu subunit=%u",
            (unsigned int)count,
            (unsigned long long)records[0].storage_no,
            (unsigned long)records[0].tariff,
            (unsigned int)records[0].subunit);
        return false;
    }

    if(wmbus_packet_decode_application_records(
           too_many_dife_payload,
           sizeof(too_many_dife_payload),
           records,
           COUNT_OF(records),
           &count) ||
       wmbus_packet_decode_application_records(
           too_many_vife_payload,
           sizeof(too_many_vife_payload),
           records,
           COUNT_OF(records),
           &count)) {
        wmbus_selftest_set_detail(detail, detail_len, "more than 10 extensions accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "10 DIFE preserved; 11 DIFE/VIFE rejected");
    return true;
}

static bool wmbus_selftest_check_dif_vif_decode_reject_malformed(char* detail, size_t detail_len) {
    static const uint8_t malformed[] = {0x04, 0x13, 0x34, 0x12};
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX] = {0};
    uint8_t count = 0U;

    if(wmbus_packet_decode_application_records(
           malformed, sizeof(malformed), records, COUNT_OF(records), &count)) {
        wmbus_selftest_set_detail(
            detail, detail_len, "malformed payload accepted count=%u", (unsigned int)count);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "malformed payload rejected=YES");
    return true;
}

static bool wmbus_selftest_check_dif_vif_decode_12_digit_bcd(char* detail, size_t detail_len) {
    /* The first record mirrors the real MAD telegram: DIF 0E is standard
     * 12-digit BCD, while FF makes its value unavailable. A bad BCD value must
     * remain raw without preventing the following standard volume record. */
    static const uint8_t payload[] = {
        0x0E,
        0x78,
        0x55,
        0x83,
        0x00,
        0x24,
        0x05,
        0xFF,
        0x04,
        0x13,
        0x50,
        0x71,
        0x00,
        0x00,
    };
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX] = {0};
    uint8_t count = 0U;

    if(!wmbus_packet_decode_application_records(
           payload, sizeof(payload), records, COUNT_OF(records), &count) ||
       count != 2U || records[0].dif != 0x0EU || records[0].data_len != 6U ||
       records[0].value_type != WmBusApplicationValueRaw ||
       records[1].quantity != WmBusApplicationQuantityVolume ||
       records[1].value_type != WmBusApplicationValueScalar || records[1].value != 29008 ||
       records[1].scale10 != -3) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "decode/count mismatch count=%u first_type=%u total=%lld",
            (unsigned int)count,
            (unsigned int)records[0].value_type,
            (long long)records[1].value);
        return false;
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "DIF=0E raw invalid BCD; following volume=29008");
    return true;
}

static bool check_dif_vif_validates_unstored_tail(char* detail, size_t detail_len) {
    uint8_t payload[WMBUS_PACKET_RECORD_MAX * 3U + 1U];
    for(size_t i = 0; i < WMBUS_PACKET_RECORD_MAX; i++) {
        payload[i * 3U] = 0x01;
        payload[i * 3U + 1U] = 0x13;
        payload[i * 3U + 2U] = i;
    }
    payload[sizeof(payload) - 1U] = 0x04;
    WmBusApplicationRecord records[WMBUS_PACKET_RECORD_MAX];
    uint8_t count;
    if(wmbus_packet_decode_application_records(
           payload, sizeof(payload), records, COUNT_OF(records), &count) ||
       count != 0U)
        return false;
    static const uint8_t invalid_time[] = {0x04, 0x6D, 0x80, 0x12, 0x21, 0x31};
    if(!wmbus_packet_decode_application_records(
           invalid_time, sizeof(invalid_time), records, COUNT_OF(records), &count) ||
       count != 1U || records[0].value_type != WmBusApplicationValueUnavailable)
        return false;
    wmbus_selftest_set_detail(
        detail, detail_len, "Malformed tail and invalid Type F time rejected");
    return true;
}

static bool check_dif_vif_unsupported_modifiers(char* detail, size_t detail_len) {
    /* Unit modifiers and record errors must not become ordinary readings.
     * A later supported record must still decode at its declared boundary. */
    static const uint8_t normalized[] = {
        0x1B, 0x44, 0x93, 0x44, 0x78, 0x56, 0x34, 0x12, 0x01, 0x07, 0x78, 0x01, 0x93, 0x3B,
        0x01, 0x01, 0x93, 0x15, 0x02, 0x02, 0xFD, 0xC6, 0x3B, 0xD0, 0x0A, 0x01, 0x13, 0x2A,
    };
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX];
    size_t frame_len;
    WmBusPacketRecord packet;
    if(!wmbus_selftest_frame_build_format_a(
           normalized, sizeof(normalized), frame, sizeof(frame), &frame_len) ||
       !wmbus_selftest_process_phy_frame_record(
           WmBusRxModeC, WmBusFrameFormatA, frame, frame_len, NULL, &packet) ||
       packet.quality != WmBusPacketQualityParsed || packet.application.record_count != 4U)
        return false;
    for(size_t i = 0; i < 3U; i++) {
        if(packet.application.records[i].quantity != WmBusApplicationQuantityUnknown ||
           packet.application.records[i].value_type != WmBusApplicationValueRaw)
            return false;
    }
    const WmBusApplicationRecord* volume = &packet.application.records[3];
    if(volume->quantity != WmBusApplicationQuantityVolume || volume->value != 42 ||
       volume->scale10 != -3)
        return false;
    wmbus_selftest_set_detail(
        detail, detail_len, "Unsupported VIFEs retained raw; following volume decoded");
    return true;
}

static const WmBusSelftestCheck wmbus_selftest_checks_parser_dif_vif[] = {
    {"check_dif_vif_unsupported_modifiers", check_dif_vif_unsupported_modifiers},
    {"check_dif_vif_validates_unstored_tail", check_dif_vif_validates_unstored_tail},

    {"check_parser_itw_type_i_datetime", wmbus_selftest_check_parser_itw_type_i_datetime},
    {"check_dif_vif_decode_basic", wmbus_selftest_check_dif_vif_decode_basic},
    {"check_dif_vif_datetime_type_i", wmbus_selftest_check_dif_vif_datetime_type_i},
    {"check_dif_vif_datetime_type_i_invalid",
     wmbus_selftest_check_dif_vif_datetime_type_i_invalid},
    {"check_dif_vif_signed_integer_widths", wmbus_selftest_check_dif_vif_signed_integer_widths},
    {"check_dif_vif_lvar_classes", wmbus_selftest_check_dif_vif_lvar_classes},
    {"check_dif_vif_extensions", wmbus_selftest_check_dif_vif_extensions},
    {"check_dif_vif_qcaloric_version", wmbus_selftest_check_dif_vif_qcaloric_version},
    {"check_dif_vif_plain_text_units", wmbus_selftest_check_dif_vif_plain_text_units},
    {"check_dif_vif_hca_records", wmbus_selftest_check_dif_vif_hca_records},
    {"check_dif_vif_extension_limits", wmbus_selftest_check_dif_vif_extension_limits},
    {"check_dif_vif_decode_12_digit_bcd", wmbus_selftest_check_dif_vif_decode_12_digit_bcd},
    {"check_dif_vif_decode_reject_malformed",
     wmbus_selftest_check_dif_vif_decode_reject_malformed},
};

const WmBusSelftestCheck* wmbus_selftest_parser_dif_vif_checks(size_t* count) {
    if(count) *count = COUNT_OF(wmbus_selftest_checks_parser_dif_vif);
    return wmbus_selftest_checks_parser_dif_vif;
}
