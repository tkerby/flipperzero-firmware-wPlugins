#include "wmbus_parser_qwaterv2.h"

#include "../model/wmbus_application_record.h"

#define WMBUS_QWATERV2_MFG_QDS                  0x4493U
#define WMBUS_QWATERV2_WALKBY_DATA_LEN          53U
#define WMBUS_QWATERV2_TOTAL_OFFSET              12U
#define WMBUS_QWATERV2_TARGET_YEAR_DATE_OFFSET   16U
#define WMBUS_QWATERV2_TARGET_YEAR_OFFSET        18U
#define WMBUS_QWATERV2_TARGET_DATE_OFFSET        22U
#define WMBUS_QWATERV2_TARGET_OFFSET             24U

static bool wmbus_parser_qwaterv2_decode_bcd4(const uint8_t* data, uint32_t* value) {
    if(!data || !value) return false;

    uint32_t result = 0U;
    uint32_t factor = 1U;
    for(uint8_t i = 0U; i < 4U; i++) {
        const uint8_t lo = data[i] & 0x0FU;
        const uint8_t hi = (data[i] >> 4U) & 0x0FU;
        if(lo > 9U || hi > 9U) return false;
        result += (uint32_t)lo * factor;
        factor *= 10U;
        result += (uint32_t)hi * factor;
        factor *= 10U;
    }

    *value = result;
    return true;
}

static bool wmbus_parser_qwaterv2_decode_date(
    const uint8_t* data,
    WmBusApplicationDateTime* date,
    bool* unavailable) {
    if(!data || !date || !unavailable) return false;

    *unavailable = data[0] == 0xFFU && data[1] == 0xFFU;
    if(*unavailable) return true;

    const uint8_t day = data[0] & 0x1FU;
    const uint8_t month = data[1] & 0x0FU;
    const uint16_t year =
        (uint16_t)(((data[0] >> 5U) & 0x07U) | ((data[1] >> 1U) & 0x78U)) + 2000U;
    if(day == 0U || day > 31U || month == 0U || month > 12U) return false;

    *date = (WmBusApplicationDateTime){
        .year = year,
        .month = month,
        .day = day,
        .precision = WmBusApplicationDateTimePrecisionDate,
    };
    return true;
}

static bool wmbus_parser_qwaterv2_store_volume(
    WmBusPacketApplicationData* application,
    uint64_t storage_no,
    uint32_t volume_l) {
    WmBusApplicationRecord* record = NULL;
    if(!wmbus_application_record_append(application, &record)) return false;

    record->quantity = WmBusApplicationQuantityVolume;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    record->storage_no = storage_no;
    record->scale10 = -3;
    record->data_len = 4U;
    wmbus_application_record_set_value(record, volume_l);
    return true;
}

static bool wmbus_parser_qwaterv2_store_date(
    WmBusPacketApplicationData* application,
    uint64_t storage_no,
    const WmBusApplicationDateTime* date,
    bool unavailable) {
    WmBusApplicationRecord* record = NULL;
    if(!application || !date || !wmbus_application_record_append(application, &record)) {
        return false;
    }

    record->quantity = WmBusApplicationQuantityDate;
    record->measurement_type = WmBusApplicationMeasurementTypeInstantaneous;
    record->storage_no = storage_no;
    record->data_len = 2U;
    if(unavailable) {
        record->value_type = WmBusApplicationValueUnavailable;
    } else {
        record->value_type = WmBusApplicationValueDateTime;
        record->value_datetime = *date;
    }
    return true;
}

static bool wmbus_parser_qwaterv2_probe(const WmBusParserPacketView* packet) {
    if(!packet || !packet->dll || !packet->tpl || !packet->identity || !packet->payload ||
       !packet->payload->application_bytes || packet->payload->application_len == 0U) {
        return false;
    }

    /* CSV identity columns for QWater WDS are version 33/35, device_type 37, CI 78. */
    return packet->dll->m_field == WMBUS_QWATERV2_MFG_QDS &&
           (packet->identity->version == 0x33U || packet->identity->version == 0x35U) &&
           packet->identity->dev_type == 0x37U && packet->tpl->ci_field == 0x78U;
}

static const uint8_t* wmbus_parser_qwaterv2_find_walkby_data(
    const uint8_t* payload,
    size_t payload_len) {
    static const uint8_t record_header[] = {0x0DU, 0xFFU, 0x5FU, 0x35U};
    if(!payload || payload_len < sizeof(record_header) + WMBUS_QWATERV2_WALKBY_DATA_LEN) {
        return NULL;
    }

    for(size_t pos = 0U;
        pos + sizeof(record_header) + WMBUS_QWATERV2_WALKBY_DATA_LEN <= payload_len;
        pos++) {
        if(payload[pos] != record_header[0] || payload[pos + 1U] != record_header[1] ||
           payload[pos + 2U] != record_header[2] || payload[pos + 3U] != record_header[3]) {
            continue;
        }

        const uint8_t* data = &payload[pos + sizeof(record_header)];
        if(data[0] == 0x00U && data[1] == 0x82U && data[3] == 0x00U && data[4] == 0x00U &&
           data[7] == 0x07U && data[8] == 0xC1U && data[9] == 0x13U) {
            return data;
        }
    }

    return NULL;
}

bool wmbus_parser_qwaterv2_parse(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application) {
    if(!out_application || !wmbus_parser_qwaterv2_probe(packet)) return false;

    const uint8_t* data = wmbus_parser_qwaterv2_find_walkby_data(
        packet->payload->application_bytes, packet->payload->application_len);
    if(!data) return false;

    uint32_t total_l = 0U;
    uint32_t target_year_l = 0U;
    uint32_t target_l = 0U;
    WmBusApplicationDateTime target_year_date = {0};
    WmBusApplicationDateTime target_date = {0};
    bool target_year_date_unavailable = false;
    bool target_date_unavailable = false;
    if(!wmbus_parser_qwaterv2_decode_bcd4(&data[WMBUS_QWATERV2_TOTAL_OFFSET], &total_l) ||
       !wmbus_parser_qwaterv2_decode_date(
           &data[WMBUS_QWATERV2_TARGET_YEAR_DATE_OFFSET],
           &target_year_date,
           &target_year_date_unavailable) ||
       !wmbus_parser_qwaterv2_decode_bcd4(
           &data[WMBUS_QWATERV2_TARGET_YEAR_OFFSET], &target_year_l) ||
       !wmbus_parser_qwaterv2_decode_date(
           &data[WMBUS_QWATERV2_TARGET_DATE_OFFSET],
           &target_date,
           &target_date_unavailable) ||
       !wmbus_parser_qwaterv2_decode_bcd4(&data[WMBUS_QWATERV2_TARGET_OFFSET], &target_l)) {
        return false;
    }

    if(!wmbus_parser_qwaterv2_store_volume(out_application, 0U, total_l) ||
       !wmbus_parser_qwaterv2_store_volume(out_application, 1U, target_year_l) ||
       !wmbus_parser_qwaterv2_store_date(
           out_application, 1U, &target_year_date, target_year_date_unavailable) ||
       !wmbus_parser_qwaterv2_store_volume(out_application, 17U, target_l) ||
       !wmbus_parser_qwaterv2_store_date(
           out_application, 17U, &target_date, target_date_unavailable)) {
        return false;
    }

    return true;
}
