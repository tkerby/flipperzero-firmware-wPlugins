#pragma once
#include <game/game.h>
#include "flip_world.h"

typedef enum
{
    ENEMY_IDLE,
    ENEMY_MOVING,
    ENEMY_MOVING_TO_START,
    ENEMY_MOVING_TO_END,
    ENEMY_ATTACKING,
    ENEMY_ATTACKED,
    ENEMY_DEAD
} EnemyState;

typedef enum
{
    ENEMY_UP,
    ENEMY_DOWN,
    ENEMY_LEFT,
    ENEMY_RIGHT
} EnemyDirection;

// EnemyContext definition
typedef struct
{
    char id[64];                // Unique ID for the enemy type
    int index;                  // Index for the specific enemy instance
    Vector size;                // Size of the enemy
    Sprite *sprite_right;       // Enemy sprite when looking right
    Sprite *sprite_left;        // Enemy sprite when looking left
    EnemyDirection direction;   // Direction the enemy is facing
    EnemyState state;           // Current state of the enemy
    Vector start_position;      // Start position of the enemy
    Vector end_position;        // End position of the enemy
    float move_timer;           // Timer for the enemy movement
    float elapsed_move_timer;   // Elapsed time for the enemy movement
    float radius;               // Collision radius for the enemy
    float speed;                // Speed of the enemy
    float attack_timer;         // Cooldown duration between attacks
    float elapsed_attack_timer; // Time elapsed since the last attack
    float strength;             // Damage the enemy deals
    float health;               // Health of the enemy
} EnemyContext;

const EntityDescription *enemy(
    GameManager *manager,
    const char *id,
    int index,
    Vector start_position,
    Vector end_position,
    float move_timer, // Wait duration before moving again
    float speed,
    float attack_timer,
    float strength,
    float health);

void spawn_enemy_json_furi(Level *level, GameManager *manager, FuriString *json);