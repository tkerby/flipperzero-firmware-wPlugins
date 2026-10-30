/*
 * Replacement for flipper-gblink's clock_timer.c, which is GPL-3.0 licensed.
 *
 * The clock timer is only used when the Flipper drives the serial clock
 * (GBLINK_CLK_INT). In a Cable Club link the Game Boy is always the clock
 * master, so this app only ever uses GBLINK_CLK_EXT and these are never
 * reached. Crash loudly if that assumption is ever broken.
 */
#include <furi.h>

/* Must match lib/flipper-gblink/gblink/clock_timer_i.h */
typedef void (*ClockTimerCallback)(void* context);

void clock_timer_start(ClockTimerCallback callback, void* context, float period) {
    UNUSED(callback);
    UNUSED(context);
    UNUSED(period);
    furi_crash("gblink INT clock unsupported");
}

void clock_timer_stop(void) {
}
