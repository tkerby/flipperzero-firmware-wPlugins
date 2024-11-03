#pragma once
#include <furi.h>
#include <engine/game_engine.h>
#include <engine/game_manager_i.h>

#include "game_menu.h"
#include "reinforcement_learning_logic.h"
#include "tutorial.h"

/* Global game defines go here */

#define WORLD_BORDER_LEFT_X   6
#define WORLD_BORDER_RIGHT_X  120
#define WORLD_BORDER_TOP_Y    6
#define WORLD_BORDER_BOTTOM_Y 53

#define WORLD_TRANSITION_LEFT_STARTING_POINT 0
#define WORLD_TRANSITION_LEFT_ENDING_POINT   4

#define BULLET_DISTANCE_THRESHOLD 8

#define STARTING_PLAYER_HEALTH 11
#define STARTING_ENEMY_HEALTH  5
//#define DEBUGGING

//Configurable variables

//While debugging we increase all lives for longer testing/gameplay.
extern int ENEMY_LIVES;

extern uint32_t shootingRate;
extern uint32_t enemyShootingRate;
extern float bulletMoveSpeed;
extern float speed;
extern float enemySpeed;
extern float jumpHeight;
extern float jumpSpeed;
extern float enemyJumpHeight;
//Tracking player data
extern int health;
extern uint32_t kills;
extern bool jumping;
extern float targetY;
extern float targetX;
//Internals
extern bool hasSpawnedFirstMob;
extern int firstMobSpawnTicks;
extern uint32_t firstKillTick;
extern uint32_t secondKillTick;
extern Level* gameLevel;

//Functions
float lerp(float y1, float y2, float t);
void enemy_spawn(Level* level, GameManager* manager, Vector spawnPosition);

typedef struct {
    Sprite* sprite;

    Sprite* sprite_left;
    Sprite* sprite_left_shadowed;
    Sprite* sprite_left_recoil;

    Sprite* sprite_right;
    Sprite* sprite_right_shadowed;
    Sprite* sprite_right_recoil;

    Sprite* sprite_jump;
    Sprite* sprite_stand;
} PlayerContext;

typedef struct {
    uint32_t score;
} GameContext;
