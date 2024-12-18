#pragma once
#include "game.h"
#include "flip_world.h"
typedef struct
{
    char id[64];
    int index;
    FlipWorldApp *app;
} LevelContext;

const LevelBehaviour *generic_level(const char *id, int index);
