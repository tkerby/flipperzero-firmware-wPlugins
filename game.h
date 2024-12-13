#pragma once
#include "engine/engine.h"
#include <draw/world.h>

// from https://github.com/jamisonderek/flipper-zero-tutorials/blob/main/vgm/apps/air_labyrinth/walls.h
typedef struct
{
    bool horizontal;
    int x;
    int y;
    int length;
} Wall;

#define WALL(h, y, x, l)   \
    (Wall)                 \
    {                      \
        h, x * 2, y * 2, l \
    }

typedef struct
{
    uint32_t score;
} GameContext;
