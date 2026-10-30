#pragma once

#include "include/ports/wc_clock_port.h"

// Clock port backed by the Flipper RTC (furi_hal_rtc_get_timestamp).
WcClockPort wc_clock_furi_port(void);
