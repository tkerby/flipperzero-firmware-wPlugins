#pragma once
#include "game.h"
#include "flip_world.h"
typedef struct
{
    char id[64];
    int index;
} LevelContext;

const LevelBehaviour *generic_level(const char *id, int index);
bool allocate_level(GameManager *manager, int index);
void set_world(Level *level, GameManager *manager, char *id);