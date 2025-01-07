#pragma once

#include "src/engine/entity.h"

typedef void (*ContextMenuBackCallback)(void* context);
typedef void (*ContextMenuItemCallback)(void* context, uint32_t index);

void context_menu_add_item(
    Entity* self,
    const char* label,
    uint32_t index,
    ContextMenuItemCallback callback,
    void* callback_context);

void context_menu_back_callback_set(
    Entity* entity,
    ContextMenuBackCallback back_callback,
    void* callback_context);

void context_menu_reset_state(Entity* entity);

extern const EntityDescription context_menu_description;
