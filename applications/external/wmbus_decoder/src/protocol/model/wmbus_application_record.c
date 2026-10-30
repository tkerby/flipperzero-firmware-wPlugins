#include "wmbus_application_record.h"

#include <stdio.h>
#include <string.h>

static uint64_t wmbus_application_pow10_u64(uint8_t power) {
    uint64_t result = 1U;
    while(power > 0U) {
        result *= 10U;
        power--;
    }
    return result;
}

bool wmbus_application_record_append(
    WmBusPacketApplicationData* application,
    WmBusApplicationRecord** out_record) {
    if(!application || !out_record || application->record_count >= WMBUS_PACKET_RECORD_MAX) {
        return false;
    }

    WmBusApplicationRecord* record = &application->records[application->record_count++];
    *record = (WmBusApplicationRecord){0};
    *out_record = record;
    return true;
}

void wmbus_application_record_set_value(WmBusApplicationRecord* record, int64_t value) {
    if(!record) return;
    record->value_type = WmBusApplicationValueScalar;
    record->value = value;
}

bool wmbus_application_record_set_raw_bytes(
    WmBusApplicationRecord* record,
    const uint8_t* data,
    uint8_t data_len) {
    if(!record || !data || data_len == 0U || data_len > 8U) return false;

    record->data_len = data_len;
    record->value_type = WmBusApplicationValueRaw;
    memset(record->raw_bytes, 0, sizeof(record->raw_bytes));
    memcpy(record->raw_bytes, data, data_len);
    return true;
}

bool wmbus_application_find_total_volume(
    const WmBusApplicationRecord* records,
    uint8_t record_count,
    uint32_t* total_m3_x1000) {
    if(!records || !total_m3_x1000) return false;

    for(uint8_t i = 0; i < record_count; i++) {
        const WmBusApplicationRecord* record = &records[i];
        if(record->storage_no != 0U || record->quantity != WmBusApplicationQuantityVolume ||
           record->value_type != WmBusApplicationValueScalar || record->value < 0 ||
           record->scale10 < -22 || record->scale10 > 6) {
            continue;
        }

        uint64_t value = (uint64_t)record->value;

        if(record->scale10 >= -3) {
            uint64_t multiplier = wmbus_application_pow10_u64((uint8_t)(record->scale10 + 3));
            if(multiplier > UINT32_MAX || value > (UINT32_MAX / multiplier)) continue;
            *total_m3_x1000 = (uint32_t)(value * multiplier);
            return true;
        }

        uint8_t divisor_power = (uint8_t)(-3 - record->scale10);
        uint64_t divisor = wmbus_application_pow10_u64(divisor_power);
        if((value % divisor) != 0U) {
            continue;
        }

        uint64_t scaled = value / divisor;
        if(scaled > UINT32_MAX) continue;
        *total_m3_x1000 = (uint32_t)scaled;
        return true;
    }

    return false;
}

bool wmbus_application_format_total_volume_m3(
    const WmBusPacketApplicationData* application,
    char* out,
    size_t out_size,
    bool with_unit) {
    if(!out || out_size == 0U) return false;
    out[0] = '\0';
    if(!application) return false;

    uint32_t total_m3_x1000 = 0U;
    if(!wmbus_application_find_total_volume(
           application->records, application->record_count, &total_m3_x1000)) {
        return false;
    }

    uint32_t whole = total_m3_x1000 / 1000U;
    uint32_t frac = total_m3_x1000 % 1000U;
    snprintf(
        out,
        out_size,
        with_unit ? "%lu.%03lu m3" : "%lu.%03lu",
        (unsigned long)whole,
        (unsigned long)frac);
    return true;
}

static uint64_t wmbus_application_value_magnitude(int64_t value) {
    if(value >= 0) return (uint64_t)value;
    return (uint64_t)(-(value + 1)) + 1U;
}

void wmbus_application_format_scaled_value(
    int64_t value,
    int8_t scale10,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';

    uint64_t magnitude = wmbus_application_value_magnitude(value);
    const char* sign = value < 0 ? "-" : "";

    /* Outside the fixed-point range use an exact exponent representation. */
    if(scale10 < -19 || scale10 > 19) {
        snprintf(out, out_size, "%llde%d", (long long)value, (int)scale10);
        return;
    }

    if(scale10 >= 0) {
        int len = snprintf(out, out_size, "%s%llu", sign, (unsigned long long)magnitude);
        if(len < 0 || (size_t)len >= out_size) return;

        size_t write = (size_t)len;
        for(int8_t i = 0; i < scale10 && write + 1U < out_size; i++) {
            out[write++] = '0';
        }
        out[write] = '\0';
        return;
    }

    uint8_t decimals = (uint8_t)(-scale10);
    uint64_t div = wmbus_application_pow10_u64(decimals);
    uint64_t whole = magnitude / div;
    uint64_t frac = magnitude % div;
    snprintf(
        out,
        out_size,
        "%s%llu.%0*llu",
        sign,
        (unsigned long long)whole,
        (int)decimals,
        (unsigned long long)frac);
}
