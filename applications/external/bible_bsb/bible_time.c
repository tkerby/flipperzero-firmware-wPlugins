#include "bible_time.h"

#include <stdio.h>

static uint8_t bible_days_in_month(uint16_t year, uint8_t month) {
    static const uint8_t days[] = {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t count = days[month];
    bool leap = ((year % 4) == 0 && (year % 100) != 0) || ((year % 400) == 0);
    if(month == 2 && leap) {
        count = 29;
    }
    return count;
}

bool bible_format_iso8601(
    char* out,
    size_t out_len,
    uint16_t year,
    uint8_t month,
    uint8_t day,
    uint8_t hour,
    uint8_t minute,
    uint8_t second) {
    if(!out || out_len < 21) {
        return false;
    }
    if(year > 9999 || month < 1 || month > 12) {
        return false;
    }
    if(day < 1 || day > bible_days_in_month(year, month)) {
        return false;
    }
    if(hour > 23 || minute > 59 || second > 59) {
        return false;
    }

    int wrote = snprintf(
        out,
        out_len,
        "%04u-%02u-%02uT%02u:%02u:%02uZ",
        (unsigned)year,
        (unsigned)month,
        (unsigned)day,
        (unsigned)hour,
        (unsigned)minute,
        (unsigned)second);
    return wrote == 20;
}
