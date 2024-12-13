#pragma once
#include <flip_world.h>
#include <flip_storage/storage.h>

//
#include <furi.h>
#include "engine/engine.h"
#include "engine/game_engine.h"
#include "engine/game_manager_i.h"
#include "engine/level_i.h"
#include "engine/entity_i.h"

void free_all_views(void *context, bool free_variable_item_list);
void callback_submenu_choices(void *context, uint32_t index);