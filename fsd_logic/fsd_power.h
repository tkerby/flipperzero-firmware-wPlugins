#pragma once
/*
 * fsd_power.h — "did the car cut our power, or did we fall over?"
 *
 * The rear tap gives us CAN, ground and 12V. Whether that 12V is ALWAYS-ON or
 * SWITCHED decides the final install: always-on means the module draws current
 * while the car sleeps, and a module that wakes whenever the car wakes is a
 * failsafe concern of its own.
 *
 * WHY NOT JUST USE A MULTIMETER
 * -----------------------------
 * Because reaching the connector means opening a door, and opening a door wakes
 * the car. The meter would have to be clipped on, read through the glass, and
 * even then it says only "0 V". This says more:
 *
 *   POWERON   the supply went away and came back
 *   BROWNOUT  the supply sagged -- thin wire, bad crimp, e-fuse near its limit
 *   SOFTWARE  we crashed; the supply is not implicated at all
 *
 * Those three want three different responses, and a voltage reading cannot tell
 * them apart.
 *
 * THE TRAP THIS AVOIDS
 * --------------------
 * "Still running after an hour" does NOT mean always-on. It also happens when
 * the car never slept -- sentry left on, a scheduled charge, someone opening
 * the app. So the record carries how long the BUS had been quiet, and a session
 * where the bus never went quiet is reported as inconclusive rather than as a
 * verdict. Answering the wrong question confidently is the failure mode here.
 *
 * PURE. The shim (esp32/.firmware/power_log.cpp) owns NVS, esp_reset_reason()
 * and the clock; this file owns what the numbers mean.
 */

#include <stdbool.h>
#include <stdint.h>

/* Included from power_log.cpp. Without this the C++ side mangles the names and
 * the linker cannot find the C definitions. */
#ifdef __cplusplus
extern "C" {
#endif

/** No CAN frame for this long counts as "the bus went quiet". */
#ifndef FSD_PWR_QUIET_MS
#define FSD_PWR_QUIET_MS 10000u
#endif

/**
 * Still running this long after the bus went quiet -> always-on.
 *
 * A judgement, not a measurement. The car takes 15-30 min to sleep, and a
 * switched feed dies at or shortly after that point. Surviving ten minutes of
 * silence is good evidence; it is not proof, which is why the reported verdict
 * is what the operator reads rather than what any code branches on.
 */
#ifndef FSD_PWR_ALWAYS_ON_MS
#define FSD_PWR_ALWAYS_ON_MS 600000u
#endif

/** Why we are running again. Mapped from esp_reset_reason() by the shim. */
typedef enum {
    FSD_PWR_RESET_UNKNOWN = 0,
    FSD_PWR_RESET_POWERON,  /* supply went away and came back */
    FSD_PWR_RESET_BROWNOUT, /* supply sagged below the rail */
    FSD_PWR_RESET_SOFTWARE, /* panic, watchdog, esp_restart() */
    FSD_PWR_RESET_EXTERNAL, /* reset pin */
} FsdPwrReset;

typedef enum {
    FSD_PWR_UNKNOWN = 0, /* nothing recorded yet, or nothing can be said */
    FSD_PWR_NO_SLEEP,    /* the bus never went quiet -- the car did not sleep */
    FSD_PWR_SWITCHED,    /* bus went quiet, then the supply died */
    FSD_PWR_ALWAYS_ON,   /* bus quiet a long time and we are still running */
    FSD_PWR_BROWNOUT,    /* supply sagged -- a wiring verdict, not a switching one */
    FSD_PWR_CRASH,       /* we fell over; says nothing about the supply */
} FsdPwrVerdict;

/**
 * What the shim writes to NVS periodically and reads back on the next boot.
 *
 * `quiet_ms` is a DURATION, not a timestamp: how long the bus had been quiet
 * at the moment of the write. 0 means the bus was still carrying traffic.
 */
typedef struct {
    bool valid;
    uint32_t uptime_ms;
    uint32_t quiet_ms;
} FsdPwrRecord;

/**
 * What the previous session tells us, read at boot.
 *
 * Returns FSD_PWR_UNKNOWN when there is no record or the reset reason does not
 * license a conclusion. That is the honest answer far more often than not, and
 * it is the reason this returns a verdict rather than a bool.
 */
FsdPwrVerdict fsd_pwr_verdict(FsdPwrReset reset, const FsdPwrRecord* prev);

/**
 * What THIS session tells us, while it is still running.
 *
 * The boot path can never report ALWAYS_ON, because a module that kept its
 * power never rebooted to report anything. So the live path covers that case:
 * still alive, bus quiet for long enough, therefore the feed did not die.
 */
FsdPwrVerdict fsd_pwr_live_verdict(uint32_t quiet_ms);

/**
 * How long the bus has been quiet, or 0 while it is carrying traffic.
 *
 * Kept here rather than inlined at the call site so the "never seen a frame"
 * case is decided once: a module that has never received anything has not
 * observed the bus going quiet -- it has observed nothing.
 */
uint32_t fsd_pwr_quiet_ms(uint32_t now_ms, uint32_t last_rx_ms, bool seen_any);

const char* fsd_pwr_reset_str(FsdPwrReset r);
const char* fsd_pwr_verdict_str(FsdPwrVerdict v);

#ifdef __cplusplus
}
#endif
