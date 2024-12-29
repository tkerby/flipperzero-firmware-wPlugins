#pragma once
#include "engine/engine.h"
#include <flip_world.h>
#include <game/game.h>

#define PLAYER_COLLISION_VERTICAL 0   // was 5
#define PLAYER_COLLISION_HORIZONTAL 0 // was 5

// Maximum enemies
#define MAX_ENEMIES 10

typedef enum
{
    PLAYER_IDLE,
    PLAYER_MOVING,
    PLAYER_ATTACKING,
    PLAYER_ATTACKED,
    PLAYER_DEAD,
} PlayerState;

typedef enum
{
    PLAYER_UP,
    PLAYER_DOWN,
    PLAYER_LEFT,
    PLAYER_RIGHT
} PlayerDirection;

typedef struct
{
    PlayerDirection direction;  // direction the player is facing
    PlayerState state;          // current state of the player
    Vector start_position;      // starting position of the player
    Sprite *sprite_right;       // player sprite looking right
    Sprite *sprite_left;        // player sprite looking left
    int8_t dx;                  // x direction
    int8_t dy;                  // y direction
    uint32_t xp;                // experience points
    uint32_t level;             // player level
    uint32_t strength;          // player strength
    uint32_t health;            // player health
    uint32_t max_health;        // player maximum health
    uint32_t health_regen;      // player health regeneration rate per second/frame
    float elapsed_health_regen; // time elapsed since last health regeneration
    float attack_timer;         // Cooldown duration between attacks
    float elapsed_attack_timer; // Time elapsed since the last attack
    char username[32];          // player username
} PlayerContext;

typedef struct
{
    PlayerContext *player_context;
    Level *levels[10];
    Entity *enemies[MAX_ENEMIES];
    Entity *players[1];
    GameKey user_input;
    float fps;
    int level_count;
    int enemy_count;
    int current_level;
} GameContext;

extern const EntityDescription player_desc;
void player_spawn(Level *level, GameManager *manager);