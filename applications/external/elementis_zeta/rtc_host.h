#pragma once
// Contract 68: HOST-ONLY stand-in for the firmware's RTC (lib/datetime/datetime.h DateTime + furi_hal_rtc_get_datetime), used by the casino plugin's `clock` operation (casino.c). The device build never includes
// this file. The host harness sets the clock by writing g_host_dt (defined in casino.c); its default is noon.
#include <stdint.h>

typedef struct {
    uint8_t hour; // 0..23
    uint8_t minute; // 0..59
    uint8_t second; // 0..59
    uint8_t day; // 1..31
    uint8_t month; // 1..12
    uint16_t year; // 2000..2099
    uint8_t weekday; // 1..7
} DateTime;

extern DateTime g_host_dt;
void furi_hal_rtc_get_datetime(DateTime* dt);
