#pragma once
#include "game.h"

typedef struct
{
    char id[64];
    int index;
} LevelContext;

extern Level *level_tree;
extern const LevelBehaviour tree_level;
extern Level *level_example;
extern const LevelBehaviour example_level;
extern LevelContext level_contexts[];
extern Level *levels[];
extern int level_count;

bool level_load_all(GameManager *game_manager);