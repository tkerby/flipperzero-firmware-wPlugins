#pragma once
#include "game.h"

typedef struct
{
    char id[64];
    int index;
} LevelContext;

const LevelBehaviour *generic_level(const char *id, int index);
