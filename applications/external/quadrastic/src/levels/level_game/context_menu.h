#pragma once

#include "../../engine/entity.h"

typedef void (*ContextMenuBackCallback)(void* context);

void context_menu_back_callback_set(
    Entity* entity,
    ContextMenuBackCallback back_callback,
    void* callback_context);

extern const EntityDescription context_menu_description;
