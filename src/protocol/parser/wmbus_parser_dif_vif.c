#include "wmbus_parser_dif_vif.h"
#include "../decode/wmbus_value.h"

#include "../model/wmbus_application_record.h"

typedef enum {
    WmBusDifDataEncodingInteger = 0,
    WmBusDifDataEncodingBcd,
    WmBusDifDataEncodingNegativeBcd,
    WmBusDifDataEncodingFloat,
    WmBusDifDataEncodingVariableText,
} WmBusDifDataEncoding;

static bool wmbus_packet_decode_signed_le(const uint8_t* data, uint8_t data_len, int64_t* value) {
    if(!data || !value || data_len == 0U || data_len > 8U) return false;

    uint64_t raw = 0U;
    for(uint8_t i = 0; i < data_len; i++) {
        raw |= ((uint64_t)data[i]) << (8U * i);
    }

    const uint8_t sign_shift = (uint8_t)(data_len * 8U - 1U);
    const uint64_t sign_bit = UINT64_C(1) << sign_shift;
    if((raw & sign_bit) == 0U) {
        *value = (int64_t)raw;
        return true;
    }

    const uint64_t mask = data_len == 8U ? UINT64_MAX : ((sign_bit << 1U) - 1U);
    const uint64_t magnitude = ((~raw) & mask) + 1U;
    if(magnitude == (UINT64_C(1) << 63U)) {
        *value = INT64_MIN;
    } else {
        *value = -(int64_t)magnitude;
    }
    return true;
}

static bool wmbus_packet_decode_datetime4(const uint8_t* data, WmBusApplicationDateTime* out) {
    if(!data || !out) return false;

    uint8_t minute = data[0] & 0x3FU;
    uint8_t hour = data[1] & 0x1FU;
    if((data[0] & 0x80U) != 0U || minute > 59U || hour > 23U || !wmbus_decode_date(&data[2], out))
        return false;

    out->hour = hour;
    out->minute = minute;
    out->second = 0U;
    out->precision = WmBusApplicationDateTimePrecisionMinute;
    return true;
}

static bool wmbus_packet_decode_datetime6(const uint8_t* data, WmBusApplicationDateTime* out) {
    if(!data || !out) return false;

    uint8_t second = data[0] & 0x3FU;
    uint8_t minute = data[1] & 0x3FU;
    uint8_t hour = data[2] & 0x1FU;
    uint8_t day = data[3] & 0x1FU;
    uint8_t month = data[4] & 0x0FU;
    uint8_t year = (uint8_t)(((data[3] >> 5) & 0x07U) | ((data[4] >> 1) & 0x78U));
    bool time_invalid = (data[1] & 0x80U) != 0U;

    if(time_invalid || second > 59U || minute > 59U || hour > 23U || day == 0U || day > 31U ||
       month == 0U || month > 12U || year > 99U) {
        return false;
    }

    out->year = (uint16_t)year + 2000U;
    out->month = month;
    out->day = day;
    out->hour = hour;
    out->minute = minute;
    out->second = second;
    out->precision = WmBusApplicationDateTimePrecisionSecond;
    return true;
}

static int wmbus_packet_data_len_from_dif(uint8_t dif, WmBusDifDataEncoding* data_encoding) {
    /* EN 13757-3 DIF data field: -1 unsupported, -2 followed by LVAR. */
    static const int8_t lengths[16] = {0, 1, 2, 3, 4, 4, 6, 8, -1, 1, 2, 3, 4, -2, 6, -1};
    uint8_t code = dif & 0x0FU;
    *data_encoding = code == 0x05U                  ? WmBusDifDataEncodingFloat :
                     code >= 0x09U && code != 0x0DU ? WmBusDifDataEncodingBcd :
                                                      WmBusDifDataEncodingInteger;
    return lengths[code];
}

