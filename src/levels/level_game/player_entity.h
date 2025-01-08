#pragma once

#include "../../engine/entity.h"

typedef struct Sprite Sprite;

typedef struct
{
    Sprite* sprite;
    float time;
} PlayerContext;

Entity*
player_spawn(Level* level, GameManager* manager);

void
player_respawn(Entity* player);

extern const EntityDescription player_description;
