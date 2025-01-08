#pragma once

#include "src/engine/entity.h"

typedef struct Sprite Sprite;

typedef struct
{
    Sprite* logo_sprite;
    uint32_t score;
    uint32_t max_score;
} GameOverEntityContext;

extern const EntityDescription game_over_description;
