#pragma once

#include <m-list.h>

#include "src/engine/level.h"

LIST_DEF(EntityList, Entity*, M_POD_OPLIST);
#define M_OPL_EntityList_t() LIST_OPLIST(EntityList)

typedef struct
{
    Entity* pause_menu;
    Entity* player;
    Entity* target;
    EntityList_t enemies;

    bool is_paused;

} GameLevelContext;

void
pause_game(Level* level);

void
resume_game(Level* level);

extern const LevelBehaviour level_game;
