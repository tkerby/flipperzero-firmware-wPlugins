#pragma once
#include "game.h"

typedef struct
{
    char id[64];
    int index;
} LevelContext;

extern Level *level_tree;
extern Level *level_example;
extern const LevelBehaviour tree_level;
extern const LevelBehaviour example_level;
extern const LevelBehaviour *level_behaviors[10];
extern LevelContext level_contexts[];
extern Level *levels[];
extern int level_count;

bool level_load_all();