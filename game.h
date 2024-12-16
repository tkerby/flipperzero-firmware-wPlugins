#pragma once
#include "engine/engine.h"
#include <draw/world.h>

void spawn_icon(Level *level, const Icon *icon, float x, float y);

typedef struct
{
    uint32_t score;
} GameContext;
