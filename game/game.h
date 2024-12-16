#pragma once
#include "engine/engine.h"
#include <game/world.h>
#include "flip_world.h"
#include "flip_storage/storage.h"

typedef struct
{
    uint32_t score;
} GameContext;

typedef struct
{
    Vector trajectory; // Direction player would like to move.
    float radius;      // collision radius
    int8_t dx;         // x direction
    int8_t dy;         // y direction
    Sprite *sprite;    // player sprite
} PlayerContext;

extern const EntityDescription player_desc;