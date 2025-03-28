#pragma once
#include <flip_world.h>
FlipWorldApp *flip_world_app_alloc();
void flip_world_app_free(FlipWorldApp *app);
void flip_world_show_submenu();
bool alloc_view_loader(void *context);
void free_view_loader(void *context);
