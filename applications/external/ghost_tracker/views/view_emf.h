#pragma once

#include <gui/view.h>

/**
 * EMF Meter module (K-II / RF Probe style).
 *
 * Two REAL hardware sources, selectable with LEFT/RIGHT:
 *  - Internal (ambient RF): reuses the CC1101 sub-GHz radio (same chip as
 *    Spirit Box/Radio Scanner), round-robining across a handful of fixed
 *    frequencies and reading real RSSI. We deliberately do NOT use the NFC
 *    chip here: its field-detect circuit only reacts to an active 13.56MHz
 *    NFC reader nearby (a phone, another reader) - never to generic
 *    magnetic/electric fields - so it would sit flat in any normal test.
 *    The CC1101's RSSI noise floor, by contrast, genuinely moves with
 *    nearby electronics and RF activity.
 *  - External (GPIO ADC): reads pin PC0 (physical pin 16) configured as an
 *    analog input, meant for a linear Hall-effect sensor (SS49E) powered
 *    from the Flipper's 3V3 rail. Flipper's ADC reference tops out at
 *    2.048V, so the sensor's ~1.65V zero-field baseline fits fine, but
 *    strong fields pushing the sensor output further up will saturate at
 *    the ADC's ceiling (reads as max/critical) instead of overflowing
 *    further. Rather than a raw DC deviation, we sample it fast (640Hz)
 *    and run a Goertzel filter tuned to 50Hz/60Hz to isolate the real AC
 *    "mains hum" ripple - the same physical quantity a real K-II/EMF
 *    meter's induction-coil + bandpass-filter front end reports.
 */

typedef struct EmfView EmfView;

EmfView* emf_view_alloc(void);
void emf_view_free(EmfView* instance);
View* emf_view_get_view(EmfView* instance);
