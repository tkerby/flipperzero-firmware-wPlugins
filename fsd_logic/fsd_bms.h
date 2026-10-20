#pragma once
/*
 * fsd_bms.h — read-only battery decoders, shared header-only by the Flipper and
 * ESP32 builds (the ESP32 build compiles no fsd_logic source files).
 *
 * 0x312 BMS_thermalStatus pack temperature: byte4 - 40 / byte5 - 40 deg C is not
 * from any DBC. Tesla's signal list puts BMS_minPackTemperature and
 * BMS_maxPackTemperature in this frame behind BMS_thermalStatusMultiplexer, at
 * positions not known here. On the #211 Model Y the decode read 206~8 and
 * 205~-32 deg C (shown wrapped to int8 as -50~8 and -51~-32) on an autumn day,
 * and 24~101 on the one 0x312 in its dump. So the layout is kept, but a reading
 * is stored only when it is plausible for one pack; the last plausible one is
 * kept with its age.
 */

#include "fsd_state.h"
#include <stdbool.h>
#include <stdint.h>

#define FSD_BMS_THERMAL_MIN_DLC  6u
#define FSD_BMS_TEMP_MIN_BYTE    4u
#define FSD_BMS_TEMP_MAX_BYTE    5u
#define FSD_BMS_TEMP_OFFSET      40
// Plausibility bounds, heuristic (not from a DBC):
//  -40 deg C: the automotive cold-soak floor; colder is another signal.
//  +70 deg C: well past where a pack is allowed to run; hotter is another signal.
//  20 deg C spread: the coldest and warmest module of one pack sit within a few
//  degrees, leaving room for heating / fast-charge gradients; wider is not one pack.
#define FSD_BMS_TEMP_LOW_C       (-40)
#define FSD_BMS_TEMP_HIGH_C      70
#define FSD_BMS_TEMP_SPREAD_C    20
// A plausible reading older than this is stale (frames stopped, or every one
// since was rejected): no read-out, no cold/hot advisory.
#define FSD_BMS_TEMP_STALE_MS    30000u

static inline bool fsd_bms_temp_plausible(int min_c, int max_c) {
    return min_c >= FSD_BMS_TEMP_LOW_C && max_c <= FSD_BMS_TEMP_HIGH_C &&
           min_c <= max_c && (max_c - min_c) <= FSD_BMS_TEMP_SPREAD_C;
}

// Parse one 0x312. Marks the BMS seen; stores min/max only when plausible,
// otherwise counts the frame in batt_temp_rejected and keeps the last good one.
static inline void fsd_bms_thermal_update(FSDState* s, const uint8_t* d, uint8_t dlc,
                                          uint32_t now_ms) {
    if (dlc < FSD_BMS_THERMAL_MIN_DLC) return;
    int tmin = (int)d[FSD_BMS_TEMP_MIN_BYTE] - FSD_BMS_TEMP_OFFSET;
    int tmax = (int)d[FSD_BMS_TEMP_MAX_BYTE] - FSD_BMS_TEMP_OFFSET;
    s->bms_seen = true;
    if (!fsd_bms_temp_plausible(tmin, tmax)) {
        s->batt_temp_rejected++;
        return;
    }
    s->batt_temp_min_c = (int8_t)tmin;
    s->batt_temp_max_c = (int8_t)tmax;
    s->batt_temp_ok    = true;
    s->batt_temp_ms    = now_ms;
}

// True while a plausible reading is at most FSD_BMS_TEMP_STALE_MS old.
static inline bool fsd_bms_temp_fresh(const FSDState* s, uint32_t now_ms) {
    return s->batt_temp_ok && (uint32_t)(now_ms - s->batt_temp_ms) <= FSD_BMS_TEMP_STALE_MS;
}
