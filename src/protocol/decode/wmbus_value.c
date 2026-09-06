#include "wmbus_value.h"

bool wmbus_decode_bcd(const uint8_t* data, uint8_t data_len, uint64_t* value) {
    /* EN 13757-3:2018 Table 5 allows up to 18 BCD digits. */
    if(!data || !value || data_len == 0U || data_len > 9U) return false;

    uint64_t result = 0U;
    uint64_t factor = 1U;
    for(uint8_t i = 0; i < data_len; i++) {
        uint8_t lo = data[i] & 0x0FU;
        uint8_t hi = (data[i] >> 4) & 0x0FU;
        if(lo > 9U || hi > 9U) return false;
        result += (uint64_t)lo * factor;
        factor *= 10U;
        result += (uint64_t)hi * factor;
        factor *= 10U;
    }
    *value = result;
    return true;
}

bool wmbus_decode_date(const uint8_t* data, WmBusApplicationDateTime* out) {
    if(!data || !out) return false;

    uint8_t day = data[0] & 0x1FU;
    uint8_t month = data[1] & 0x0FU;
    uint16_t year = (uint16_t)(((data[0] >> 5) & 0x07U) | ((data[1] >> 1) & 0x78U)) + 2000U;
    if(day == 0U || day > 31U || month == 0U || month > 12U) return false;

    out->year = year;
    out->month = month;
    out->day = day;
    out->hour = 0U;
    out->minute = 0U;
    out->second = 0U;
    out->precision = WmBusApplicationDateTimePrecisionDate;
    return true;
}
