#pragma once

#include <stdint.h>

// Port: wall-clock time. The domain takes epoch seconds as opaque values; this is the only
// way the app obtains "now". Adapter (platform/) wraps furi's RTC; tests inject a fake.
typedef struct {
    void *self;
    uint32_t (*now)(void *self);
} WcClockPort;

static inline uint32_t wc_clock_now(const WcClockPort *c) {
    return c->now(c->self);
}
