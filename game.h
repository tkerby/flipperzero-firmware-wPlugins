#pragma once
#include "engine/engine.h"
#include <draw/world.h>

void spawn_icon(Level *level, const Icon *icon, float x, float y, uint8_t width, uint8_t height);

typedef struct
{
    uint32_t score;
} GameContext;

typedef struct
{
    const Icon *icon;
    uint8_t width;
    uint8_t height;
} IconContext;