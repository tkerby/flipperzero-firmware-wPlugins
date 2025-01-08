#pragma once

#include "src/engine/entity.h"
#include "src/engine/vector.h"

typedef struct Sprite Sprite;

typedef struct
{
    Sprite* sprite;
    Vector pos_start;
    Vector pos_end;
    float duration;
    float time;
} MovingSpriteContext;

Entity*
moving_sprite_add_to_level(Level* level,
                           GameManager* manager,
                           Vector pos_start,
                           Vector pos_end,
                           float duration,
                           const char* sprite_name);

extern const EntityDescription moving_sprite_description;
