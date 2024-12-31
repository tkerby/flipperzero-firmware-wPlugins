#pragma once

#include <m-list.h>

#include "../../engine/level.h"

LIST_DEF(EntityList, Entity*, M_POD_OPLIST);
#define M_OPL_EntityList_t() LIST_OPLIST(EntityList)
#define FOREACH(name, list) for                                                \
    M_EACH(name, list, EntityList_t)

typedef struct
{
    Entity* player;
    Entity* target;
    EntityList_t enemies;

    bool is_paused;
    Entity* pause_menu;

} GameLevelContext;

void
pause_game(Level* level);
void
resume_game(Level* level);

extern const LevelBehaviour level_game;
