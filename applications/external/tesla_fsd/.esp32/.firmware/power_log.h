#pragma once
/*
 * power_log.h — ties fsd_logic/fsd_power to the ESP32: NVS, esp_reset_reason(),
 * RTC no-init memory, the millis() clock and Serial.
 *
 * It answers two questions a user of this device cannot otherwise answer:
 *
 *   - WHY DID THE BOARD RESTART? Power-on, brownout, panic, watchdog, a plain
 *     software restart, or a deep-sleep wake each call for a different response.
 *     The last two releases rebooted whenever a browser opened the dashboard; a
 *     line on the dashboard saying "last reset was a crash" would have settled
 *     it in one screenshot.
 *
 *   - IS THE 12V AT THE TAP SWITCHED OR ALWAYS-ON? The tap carries CAN, ground
 *     and 12V. A switched feed dies when the car sleeps; an always-on feed draws
 *     current overnight. A multimeter can't answer it (reaching the connector
 *     wakes the car) but the board can: if it loses power after the bus has gone
 *     quiet, the feed is switched; if it is still running (or kept its RTC state
 *     across a deep sleep) long after the bus went quiet, it is always-on.
 *
 * PERSISTENCE. The 12V test has to survive the board losing power, so the
 * evidence lives in flash (NVS). To keep flash writes off the CAN path, nothing
 * is written while the bus is active: a single record is committed only after
 * the bus has been quiet for FSD_PWR_CHECKPOINT_MS, when there are no frames to
 * lose (fsd_pwr_checkpoint_due), and the commits per session are capped. Each
 * boot reads the record, judges it, then clears it, so a session reasons only
 * from its own checkpoints. State that only has to survive a reset, not a power
 * cut -- the restart counter and "we slept on a quiet bus" -- lives in RTC
 * no-init memory, which costs no flash.
 *
 * The judgement itself is in fsd_logic/fsd_power.{h,c} and host-tested. This
 * file owns only the hardware: NVS, esp_reset_reason(), RTC RAM, the clock and
 * Serial.
 *
 * Listen-Only: nothing here transmits.
 */

#include <stdbool.h>
#include <stdint.h>

#include "../../fsd_logic/fsd_power.h"

/* Fine-grained reset reasons for the dashboard. Coarser FsdPwrReset drives the
 * verdict (panic/watchdog/software all mean "we fell over"); these keep them
 * apart for the "why did it restart" line. The dashboard JS mirrors this order. */
enum {
    PWR_RST_UNKNOWN = 0,
    PWR_RST_POWERON,
    PWR_RST_BROWNOUT,
    PWR_RST_PANIC,
    PWR_RST_WATCHDOG,
    PWR_RST_SOFTWARE,
    PWR_RST_DEEPSLEEP,
    PWR_RST_EXTERNAL,
};

/**
 * Read the previous session's record, print the verdict, start a new session.
 * Call once from setup(), after Serial is up.
 */
void power_log_init(void);

/**
 * Feed the clock and the bus. Call from loop().
 *
 * A flash commit happens here and only here, and only once the bus has been
 * quiet past the checkpoint window -- never while frames are arriving.
 *
 * @param last_rx_ms  millis() of the last CAN frame (0 until the first one)
 * @param seen_any    whether any frame has ever arrived (0 is a legal clock
 *                    value, so last_rx_ms alone cannot say)
 */
void power_log_tick(uint32_t now_ms, uint32_t last_rx_ms, bool seen_any);

/**
 * Note that the board is about to deep-sleep on a quiet bus (LilyGO). Records
 * that in RTC RAM so a later deep-sleep wake can conclude the feed held through
 * the sleep, and flushes the quiet checkpoint if it has not already been
 * committed. Call right before esp_deep_sleep_start(); the bus is quiet there.
 */
void power_log_note_deep_sleep(uint32_t now_ms, uint32_t last_rx_ms, bool seen_any);

/** The verdict as it stands (live/deep-sleep outranks the boot reading). */
FsdPwrVerdict power_log_verdict(void);

/* Reporting helpers for the boot banner and the dashboard JSON. */
const char *power_log_verdict_str(void);
int         power_log_verdict_code(void);   /* FsdPwrVerdict */
const char *power_log_reset_str(void);
int         power_log_reset_code(void);     /* PWR_RST_* */
uint32_t    power_log_restart_count(void);  /* restarts since power-on; a deep-sleep wake is not one */
