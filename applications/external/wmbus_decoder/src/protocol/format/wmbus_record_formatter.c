#include "wmbus_record_formatter.h"

#include <stdio.h>
#include <string.h>

#include "wmbus_hex_utils.h"
#include "../model/wmbus_application_record.h"

typedef struct {
    const char* label;
    const char* unit;
} WmBusRecordPresentation;

static const WmBusRecordPresentation wmbus_record_presentations[] = {
    [WmBusApplicationQuantityUnknown] = {.label = "Data"},
    [WmBusApplicationQuantityVolume] = {.label = "Volume", .unit = "m3"},
    [WmBusApplicationQuantityEnergy] = {.label = "Energy", .unit = "Wh"},
    [WmBusApplicationQuantityPower] = {.label = "Power", .unit = "W"},
    [WmBusApplicationQuantityVolumeFlow] = {.label = "Flow", .unit = "m3/h"},
    [WmBusApplicationQuantityFlowTemperature] = {.label = "Flow temp", .unit = "C"},
    [WmBusApplicationQuantityReturnTemperature] = {.label = "Return temp", .unit = "C"},
    [WmBusApplicationQuantityTemperatureDifference] = {.label = "Delta temp", .unit = "K"},
    [WmBusApplicationQuantityHeatCostAllocation] = {.label = "HCA"},
    [WmBusApplicationQuantityDate] = {.label = "Date"},
    [WmBusApplicationQuantityDateTime] = {.label = "DateTime"},
    [WmBusApplicationQuantityVersion] = {.label = "Version"},
    [WmBusApplicationQuantityStatus] = {.label = "Status"},
    [WmBusApplicationQuantityVoltage] = {.label = "Voltage", .unit = "V"},
};

static const WmBusRecordPresentation*
    wmbus_record_formatter_presentation(const WmBusApplicationRecord* record) {
    if(!record || (size_t)record->quantity >= COUNT_OF(wmbus_record_presentations)) {
        return &wmbus_record_presentations[WmBusApplicationQuantityUnknown];
    }
    const WmBusRecordPresentation* presentation = &wmbus_record_presentations[record->quantity];
    return presentation->label ? presentation :
                                 &wmbus_record_presentations[WmBusApplicationQuantityUnknown];
}

static const char*
    wmbus_record_formatter_measurement_type_short(const WmBusApplicationRecord* record) {
    if(!record) return NULL;

    switch(record->measurement_type) {
    case WmBusApplicationMeasurementTypeMinimum:
        return "min";
    case WmBusApplicationMeasurementTypeMaximum:
        return "max";
    case WmBusApplicationMeasurementTypeAtError:
        return "err";
    case WmBusApplicationMeasurementTypeInstantaneous:
    case WmBusApplicationMeasurementTypeUnknown:
    default:
        return NULL;
    }
}

bool wmbus_record_formatter_format_value_text(
    const WmBusApplicationRecord* record,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return false;
    out[0] = '\0';
    if(!record) return false;

    switch(record->value_type) {
    case WmBusApplicationValueScalar: {
        wmbus_application_format_scaled_value(record->value, record->scale10, out, out_size);
        const char* unit = wmbus_record_formatter_presentation(record)->unit;
        if(unit && out[0] != '\0') {
            size_t len = strlen(out);
            snprintf(&out[len], out_size - len, " %s", unit);
        }
        return true;
    }
    case WmBusApplicationValueDateTime:
        if(record->value_datetime.precision == WmBusApplicationDateTimePrecisionSecond) {
            snprintf(
                out,
                out_size,
                "%04u-%02u-%02u %02u:%02u:%02u",
                (unsigned int)record->value_datetime.year,
                (unsigned int)record->value_datetime.month,
                (unsigned int)record->value_datetime.day,
                (unsigned int)record->value_datetime.hour,
                (unsigned int)record->value_datetime.minute,
                (unsigned int)record->value_datetime.second);
        } else if(record->value_datetime.precision == WmBusApplicationDateTimePrecisionMinute) {
            snprintf(
                out,
                out_size,
                "%04u-%02u-%02u %02u:%02u",
                (unsigned int)record->value_datetime.year,
                (unsigned int)record->value_datetime.month,
                (unsigned int)record->value_datetime.day,
                (unsigned int)record->value_datetime.hour,
                (unsigned int)record->value_datetime.minute);
        } else {
            snprintf(
                out,
                out_size,
                "%04u-%02u-%02u",
                (unsigned int)record->value_datetime.year,
                (unsigned int)record->value_datetime.month,
                (unsigned int)record->value_datetime.day);
        }
        return true;
    case WmBusApplicationValueUnavailable:
        snprintf(out, out_size, "-");
        return true;
    case WmBusApplicationValueRaw:
        if((record->quantity == WmBusApplicationQuantityVersion ||
            record->quantity == WmBusApplicationQuantityStatus) &&
           record->data_len > 0U && record->data_len <= sizeof(record->raw_bytes)) {
            wmbus_hex_encode(record->raw_bytes, record->data_len, out, out_size);
            return out[0] != '\0';
        }
        return false;
    case WmBusApplicationValueNone:
    default:
        return false;
    }
}

static void
    wmbus_record_formatter_append_label(const WmBusApplicationRecord* record, FuriString* out) {
    furi_string_cat_str(out, wmbus_record_formatter_presentation(record)->label);
    const char* measurement = wmbus_record_formatter_measurement_type_short(record);
    if(!record->storage_no && !record->tariff && !record->subunit && !measurement) return;

    furi_string_push_back(out, '[');
    if(record->storage_no)
        furi_string_cat_printf(out, "S%llu ", (unsigned long long)record->storage_no);
    if(record->tariff) furi_string_cat_printf(out, "T%lu ", (unsigned long)record->tariff);
    if(record->subunit) furi_string_cat_printf(out, "U%u ", (unsigned int)record->subunit);
    if(measurement) furi_string_cat_printf(out, "%s ", measurement);
    furi_string_left(out, furi_string_size(out) - 1U);
    furi_string_push_back(out, ']');
}

bool wmbus_record_formatter_format_joined(
    const WmBusApplicationRecord* records,
    uint8_t record_count,
    char delimiter,
    FuriString* out) {
    if(!out) return false;
    furi_string_reset(out);
    if(!records || record_count == 0U) return false;

    bool wrote_any = false;
    uint64_t storage_no = 0U;
    bool have_storage = false;

    for(uint8_t i = 0U; i < record_count; i++) {
        if(!have_storage || records[i].storage_no < storage_no) {
            storage_no = records[i].storage_no;
            have_storage = true;
        }
    }

    while(have_storage) {
        for(uint8_t i = 0U; i < record_count; i++) {
            if(records[i].storage_no != storage_no) continue;

            char value[WMBUS_RECORD_VALUE_TEXT_MAX];
            if(!wmbus_record_formatter_format_value_text(&records[i], value, sizeof(value))) {
                continue;
            }
            if(wrote_any) furi_string_push_back(out, delimiter);
            wmbus_record_formatter_append_label(&records[i], out);
            furi_string_cat_printf(out, "=%s", value);
            wrote_any = true;
        }

        bool have_next = false;
        uint64_t next_storage = 0U;
        for(uint8_t i = 0U; i < record_count; i++) {
            if(records[i].storage_no > storage_no &&
               (!have_next || records[i].storage_no < next_storage)) {
                next_storage = records[i].storage_no;
                have_next = true;
            }
        }
        storage_no = next_storage;
        have_storage = have_next;
    }

    return wrote_any;
}
