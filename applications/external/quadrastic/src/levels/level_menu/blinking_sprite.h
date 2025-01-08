#pragma once

#include "src/engine/entity.h"

typedef struct Sprite Sprite;

typedef struct {
    Sprite* sprite;
    float delay;
    float show_duration;
    float hide_duration;
    float time;
} BlinkingSpriteContext;

Entity* blinking_sprite_add_to_level(
    Level* level,
    GameManager* manager,
    Vector pos,
    float delay,
    float show_duration,
    float hide_duration,
    const char* sprite_name);

extern const EntityDescription blinking_sprite_description;
