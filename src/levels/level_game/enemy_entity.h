#pragma once

#include "../../engine/entity.h"

typedef struct Sprite Sprite;

typedef enum
{
    EnemyDirectionUp,
    EnemyDirectionDown,
    EnemyDirectionLeft,
    EnemyDirectionRight,
} EnemyDirection;

typedef struct
{
    Sprite* sprite;
    EnemyDirection direction;
    float speed;
} EnemyContext;

void
spawn_enemy(GameManager* manager);

extern const EntityDescription enemy_description;
