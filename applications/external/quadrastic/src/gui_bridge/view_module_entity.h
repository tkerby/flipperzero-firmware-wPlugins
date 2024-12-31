#pragma once

#include "../engine/entity.h"

#include "view_module_descriptions.h"

/** Prototype for back event callback */
typedef bool (*ViewModuleBackCallback)(void* context);

Entity*
view_module_add_to_level(Level* level,
                         GameManager* manager,
                         const ViewModuleDescription* module_description);

void*
view_module_get_module(Entity* entity);

void
view_module_set_back_callback(Entity* entity,
                              ViewModuleBackCallback back_callback,
                              void* context);