static bool
    wmbus_packet_decode_lvar(uint8_t lvar, uint8_t* data_len, WmBusDifDataEncoding* data_encoding) {
    if(!data_len || !data_encoding) return false;

    if(lvar <= 0xBFU) {
        *data_len = lvar;
        *data_encoding = WmBusDifDataEncodingVariableText;
    } else if(lvar >= 0xC0U && lvar <= 0xC9U) {
        *data_len = lvar - 0xC0U;
        *data_encoding = WmBusDifDataEncodingBcd;
    } else if(lvar >= 0xD0U && lvar <= 0xD9U) {
        *data_len = lvar - 0xD0U;
        *data_encoding = WmBusDifDataEncodingNegativeBcd;
    } else if(lvar >= 0xE0U && lvar <= 0xEFU) {
        *data_len = lvar - 0xE0U;
        *data_encoding = WmBusDifDataEncodingInteger;
    } else if(lvar >= 0xF0U && lvar <= 0xF6U) {
        /* EN 13757-3:2018 Table 5: extended binary lengths. */
        *data_len = lvar <= 0xF4U ? 4U * (lvar - 0xECU) : 16U * (lvar - 0xF2U);
        *data_encoding = WmBusDifDataEncodingInteger;
    } else {
        return false;
    }

    return true;
}

static WmBusApplicationMeasurementType wmbus_packet_measurement_type_from_dif(uint8_t dif) {
    switch(dif & 0x30U) {
    case 0x00U:
        return WmBusApplicationMeasurementTypeInstantaneous;
    case 0x10U:
        return WmBusApplicationMeasurementTypeMaximum;
    case 0x20U:
        return WmBusApplicationMeasurementTypeMinimum;
    case 0x30U:
        return WmBusApplicationMeasurementTypeAtError;
    default:
        return WmBusApplicationMeasurementTypeUnknown;
    }
}

static void
    wmbus_packet_map_vif(WmBusApplicationRecord* record, uint8_t first_vife, bool has_first_vife) {
    if(!record) return;

    record->quantity = WmBusApplicationQuantityUnknown;
    record->scale10 = 0;

    const uint8_t vif = record->vif & 0x7FU;
    const uint8_t first_vife_code = first_vife & 0x7FU;

    if((vif & 0x78U) == 0x10U) {
        record->quantity = WmBusApplicationQuantityVolume;
        record->scale10 = (int8_t)((int)(vif & 0x07U) - 6);
    } else if((vif & 0x78U) == 0x00U) {
        record->quantity = WmBusApplicationQuantityEnergy;
        record->scale10 = (int8_t)((int)(vif & 0x07U) - 3);
    } else if((vif & 0x78U) == 0x28U) {
        record->quantity = WmBusApplicationQuantityPower;
        record->scale10 = (int8_t)((int)(vif & 0x07U) - 3);
    } else if((vif & 0x78U) == 0x38U) {
        record->quantity = WmBusApplicationQuantityVolumeFlow;
        record->scale10 = (int8_t)((int)(vif & 0x07U) - 6);
    } else if((vif & 0x7CU) == 0x58U) {
        record->quantity = WmBusApplicationQuantityFlowTemperature;
        record->scale10 = (int8_t)((int)(vif & 0x03U) - 3);
    } else if((vif & 0x7CU) == 0x5CU) {
        record->quantity = WmBusApplicationQuantityReturnTemperature;
        record->scale10 = (int8_t)((int)(vif & 0x03U) - 3);
    } else if((vif & 0x7CU) == 0x60U) {
        record->quantity = WmBusApplicationQuantityTemperatureDifference;
        record->scale10 = (int8_t)((int)(vif & 0x03U) - 3);
    } else if(vif == 0x6EU) {
        record->quantity = WmBusApplicationQuantityHeatCostAllocation;
    } else if(vif == 0x6CU) {
        record->quantity = WmBusApplicationQuantityDate;
    } else if(vif == 0x6DU) {
        record->quantity = WmBusApplicationQuantityDateTime;
    } else if(vif == 0x7DU && has_first_vife && first_vife_code == 0x0CU) {
        record->quantity = WmBusApplicationQuantityVersion;
    } else if(vif == 0x7DU && has_first_vife && first_vife_code == 0x17U) {
        record->quantity = WmBusApplicationQuantityStatus;
    } else if(vif == 0x7DU && has_first_vife && (first_vife_code & 0x70U) == 0x40U) {
        /* EN 13757-3 secondary VIF: E100 nnnn = 10^(nnnn - 9) V. */
        record->quantity = WmBusApplicationQuantityVoltage;
        record->scale10 = (int8_t)((int)(first_vife_code & 0x0FU) - 9);
    }
}

