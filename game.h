#pragma once
#include "engine/engine.h"

// Screen size
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// World size (3x3)
#define WORLD_WIDTH 384
#define WORLD_HEIGHT 192

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