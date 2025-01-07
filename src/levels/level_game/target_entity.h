#pragma once

#include "src/engine/entity.h"

typedef struct Sprite Sprite;

typedef struct
{
    Sprite* sprite;
    float time;
} TargetContext;

Entity*
create_target(Level* level, GameManager* manager);

void
target_reset(Entity* self, GameManager* manager);

extern const EntityDescription target_description;