static void wmbus_packet_decode_record_value(
    WmBusApplicationRecord* record,
    const uint8_t* data,
    uint8_t data_len,
    WmBusDifDataEncoding data_encoding) {
    if(!record) return;

    record->value_type = WmBusApplicationValueNone;
    record->value = 0;

    if(data_len == 0U) {
        return;
    }

    if(record->quantity == WmBusApplicationQuantityDate) {
        if(data_len == 2U && wmbus_decode_date(data, &record->value_datetime)) {
            record->value_type = WmBusApplicationValueDateTime;
        } else {
            record->value_type = WmBusApplicationValueUnavailable;
        }
        return;
    } else if(record->quantity == WmBusApplicationQuantityDateTime) {
        if((data_len == 4U && wmbus_packet_decode_datetime4(data, &record->value_datetime)) ||
           (data_len == 6U && wmbus_packet_decode_datetime6(data, &record->value_datetime))) {
            record->value_type = WmBusApplicationValueDateTime;
        } else {
            record->value_type = WmBusApplicationValueUnavailable;
        }
        return;
    }

    if(record->quantity == WmBusApplicationQuantityVersion ||
       record->quantity == WmBusApplicationQuantityStatus) {
        if(!wmbus_application_record_set_raw_bytes(record, data, data_len)) {
            record->value_type = WmBusApplicationValueRaw;
        }
        return;
    }

    if(data_encoding == WmBusDifDataEncodingVariableText) {
        record->value_type = WmBusApplicationValueRaw;
        return;
    }

    if(data_encoding == WmBusDifDataEncodingBcd ||
       data_encoding == WmBusDifDataEncodingNegativeBcd) {
        uint64_t bcd_value = 0U;
        if(wmbus_decode_bcd(data, data_len, &bcd_value) && bcd_value <= INT64_MAX) {
            int64_t value = (int64_t)bcd_value;
            wmbus_application_record_set_value(
                record, data_encoding == WmBusDifDataEncodingNegativeBcd ? -value : value);
        } else if(!wmbus_application_record_set_raw_bytes(record, data, data_len)) {
            record->value_type = WmBusApplicationValueRaw;
        }
        return;
    }

    if(data_encoding == WmBusDifDataEncodingInteger) {
        int64_t value = 0;
        if(wmbus_packet_decode_signed_le(data, data_len, &value)) {
            wmbus_application_record_set_value(record, value);
            return;
        }
    }

    if(!wmbus_application_record_set_raw_bytes(record, data, data_len)) {
        record->value_type = WmBusApplicationValueRaw;
    }
}

