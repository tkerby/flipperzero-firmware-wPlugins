#include "wmbus_parser_qwaterv2.h"
#include "wmbus_parser_dif_vif.h"
#include "../decode/wmbus_value.h"

#include "../model/wmbus_application_record.h"

#define WMBUS_QWATERV2_MFG_QDS                 0x4493U
#define WMBUS_QWATERV2_WALKBY_DATA_LEN         53U
#define WMBUS_QWATERV2_TOTAL_OFFSET            12U
#define WMBUS_QWATERV2_TARGET_YEAR_DATE_OFFSET 16U
#define WMBUS_QWATERV2_TARGET_YEAR_OFFSET      18U
#define WMBUS_QWATERV2_TARGET_DATE_OFFSET      22U
#define WMBUS_QWATERV2_TARGET_OFFSET           24U

static bool wmbus_parser_qwaterv2_store_volume(
    WmBusPacketApplicationData* application,
    uint64_t storage_no,
    uint64_t volume_l) {
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

static const uint8_t*
    wmbus_parser_qwaterv2_find_walkby_data(const uint8_t* payload, size_t payload_len) {
    size_t pos = 0U;
    const uint8_t* walkby = NULL;
    WmBusDataRecord data;
    WmBusDataRecordStatus status;
    while((status = wmbus_dif_vif_next_record(payload, payload_len, &pos, &data)) ==
          WmBusDataRecordOk) {
        if(data.record.dif != 0x0DU || data.record.vif != 0xFFU || data.vife_count != 1U ||
           data.first_vife != 0x5FU || data.record.data_len != WMBUS_QWATERV2_WALKBY_DATA_LEN)
            continue;
        const uint8_t* bytes = data.data;
        if(walkby || bytes[0] != 0x00U || bytes[1] != 0x82U || bytes[3] != 0x00U ||
           bytes[4] != 0x00U || bytes[7] != 0x07U || bytes[8] != 0xC1U || bytes[9] != 0x13U) {
            return NULL;
        }
        walkby = bytes;
    }
    return status == WmBusDataRecordEnd ? walkby : NULL;
}

bool wmbus_parser_qwaterv2_parse(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application) {
    if(!out_application || !wmbus_parser_qwaterv2_probe(packet)) return false;

    const uint8_t* data = wmbus_parser_qwaterv2_find_walkby_data(
        packet->payload->application_bytes, packet->payload->application_len);
    if(!data) return false;

    uint64_t total_l = 0U;
    uint64_t target_year_l = 0U;
    uint64_t target_l = 0U;
    WmBusApplicationDateTime target_year_date = {0};
    WmBusApplicationDateTime target_date = {0};
    bool target_year_date_unavailable = data[16] == 0xFFU && data[17] == 0xFFU;
    bool target_date_unavailable = data[22] == 0xFFU && data[23] == 0xFFU;
    if(!wmbus_decode_bcd(&data[WMBUS_QWATERV2_TOTAL_OFFSET], 4U, &total_l) ||
       !wmbus_decode_bcd(&data[WMBUS_QWATERV2_TARGET_YEAR_OFFSET], 4U, &target_year_l) ||
       !wmbus_decode_bcd(&data[WMBUS_QWATERV2_TARGET_OFFSET], 4U, &target_l) ||
       (!target_year_date_unavailable &&
        !wmbus_decode_date(&data[WMBUS_QWATERV2_TARGET_YEAR_DATE_OFFSET], &target_year_date)) ||
       (!target_date_unavailable &&
        !wmbus_decode_date(&data[WMBUS_QWATERV2_TARGET_DATE_OFFSET], &target_date))) {
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
