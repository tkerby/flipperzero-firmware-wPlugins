#pragma once

#include "src/engine/entity.h"

#define ENEMY_SIZE      6.0f
#define HALF_ENEMY_SIZE 3.0f

typedef struct Sprite Sprite;

typedef enum {
    EnemyDirectionUp,
    EnemyDirectionDown,
    EnemyDirectionLeft,
    EnemyDirectionRight,
} EnemyDirection;

typedef struct {
    Sprite* sprite;
    EnemyDirection direction;
    float speed;
} EnemyContext;

void enemy_spawn(GameManager* manager);

extern const EntityDescription enemy_description;
