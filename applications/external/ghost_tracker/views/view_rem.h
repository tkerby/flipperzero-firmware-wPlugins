#pragma once

#include <gui/view.h>

/**
 * REM Pod module (electric-field / capacitive proximity).
 *
 * Real sensing technique: a bare copper wire antenna is wired to physical
 * pin 15 (PC1), configured as a floating analog input with NO oversampling.
 * A floating high-impedance ADC node picks up ambient 50/60Hz mains electric
 * field coupling; a nearby body/object changes that coupling and increases
 * the sample-to-sample jitter (peak-to-peak noise) on the pin. We measure
 * that real peak-to-peak jitter directly from raw hardware samples - this is
 * the same physical principle real REM Pod devices use (a high-Z antenna
 * node, not a literal RC charge-time circuit), and needs no extra
 * components beyond the wire.
 */

typedef struct RemView RemView;

RemView* rem_view_alloc(void);
void rem_view_free(RemView* instance);
View* rem_view_get_view(RemView* instance);
