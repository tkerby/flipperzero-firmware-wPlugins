#pragma once

#include "src/engine/entity.h"

typedef struct Sprite Sprite;

typedef struct
{
    Sprite* sprite;
    float delay;
    float time;
} DelayedSpriteContext;

Entity*
delayed_sprite_add_to_level(Level* level,
                            GameManager* manager,
                            Vector pos,
                            float delay,
                            const char* sprite_name);

extern const EntityDescription delayed_sprite_description;
