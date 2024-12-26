#pragma once

#include "../../engine/entity.h"
#include "../../engine/vector.h"

typedef struct Sprite Sprite;

typedef struct
{
    Sprite* sprite;
    Vector pos_start;
    Vector pos_end;
    float duration;
    float time;
} MovingSpriteContext;

void
moving_sprite_init(Entity* entity,
                   GameManager* manager,
                   Vector pos_start,
                   Vector pos_end,
                   float duration,
                   const char* sprite_name);

extern const EntityDescription moving_sprite_description;
