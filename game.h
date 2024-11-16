#pragma once
#include <furi.h>
#include <engine/game_engine.h>
#include <engine/game_manager_i.h>

#include "game_menu.h"
#include "reinforcement_learning_logic.h"
#include "game_level.h"
#include "tutorial_level.h"

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

#ifdef __cplusplus
extern "C" {
#endif
//#define DEBUGGING

//Configurable variables

//While debugging we increase all lives for longer testing/gameplay.
extern int ENEMY_LIVES;

extern uint32_t shootingDelay;
extern uint32_t enemyShootingDelay;
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
extern int firstMobSpawnTicks;
extern uint32_t firstKillTick;
extern uint32_t secondKillTick;
extern uint32_t gameBeginningTick;
extern Level* gameLevel;

//Functions
float lerp(float y1, float y2, float t);
void enemy_spawn(
    Level* level,
    GameManager* manager,
    Vector spawnPosition,
    uint32_t mercyTicks,
    bool right);

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
    Sprite* sprite_forward;

    bool horizontalGame;
} PlayerContext;

typedef struct {
    uint32_t score;
} GameContext;

#ifdef __cplusplus
}
#endif
