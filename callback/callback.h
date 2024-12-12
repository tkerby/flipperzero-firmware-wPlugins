#pragma once
#include <flip_world.h>
#include <flip_storage/storage.h>

void free_all_views(void *context, bool free_variable_item_list);
void callback_submenu_choices(void *context, uint32_t index);