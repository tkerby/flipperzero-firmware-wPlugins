#pragma once

#include "../../engine/entity.h"

typedef struct Sprite Sprite;

typedef struct {
    Sprite* sprite;
    float delay;
    float time;
} DelayedSpriteContext;

void delayed_sprite_init(
    Entity* entity,
    GameManager* manager,
    Vector pos,
    float delay,
    const char* sprite_name);

extern const EntityDescription delayed_sprite_description;