WmBusDataRecordStatus wmbus_dif_vif_next_record(
    const uint8_t* payload,
    size_t payload_len,
    size_t* offset,
    WmBusDataRecord* out) {
    if(!offset || !out || (!payload && payload_len) || *offset > payload_len) {
        return WmBusDataRecordInvalid;
    }
    size_t pos = *offset;
    while(pos < payload_len && payload[pos] == 0x2FU)
        pos++;
    if(pos == payload_len || payload[pos] == 0x0FU || payload[pos] == 0x1FU) {
        *offset = payload_len;
        return WmBusDataRecordEnd;
    }
    *out = (WmBusDataRecord){0};
    WmBusApplicationRecord* record = &out->record;
    record->dif = payload[pos++];
    record->measurement_type = wmbus_packet_measurement_type_from_dif(record->dif);

    uint64_t storage_no = (uint64_t)((record->dif >> 6) & 0x01U);
    uint8_t dife_index = 0U;

    while((record->dif & 0x80U) != 0U) {
        if(pos >= payload_len || dife_index >= WMBUS_APPLICATION_DIFE_MAX)
            return WmBusDataRecordInvalid;
        uint8_t dife = payload[pos++];
        storage_no |= (uint64_t)(dife & 0x0FU) << (1U + (4U * dife_index));
        record->tariff |= (uint32_t)((dife >> 4) & 0x03U) << (2U * dife_index);
        record->subunit |= (uint16_t)((dife >> 6) & 0x01U) << dife_index;
        dife_index++;
        if((dife & 0x80U) == 0U) break;
    }
    record->storage_no = storage_no;

    if(pos >= payload_len) return WmBusDataRecordInvalid;
    record->vif = payload[pos++];

    const uint8_t vif_code = record->vif & 0x7FU;
    if(vif_code == 0x7CU) {
        if(pos >= payload_len) return WmBusDataRecordInvalid;
        const uint8_t unit_len = payload[pos++];
        if((size_t)unit_len > payload_len - pos) return WmBusDataRecordInvalid;
        pos += unit_len;
    }

    uint8_t first_vife = 0U;
    uint8_t vife_count = 0U;
    bool unsupported_modifier = false;
    while((record->vif & 0x80U) != 0U) {
        if(pos >= payload_len || vife_count >= WMBUS_APPLICATION_VIFE_MAX)
            return WmBusDataRecordInvalid;
        uint8_t vife = payload[pos++];
        if(vife_count == 0U) {
            first_vife = vife;
        }
        /* The first FD/FB extension selects a unit table. Other VIFEs
         * modify the unit or report errors; only "no error" is supported. */
        bool table_selector = vife_count == 0U && (vif_code == 0x7DU || vif_code == 0x7BU);
        if(!table_selector && (vife & 0x7FU) != 0U) unsupported_modifier = true;
        vife_count++;
        if((vife & 0x80U) == 0U) break;
    }

    WmBusDifDataEncoding data_encoding = WmBusDifDataEncodingInteger;
    int data_len = wmbus_packet_data_len_from_dif(record->dif, &data_encoding);
    if(data_len == -1) return WmBusDataRecordInvalid;
    if(data_len == -2) {
        uint8_t variable_len = 0U;
        if(pos >= payload_len ||
           !wmbus_packet_decode_lvar(payload[pos], &variable_len, &data_encoding)) {
            return WmBusDataRecordInvalid;
        }
        pos++;
        data_len = variable_len;
    }
    if(data_len < 0 || (size_t)data_len > (payload_len - pos)) return WmBusDataRecordInvalid;
    record->data_len = (uint8_t)data_len;

    out->first_vife = first_vife;
    out->vife_count = vife_count;
    out->data = &payload[pos];
    if(unsupported_modifier) {
        record->value_type = WmBusApplicationValueRaw;
        wmbus_application_record_set_raw_bytes(record, out->data, record->data_len);
    } else {
        wmbus_packet_map_vif(record, first_vife, vife_count != 0U);
        wmbus_packet_decode_record_value(record, out->data, record->data_len, data_encoding);
    }
    pos += record->data_len;
    *offset = pos;
    return WmBusDataRecordOk;
}

bool wmbus_packet_decode_application_records(
    const uint8_t* payload,
    size_t payload_len,
    WmBusApplicationRecord* out,
    uint8_t out_max,
    uint8_t* out_count) {
    if(out_count) *out_count = 0U;
    if(payload_len && (!payload || !out)) return false;
    size_t pos = 0U;
    uint8_t count = 0U;
    WmBusDataRecord data;
    WmBusDataRecordStatus status;
    while((status = wmbus_dif_vif_next_record(payload, payload_len, &pos, &data)) ==
          WmBusDataRecordOk) {
        if(count < out_max) out[count++] = data.record;
    }
    if(status == WmBusDataRecordInvalid) return false;
    if(out_count) *out_count = count;
    return true;
}

bool wmbus_parser_dif_vif_parse(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application) {
    if(!packet || !packet->payload || !packet->payload->application_bytes ||
       packet->payload->application_len == 0U || !out_application) {
        return false;
    }

    if(!wmbus_packet_decode_application_records(
           packet->payload->application_bytes,
           packet->payload->application_len,
           out_application->records,
           WMBUS_PACKET_RECORD_MAX,
           &out_application->record_count)) {
        return false;
    }

    for(uint8_t i = 0U; i < out_application->record_count; i++) {
        if(out_application->records[i].quantity != WmBusApplicationQuantityUnknown) {
            return true;
        }
    }
    return false;
}
