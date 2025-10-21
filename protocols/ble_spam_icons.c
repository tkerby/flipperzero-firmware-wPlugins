#include <gui/icon.h>

#ifndef WEAK
#define WEAK __attribute__((weak))
#endif

#define ZERO_ICON (Icon){0}

/* Weak placeholders; if uFBT generates real icons,
   the strong symbols will override these automatically. */
const Icon I_ble_spam             WEAK = ZERO_ICON;
const Icon I_ble                  WEAK = ZERO_ICON;
const Icon I_SmallArrowUp_3x5     WEAK = ZERO_ICON;
const Icon I_SmallArrowDown_3x5   WEAK = ZERO_ICON;
const Icon I_Pin_back_arrow_10x8  WEAK = ZERO_ICON;
const Icon I_WarningDolphin_45x42 WEAK = ZERO_ICON;

const Icon I_ble_spam_10px        WEAK = ZERO_ICON;
const Icon I_android              WEAK = ZERO_ICON;
const Icon I_apple                WEAK = ZERO_ICON;
const Icon I_windows              WEAK = ZERO_ICON;
const Icon I_heart                WEAK = ZERO_ICON;
