#pragma once
#include "VGMGameEngine.h"
void clear_player_username(Entity *player, Game *game, bool clear_current = false);
void enemy_spawn_json(Level *level, const char *json);
void player_spawn(Level *level, const char *name, Vector position);