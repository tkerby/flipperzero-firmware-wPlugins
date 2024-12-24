#pragma once
#include "game.h"
#include "flip_world.h"

// EnemyContext definition
typedef struct
{
    char id[64];          // Unique ID for the enemy type
    int index;            // Index for the specific enemy instance
    Vector trajectory;    // Direction the enemy moves
    Sprite *sprite_right; // Enemy sprite when looking right
    Sprite *sprite_left;  // Enemy sprite when looking left
    bool is_looking_left; // Whether the enemy is facing left
    float radius;         // Collision radius for the enemy
    float x;              // X position
    float y;              // Y position
    float width;          // Width of the enemy
    float height;         // Height of the enemy
} EnemyContext;

const EntityDescription *enemy(GameManager *manager, const char *id, int index, float x, float y, float width, float height, bool moving_left);
