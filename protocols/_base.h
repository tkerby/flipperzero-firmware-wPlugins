// protocols/_base.h
#pragma once

#include <gui/icon.h>  // brings in 'typedef struct Icon Icon;' from SDK

// --- Make generated icons header optional on CI ---
#if defined(__has_include)
#  if __has_include("ble_spam_icons.h")
#    include "ble_spam_icons.h"
#    define MB_HAS_ICONS 1
#  else
#    define MB_HAS_ICONS 0
#  endif
#else
#  define MB_HAS_ICONS 0
#endif
