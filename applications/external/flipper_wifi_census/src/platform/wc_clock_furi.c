#include "include/platform/wc_clock_furi.h"

#include <furi_hal_rtc.h>

#include <stddef.h>

static uint32_t clock_now(void *self) {
    (void)self;
    return furi_hal_rtc_get_timestamp();
}

WcClockPort wc_clock_furi_port(void) {
    WcClockPort p = {.self = NULL, .now = clock_now};
    return p;
}
