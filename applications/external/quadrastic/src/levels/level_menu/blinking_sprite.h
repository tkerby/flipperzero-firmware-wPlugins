#pragma once

#include "../../engine/entity.h"

typedef struct Sprite Sprite;

typedef struct
{
    Sprite* sprite;
    float delay;
    float show_duration;
    float hide_duration;
    float time;
} BlinkingSpriteContext;

void
blinking_sprite_init(Entity* entity,
                     GameManager* manager,
                     Vector pos,
                     float delay,
                     float show_duration,
                     float hide_duration,
                     const char* sprite_name);

extern const EntityDescription blinking_sprite_description;
