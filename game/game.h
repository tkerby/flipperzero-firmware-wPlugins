#pragma once
#include "engine/engine.h"
#include <game/world.h>
#include <game/level.h>
#include <game/enemy.h>
#include "flip_world.h"
#include "flip_storage/storage.h"

#define PLAYER_COLLISION_VERTICAL 5
#define PLAYER_COLLISION_HORIZONTAL 5

typedef enum
{
    PLAYER_IDLE,
    PLAYER_MOVING,
    PLAYER_ATTACKING,
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
    PlayerDirection direction; // direction the player is facing
    PlayerState state;         // current state of the player
    Vector start_position;     // starting position of the player
    Sprite *sprite_right;      // player sprite looking right
    Sprite *sprite_left;       // player sprite looking left
    int8_t dx;                 // x direction
    int8_t dy;                 // y direction
    uint32_t xp;               // experience points
    uint32_t level;            // player level
    uint32_t health;           // player health
    uint32_t strength;         // player strength
} PlayerContext;

typedef struct
{
    float fps;
    PlayerContext *player_context;
    Level *levels[10];
    Entity *enemies[2];
    Entity *players[1];
    int level_count;
} GameContext;

extern const EntityDescription player_desc;
void player_spawn(Level *level, GameManager *manager);

// Get game context: GameContext* game_context = game_manager_game_context_get(manager);