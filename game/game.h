#pragma once
#include "engine/engine.h"
#include <game/world.h>
#include <game/level.h>
#include "flip_world.h"
#include "flip_storage/storage.h"

#define PLAYER_COLLISION_VERTICAL 5
#define PLAYER_COLLISION_HORIZONTAL 5

typedef struct
{
    uint32_t xp;
    uint32_t level;
    uint32_t health;
    //
    uint32_t strength; // for later uppdate
    uint32_t defense;  // for later uppdate
} GameContext;

typedef struct
{
    Vector trajectory;    // Direction player would like to move.
    float radius;         // collision radius
    int8_t dx;            // x direction
    int8_t dy;            // y direction
    Sprite *sprite_right; // player sprite
    Sprite *sprite_left;  // player sprite looking left
    bool is_looking_left; // player is looking left
} PlayerContext;

extern const EntityDescription player_desc;
void player_spawn(Level *level, GameManager *manager);

// Get game context: GameContext* game_context = game_manager_game_context_get(manager);