#pragma once
#include "engine/engine.h"
#include "furi_hal.h"

#include "notification/notification_messages.h"

#include "gui/view_holder.h"
#include "gui/modules/widget.h"
#include "gui/modules/submenu.h"
#include "api_lock.h"
/* Global game defines go here */

typedef struct {
    uint32_t score;
} GameContext;