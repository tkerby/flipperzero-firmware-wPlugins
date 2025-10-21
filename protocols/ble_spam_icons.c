#include <gui/icon.h>

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

/* Weak, zero-sized placeholders. If uFBT generates real icons,
   those strong symbols override these automatically at link time. */
const Icon I_ble_spam             WEAK = (Icon){0};
const Icon I_ble                  WEAK = (Icon){0};
const Icon I_SmallArrowUp_3x5     WEAK = (Icon){0};
const Icon I_SmallArrowDown_3x5   WEAK = (Icon){0};
const Icon I_Pin_back_arrow_10x8  WEAK = (Icon){0};
const Icon I_WarningDolphin_45x42 WEAK = (Icon){0};

const Icon I_android              WEAK = (Icon){0};
const Icon I_apple                WEAK = (Icon){0};
const Icon I_windows              WEAK = (Icon){0};
const Icon I_heart                WEAK = (Icon){0};
