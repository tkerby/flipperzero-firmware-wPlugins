#pragma once
#include <furi.h>
#include <furi_hal_random.h>
#include <engine/engine.h>
#include <engine/entity.h>
#include <engine/game_manager_i.h>
#include <storage/storage.h>
#include "game.h"

#ifdef __cplusplus
extern "C" {
#endif

extern int playerLevel;

struct game_obstacle {
    float x;
    float y;
    float width;
    float height;
    bool direction;
    bool visible;

    void (*destructionTask)(void);
};

struct game_door {
    float x;
    float y;
    float width;
    float height;
    bool visible;

    int transitionTicks;
    char* transitionText;
    uint32_t transitionTime;
    void (*postTask)(void);
};

void game_level_player_update(Entity* self, GameManager* manager, void* context, Vector* pos);
void game_level_player_render(GameManager* manager, Canvas* canvas, void* context);
void game_level_enemy_render(GameManager* manager, Canvas* canvas, void* context);

#ifdef __cplusplus
}
#endif
