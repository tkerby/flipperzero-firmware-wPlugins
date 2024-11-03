#pragma once
#include <furi.h>
#include <furi_hal_random.h>
#include <engine/engine.h>
#include <engine/entity.h>
#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

void game_level_player_update(Entity* self, GameManager* manager, void* context, Vector* pos);
void game_level_player_render(GameManager* manager, Canvas* canvas, void* context);
void game_level_enemy_render(GameManager* manager, Canvas* canvas, void* context);

#ifdef __cplusplus
}
#endif