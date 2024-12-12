#pragma once
#include "engine/engine.h"

// Screen size
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
// World size (3x3)
#define WORLD_WIDTH 384
#define WORLD_HEIGHT 192

typedef struct
{
    uint32_t score;
} GameContext;