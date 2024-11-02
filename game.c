#include "game.h"

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

/****** Entities: Player ******/

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

//Useful for the reinforcement learning
enum EnemyAction {
    EnemyActionAttack,
    EnemyActionRetreat,
    EnemyActionStand,
    EnemyActionJump
};

//Enemy structure
struct Enemy {
    Entity* instance;
    bool jumping;
    float targetY;
    int lives;
    uint32_t lastShot;
};

// Forward declaration of player_desc, because it's used in player_spawn function.

//Configurable values
uint32_t shootingRate = 1700;
uint32_t enemyShootingRate = 1500;
float bulletMoveSpeed = 0.55f;
float speed = 0.6f;
float enemySpeed = 0.13f;
float jumpHeight = 22.0F;
float jumpSpeed = 0.074f;
//Enemies can jump slightly higher
float enemyJumpHeight = 22.0F + 10.0F;

//While debugging we increase all lives for longer testing/gameplay.
#ifdef DEBUGGING
static int ENEMY_LIVES = 30;
static int health = 100;
#else
static int ENEMY_LIVES = STARTING_ENEMY_HEALTH;
static int health = STARTING_PLAYER_HEALTH;
#endif

//Tracking player data
uint32_t kills = 0;
bool jumping = false;

//Enemy, and bullet storage
#define MAX_ENEMIES 30
struct Enemy enemies[MAX_ENEMIES];

#define MAX_BULLETS 30
Entity* bullets[MAX_BULLETS];
bool bulletsDirection[MAX_BULLETS];

Entity* enemyBullets[MAX_BULLETS];

Entity* globalPlayer = NULL;

//Internal variables
static float targetY = WORLD_BORDER_BOTTOM_Y;
static float targetX = 10.0F;
static const EntityDescription player_desc;
static const EntityDescription target_desc;
static const EntityDescription enemy_desc;
static const EntityDescription target_enemy_desc;
Level* gameLevel;

//More internal variables
uint32_t firstKillTick;
uint32_t secondKillTick;
bool tutorialCompleted = false;
bool startedGame = false;
bool hasSpawnedFirstMob;
int firstMobSpawnTicks;
uint32_t gameBeginningTick;
uint32_t lastShotTick;
uint32_t lastBehaviorTick;

enum EnemyAction lastAction = EnemyActionStand;

GameManager* globalGameManager;

static void frame_cb(GameEngine* engine, Canvas* canvas, InputState input, void* context) {
    UNUSED(engine);
    GameManager* game_manager = context;
    game_manager_input_set(game_manager, input);
    game_manager_update(game_manager);
    game_manager_render(game_manager, canvas);
}

static float lerp(float y1, float y2, float t) {
    return y1 + t * (y2 - y1);
}
/*
static Vector random_pos(void) {
    return (Vector){rand() % 120 + 4, rand() % 58 + 4};
}
*/

static void player_spawn(Level* level, GameManager* manager) {
    Entity* player = level_add_entity(level, &player_desc);
    globalPlayer = player;

    // Set player position.
    // Depends on your game logic, it can be done in start entity function, but also can be done here.
    entity_pos_set(player, (Vector){10, 32});

    // Add collision box to player entity
    // Box is centered in player x and y, and it's size is 16x16
    entity_collider_add_rect(player, 16, 16);

    // Get player context
    PlayerContext* player_context = entity_context_get(player);

    // Load player sprite
    player_context->sprite_right = game_manager_sprite_load(manager, "player_right.fxbm");
    player_context->sprite_right_shadowed =
        game_manager_sprite_load(manager, "player_right_shadow.fxbm");
    player_context->sprite_right_recoil =
        game_manager_sprite_load(manager, "player_right_recoil.fxbm");

    player_context->sprite_left = game_manager_sprite_load(manager, "player_left.fxbm");
    player_context->sprite_left_shadowed =
        game_manager_sprite_load(manager, "player_left_shadow.fxbm");
    player_context->sprite_left_recoil =
        game_manager_sprite_load(manager, "player_left_recoil.fxbm");
    player_context->sprite_jump = game_manager_sprite_load(manager, "player_jump.fxbm");
    player_context->sprite_stand = game_manager_sprite_load(manager, "player_stand.fxbm");

    player_context->sprite = player_context->sprite_right;

    gameBeginningTick = furi_get_tick();
}

static void enemy_spawn(Level* level, GameManager* manager, Vector spawnPosition) {
    int enemyIndex = -1;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance != NULL) continue;
        enemyIndex = i;
        enemies[i].instance = level_add_entity(level, &enemy_desc);
        //FURI_LOG_I("deadzone", "ALLOCATING ENEMY TO MEMORY!");
        enemies[i].jumping = false;
        enemies[i].targetY = WORLD_BORDER_BOTTOM_Y;
        enemies[i].lives = ENEMY_LIVES;
        enemies[i].lastShot = furi_get_tick() + 2000;
        break;
    }

    // Too many enemies
    if(enemyIndex == -1) return;

    // Set enemy position.
    // Depends on your game logic, it can be done in start entity function, but also can be done here.
    entity_pos_set(enemies[enemyIndex].instance, spawnPosition);

    // Add collision box to player entity
    // Box is centered in player x and y, and it's size is 16x16
    entity_collider_add_rect(enemies[enemyIndex].instance, 16, 16);

    // Get enemy context
    PlayerContext* player_context = entity_context_get(enemies[enemyIndex].instance);

    // Load enemy sprite
    player_context->sprite = game_manager_sprite_load(manager, "enemy_left.fxbm");
}

static void player_jump_handler(PlayerContext* playerCtx, Vector* pos, InputState* input) {
    //Initiate jump process (first jump) if we are on ground (and are not jumping)
    if(input->held & GameKeyUp && !jumping && roundf(pos->y) == WORLD_BORDER_BOTTOM_Y) {
        //Start jumping
        jumping = true;

        //Set target Y (based on maximum jump height)
        targetY = CLAMP(pos->y - jumpHeight, WORLD_BORDER_BOTTOM_Y, WORLD_BORDER_TOP_Y);

        //Set player sprite to jumping
        playerCtx->sprite = playerCtx->sprite_jump;
        return;
    }

    //Interpolate to the target jump position
    pos->y = lerp(pos->y, targetY, jumpSpeed);

    //Continuation of jump (until threshold is reached)
    if(jumping) {
        //We've reached our threshold
        if(roundf(pos->y) == roundf(targetY)) {
            //Reset threshold and jumping state
            targetY = WORLD_BORDER_BOTTOM_Y;
            jumping = false;
        }
    }

    if(roundf(pos->y) == WORLD_BORDER_BOTTOM_Y && !jumping &&
       playerCtx->sprite == playerCtx->sprite_jump) {
        playerCtx->sprite = playerCtx->sprite_stand;
    }
}

static void player_shoot_handler(PlayerContext* playerCtx, InputState* input, Vector* pos) {
    //If we are not facing right or left, we cannot shoot
    bool canShoot = playerCtx->sprite != playerCtx->sprite_jump &&
                    playerCtx->sprite != playerCtx->sprite_stand &&
                    playerCtx->sprite != playerCtx->sprite_left_recoil &&
                    playerCtx->sprite != playerCtx->sprite_right_recoil &&
                    playerCtx->sprite != playerCtx->sprite_left_shadowed &&
                    playerCtx->sprite != playerCtx->sprite_right_shadowed;

    uint32_t currentTick = furi_get_tick();
    //Shooting action
    if((input->held & GameKeyOk) && (currentTick - lastShotTick >= shootingRate) && canShoot) {
        lastShotTick = currentTick;
        //Spawn bullet
        int bulletIndex = -1;
        //Find first empty bullet
        for(int i = 0; i < MAX_BULLETS; i++) {
            if(bullets[i] != NULL) continue;
            bulletIndex = i;
            break;
        }

        //Too many bullets in the scene.
        if(bulletIndex == -1) return;

        bullets[bulletIndex] = level_add_entity(gameLevel, &target_desc);
        bulletsDirection[bulletIndex] = playerCtx->sprite == playerCtx->sprite_right ||
                                        playerCtx->sprite == playerCtx->sprite_right_recoil ||
                                        playerCtx->sprite == playerCtx->sprite_right_shadowed;
        float deltaX = bulletsDirection[bulletIndex] ? 10 : -10;
        // Set target position
        Vector bulletPos = (Vector){pos->x + deltaX, pos->y};
        entity_pos_set(bullets[bulletIndex], bulletPos);

        //Handle recoil
        float recoil = 0.0f;
        if(playerCtx->sprite == playerCtx->sprite_right) {
            playerCtx->sprite = playerCtx->sprite_right_recoil;
            recoil -= 7;
        } else if(playerCtx->sprite == playerCtx->sprite_left) {
            playerCtx->sprite = playerCtx->sprite_left_recoil;
            recoil += 7;
        }
        pos->x += recoil;
        targetX += recoil;
    }

    if(furi_get_tick() - lastShotTick > 300) {
        if(playerCtx->sprite == playerCtx->sprite_right_recoil) {
            playerCtx->sprite = playerCtx->sprite_right;
        } else if(playerCtx->sprite == playerCtx->sprite_left_recoil) {
            playerCtx->sprite = playerCtx->sprite_left;
        }
    }

    //Bullet movement mechanics
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(bullets[i]);

        float deltaX = bulletsDirection[i] ? bulletMoveSpeed : -bulletMoveSpeed;
        bulletPos.x =
            CLAMP(bulletPos.x + deltaX, WORLD_BORDER_RIGHT_X + 5, WORLD_BORDER_LEFT_X - 5);
        entity_pos_set(bullets[i], bulletPos);

        //Once bullet reaches end, destroy it
        if(roundf(bulletPos.x) >= WORLD_BORDER_RIGHT_X ||
           roundf(bulletPos.x) <= WORLD_BORDER_LEFT_X) {
            level_remove_entity(gameLevel, bullets[i]);
            bullets[i] = NULL;
        }
    }
}

static void enemy_shoot_handler(Vector* pos, uint32_t* enemyLastShotTick) {
    uint32_t currentTick = furi_get_tick();

    bool playerSpawning = hasSpawnedFirstMob && furi_get_tick() - firstMobSpawnTicks < 8000;
    float shootingRateFactor = playerSpawning ? 30.0F : 1;
    //Shooting action
    if(currentTick - *enemyLastShotTick >= (enemyShootingRate * (shootingRateFactor)) &&
       health != 0) {
        *enemyLastShotTick = currentTick;
        //Spawn bullet
        int bulletIndex = -1;
        //Find first empty bullet
        for(int i = 0; i < MAX_BULLETS; i++) {
            if(enemyBullets[i] != NULL) continue;
            bulletIndex = i;
            break;
        }

        //Too many bullets in the scene.
        if(bulletIndex == -1) return;

        enemyBullets[bulletIndex] = level_add_entity(gameLevel, &target_enemy_desc);
        // Set target position
        Vector bulletPos = (Vector){pos->x - 10, pos->y};
        entity_pos_set(enemyBullets[bulletIndex], bulletPos);
    }

    //Bullet movement mechanics
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(enemyBullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(enemyBullets[i]);
        bulletPos.x = CLAMP(
            bulletPos.x - bulletMoveSpeed, WORLD_BORDER_RIGHT_X + 5, WORLD_BORDER_LEFT_X - 5);
        entity_pos_set(enemyBullets[i], bulletPos);

        //Once bullet reaches end, destroy it
        if(roundf(bulletPos.x) >= WORLD_BORDER_RIGHT_X ||
           roundf(bulletPos.x) <= WORLD_BORDER_LEFT_X) {
            level_remove_entity(gameLevel, enemyBullets[i]);
            enemyBullets[i] = NULL;
        }
    }
}

static void
    tutorial_update(Entity* self, GameManager* manager, PlayerContext* playerCtx, Vector* pos) {
    if(!game_menu_tutorial_selected) return;
    //Completed tutorial and reached the end
    if(tutorialCompleted && roundf(pos->x) == WORLD_BORDER_RIGHT_X) {
        entity_pos_set(self, (Vector){WORLD_TRANSITION_LEFT_STARTING_POINT, pos->y});
        pos->x = WORLD_TRANSITION_LEFT_STARTING_POINT;
        targetX = WORLD_BORDER_LEFT_X;
        startedGame = true;
        playerCtx->sprite = playerCtx->sprite_right;
    }

    if(tutorialCompleted && pos->x < WORLD_TRANSITION_LEFT_ENDING_POINT) {
        //Smooth level transitioning code, hence border is extended in this snippet.
        //(0 -> right border) instead of (left border -> right border)
        pos->x = CLAMP(
            lerp(pos->x, targetX, jumpSpeed),
            WORLD_BORDER_RIGHT_X,
            WORLD_TRANSITION_LEFT_STARTING_POINT);
        pos->y = CLAMP(pos->y, WORLD_BORDER_BOTTOM_Y, WORLD_BORDER_TOP_Y);
        entity_pos_set(self, *pos);

        return;
    }

    if(tutorialCompleted && startedGame && roundf(pos->x) > WORLD_BORDER_LEFT_X) {
        if(!hasSpawnedFirstMob) {
            //Spawn new mob
            enemy_spawn(gameLevel, manager, (Vector){110, 49});
            hasSpawnedFirstMob = true;
            firstMobSpawnTicks = furi_get_tick();
        }
    }
}

static void tutorial_render(GameManager* manager, Canvas* canvas, PlayerContext* player) {
    if(!game_menu_tutorial_selected) return;
    if(kills == 1) {
        if(furi_get_tick() - firstKillTick < 2000) {
            canvas_printf(canvas, 30, 30, "Great job!");
            static bool firstKillMsg;
            if(!firstKillMsg) {
                firstKillMsg = true;
                NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
                notification_message(notifications, &sequence_single_vibro);
                furi_delay_ms(2000);
            }
        } else if(furi_get_tick() - firstKillTick < 7500) {
            canvas_printf(canvas, 20, 40, "You got your first kill!");
            static bool nextMsg;
            if(!nextMsg) {
                nextMsg = true;
                NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
                notification_message(notifications, &sequence_single_vibro);
                furi_delay_ms(2000);
            }
        } else if(!startedGame) {
            if(player->sprite == player->sprite_right) {
                player->sprite = player->sprite_right_shadowed;
            } else if(player->sprite == player->sprite_left) {
                player->sprite = player->sprite_left_shadowed;
            }
            canvas_printf(canvas, 47, 40, "Continue!");
            canvas_printf(canvas, 100, 53, "-->");
            canvas_draw_box(canvas, 126, 44, 2, 16);
            static bool continued;
            if(!continued) {
                continued = true;
                NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
                notification_message(notifications, &sequence_success);
                tutorialCompleted = true;
            }
        }
    } else if(kills == 2) {
        static bool secondKill;
        if(!secondKill) {
            secondKill = true;
            secondKillTick = furi_get_tick();
        }

        if(furi_get_tick() - secondKillTick < 5000) {
            canvas_printf(
                canvas,
                20,
                30,
                health > ((int)roundf(STARTING_PLAYER_HEALTH / 2.0f)) ? "Was that easy?..." :
                                                                        "You seem to struggle?");
            for(int i = 5; i > 0; i--) {
                furi_delay_ms(i);
            }

        } else if(furi_get_tick() - secondKillTick > 7000 && furi_get_tick() - secondKillTick < 9000) {
            if(health > ((int)roundf(STARTING_PLAYER_HEALTH / 2.0f))) {
                canvas_printf(canvas, 5, 30, "Now the final challenge!");
            } else {
                canvas_printf(canvas, 5, 30, "Well, here's the real");
                canvas_printf(canvas, 5, 40, "challenge!");
            }
            for(int i = 5; i > 0; i--) {
                furi_delay_ms(i);
            }
        } else if(furi_get_tick() - secondKillTick > 10000) {
            static bool hasSpawnedFinalBoss1;
            if(!hasSpawnedFinalBoss1) {
                hasSpawnedFinalBoss1 = true;
                enemy_spawn(
                    gameLevel, manager, (Vector){WORLD_BORDER_RIGHT_X - 30, WORLD_BORDER_TOP_Y});
            }
            static bool hasSpawnedFinalBoss2;
            if(furi_get_tick() - secondKillTick > 12000 && !hasSpawnedFinalBoss2) {
                hasSpawnedFinalBoss2 = true;
                enemy_spawn(
                    gameLevel, manager, (Vector){WORLD_BORDER_RIGHT_X, WORLD_BORDER_TOP_Y});
            }
        }
        if(furi_get_tick() - secondKillTick > 8000) {
            int tickThousands = (int)roundf((furi_get_tick() - secondKill) / 1000.0F);
            furi_delay_ms(tickThousands % 3 == 0 ? 10 : 0);
        }
    } else if(kills == 4) {
        canvas_draw_box(canvas, 126, 44, 2, 16);
        //Text will be blinking/flashing
        if((int)roundf((furi_get_tick() - secondKillTick) / 1000.0f) % 2 == 0) {
            canvas_printf(canvas, 20, 30, "You've completed the");
            canvas_printf(canvas, 20, 40, "tutorial!");
        }
    }

    if(tutorialCompleted && startedGame) {
        canvas_draw_box(canvas, 0, 44, 2, 16);
    }

    if(hasSpawnedFirstMob) {
        if(furi_get_tick() - firstMobSpawnTicks < 3000) {
            canvas_printf(canvas, 0, 40, "Welcome to the next level!");
            furi_delay_ms(10);
        } else if(furi_get_tick() - firstMobSpawnTicks < 7000) {
            canvas_printf(canvas, 50, 40, "Fight!");
            for(int i = 5; i > 0; i--) {
                for(int j = 0; j < 5; j++) {
                    furi_delay_ms(i);
                }
            }
        }
    }
}

uint32_t lastSwitchRight = 0;

static void player_update(Entity* self, GameManager* manager, void* context) {
    if(game_menu_quit_selected) return;
    UNUSED(context);

    // Get game input
    InputState input = game_manager_input_get(manager);

    // Get player position
    Vector pos = entity_pos_get(self);

    PlayerContext* playerCtx = context;

    //Player bullet collision
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(enemyBullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(enemyBullets[i]);
        float distXSq = (bulletPos.x - pos.x) * (bulletPos.x - pos.x);
        float distYSq = (bulletPos.y - pos.y) * (bulletPos.y - pos.y);
        float distance = sqrtf(distXSq + distYSq);
        //FURI_LOG_I("deadzone","Enemy bullet dist: %f, %f",(double)sqrtf(distXSq),(double)sqrtf(distYSq));
        if(distance < BULLET_DISTANCE_THRESHOLD ||
           (playerCtx->sprite == playerCtx->sprite_jump && sqrtf(distXSq) < 6 &&
            (distYSq > 0 ? distYSq < 8 : sqrtf(distYSq) < 7))) {
            //Self destruction of bullet and potentially player
            level_remove_entity(gameLevel, enemyBullets[i]);
            enemyBullets[i] = NULL;
            const NotificationSequence* damageSound = &sequence_single_vibro;

            if(--health == 0) {
                //Ran out of lives
                level_remove_entity(gameLevel, self);

                //Replace damage sound with death sound
                damageSound = &sequence_double_vibro;

                //Destroy all associated bullets
                for(int i = 0; i < MAX_BULLETS; i++) {
                    if(bullets[i] == NULL) continue;
                    level_remove_entity(gameLevel, bullets[i]);
                    bullets[i] = NULL;
                }
            }

            //Play sound of getting hit
            NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
            notification_message(notifications, damageSound);
            break;
        }
    }

    tutorial_update(self, manager, playerCtx, &pos);

    //Movement - Game position starts at TOP LEFT.
    //The higher the Y, the lower we move on the screen
    //The higher the X, the more we move toward the right side.
    //Jump Mechanics
    player_jump_handler(playerCtx, &pos, &input);

    //Player Shooting Mechanics
    player_shoot_handler(playerCtx, &input, &pos);

    if(input.held & GameKeyLeft) {
        targetX -= speed;
        pos.x = CLAMP(pos.x - speed, WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);

        //Switch sprite to left direction
        if(playerCtx->sprite != playerCtx->sprite_left_shadowed) {
            playerCtx->sprite = playerCtx->sprite_left;
        }
    }

    if(input.held & GameKeyRight) {
        targetX += speed;
        pos.x = CLAMP(pos.x + speed, WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
        //Switch to right direction
        if(playerCtx->sprite != playerCtx->sprite_right_shadowed) {
            playerCtx->sprite = playerCtx->sprite_right;
        }
    }

    // Clamp player position to screen bounds
    pos.x = CLAMP(lerp(pos.x, targetX, jumpSpeed), WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
    pos.y = CLAMP(pos.y, WORLD_BORDER_BOTTOM_Y, WORLD_BORDER_TOP_Y);

    //Y increases as we go down.
    //X increases as we go right

    // Set new player position
    entity_pos_set(self, pos);

    // Control game exit
    if(input.pressed & GameKeyBack) {
        //Only reaches if player is alive. Send them to the main menu
        game_manager_game_stop(manager);
    }
}

static void player_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    if(game_menu_quit_selected) return;
    // Get player context
    PlayerContext* player = context;

    // Get player position
    Vector pos = entity_pos_get(self);

    // Draw player sprite
    // We subtract 5 from x and y, because collision box is 10x10, and we want to center sprite in it.
    canvas_draw_sprite(canvas, player->sprite, pos.x - 5, pos.y - 5);

    canvas_draw_frame(canvas, 2, -4, 124, 61);
    canvas_draw_line(canvas, 1, 57, 0, 60);
    canvas_draw_line(canvas, 126, 57, 128, 61);

    //canvas_draw_frame(canvas, 0, -4, 135, 64);

    // Get game context
    GameContext* game_context = game_manager_game_context_get(manager);
    UNUSED(game_context);

    // Draw score
    //canvas_printf(canvas, 60, 7, "Enemy Lives: %d", enemies[0].lives);
    canvas_printf(canvas, 80, 7, "Health: %d", health);

    if(kills > 0) {
        canvas_printf(canvas, 10, 7, "Kills: %d", kills);
    }

    tutorial_render(manager, canvas, player);

#ifdef DEBUGGING
    canvas_printf(
        canvas,
        20,
        50,
        "Weights: %.1f, %.1f",
        (double)game_ai_weights[0],
        (double)game_ai_weights[1]);

    canvas_printf(
        canvas, 40, 60, "%.1f, %.1f", (double)game_ai_weights[2], (double)game_ai_weights[3]);
#endif
}

static void enemy_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    if(game_menu_quit_selected) return;
    UNUSED(manager);
    // Get enemy context
    PlayerContext* player = context;

    // Get enemy position
    Vector pos = entity_pos_get(self);

    //Draw enemy health above their head
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance != self) continue;
        canvas_printf(canvas, pos.x + 1, pos.y - 10, "%d", enemies[i].lives);
        break;
    }

    // Draw enemy sprite
    // We subtract 5 from x and y, because collision box is 16x16, and we want to center sprite in it.
    canvas_draw_sprite(canvas, player->sprite, pos.x - 5, pos.y - 5);

    //This must be processed here, as the player object will be deinstantiated

    if(health == STARTING_PLAYER_HEALTH && furi_get_tick() - gameBeginningTick < 5000) {
        canvas_printf(canvas, 30, 20, "Welcome to the");
        canvas_printf(canvas, 30, 36, "DEADZONE tutorial!");
    } else if(health == 0) {
        canvas_printf(canvas, 30, 10, "You're dead.");
        canvas_printf(canvas, 20, 20, "Press Back to respawn.");
    }
}

static void enemy_update(Entity* self, GameManager* manager, void* context) {
    if(game_menu_quit_selected) return;
    UNUSED(context);
    UNUSED(manager);

    Vector pos = entity_pos_get(self);

    //Bullet collision
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(bullets[i]);
        float distXSq = (bulletPos.x - pos.x) * (bulletPos.x - pos.x);
        float distYSq = (bulletPos.y - pos.y) * (bulletPos.y - pos.y);
        float distance = sqrtf(distXSq + distYSq);
        //FURI_LOG_I("deadzone", "Bullet dist: %f", (double)distance);
        if(distance < BULLET_DISTANCE_THRESHOLD) {
            //Self destruction of bullet and entity
            level_remove_entity(gameLevel, bullets[i]);
            bullets[i] = NULL;

            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(enemies[i].instance != self) continue;

                const NotificationSequence* damageSound = &sequence_semi_success;

                if(--enemies[i].lives == 0) {
                    //Ran out of lives
                    level_remove_entity(gameLevel, self);

                    //Replace damage sound with death sound
                    damageSound = &sequence_success;

                    //Destroy all associated bullets
                    //TODO CHECK IF BULLETS WERE THEIRS
                    for(int i = 0; i < MAX_BULLETS; i++) {
                        if(enemyBullets[i] == NULL) continue;
                        level_remove_entity(gameLevel, enemyBullets[i]);
                        enemyBullets[i] = NULL;
                    }
                    enemies[i].instance = NULL;
                    kills++;
                    if(kills == 1) {
                        firstKillTick = furi_get_tick();
                    }

                    if(enemies[i].instance != NULL) {
                        update_weights(
                            game_ai_weights,
                            game_ai_features,
                            enemies[i].jumping ? EnemyActionJump : lastAction,
                            -400 / abs((int)roundf(
                                       entity_pos_get(enemies[i].instance).x -
                                       entity_pos_get(globalPlayer).x)));
                    }
                }

                //Play sound of getting hit
                NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
                notification_message(notifications, damageSound);

                //Don't spawn a new one
                //enemy_spawn(gameLevel, manager);
                break;
            }
            break;
        }
    }

    //Enemy NPC behavior

    uint32_t currentTick = furi_get_tick();
    if(currentTick - lastBehaviorTick > 200) {
        lastBehaviorTick = currentTick;
        Vector playerPos = entity_pos_get(globalPlayer);
        float distXSqToPlayer = (playerPos.x - pos.x) * (playerPos.x - pos.x);
        float distYSqToPlayer = (playerPos.y - pos.y) * (playerPos.y - pos.y);
        float distanceToPlayer = sqrtf(distXSqToPlayer + distYSqToPlayer);

        if(roundf(distanceToPlayer) > 80.0f) {
            update_weights(game_ai_weights, game_ai_features, EnemyActionAttack, -300);
        } else if(roundf(distanceToPlayer) < 45.0f) {
            update_weights(game_ai_weights, game_ai_features, EnemyActionAttack, -300);
        } else if(roundf(distanceToPlayer) > 45.0f) {
            update_weights(game_ai_weights, game_ai_features, EnemyActionAttack, 300);
        }

        if(roundf(pos.y) == WORLD_BORDER_BOTTOM_Y) {
            update_weights(game_ai_weights, game_ai_features, lastAction, 10.0f);
        }

        Vector closestBullet = (Vector){200.0F, 200.0F};
        for(int i = 0; i < MAX_BULLETS; i++) {
            if(bullets[i] == NULL) continue;
            Vector bulletPos = entity_pos_get(bullets[i]);
            float distXSq = (bulletPos.x - pos.x) * (bulletPos.x - pos.x);
            float distYSq = (bulletPos.y - pos.y) * (bulletPos.y - pos.y);
            float distance = sqrtf(distXSq + distYSq);

            float closestBulletDistX = (closestBullet.x - pos.x) * (closestBullet.x - pos.x);
            float closestBulletDistY = (closestBullet.y - pos.y) * (closestBullet.y - pos.y);
            float closestBulletDist = sqrtf(closestBulletDistX + closestBulletDistY);
            if(distance <= closestBulletDist) {
                closestBullet = bulletPos;
            }
        }

        bool jumped = false;
        for(int i = 0; i < MAX_ENEMIES; i++) {
            if(enemies[i].instance != self) continue;
            jumped = enemies[i].jumping;
            break;
        }

        if(closestBullet.x > pos.x && (closestBullet.x - pos.x) < 0.9F) {
            update_weights(
                game_ai_weights,
                game_ai_features,
                jumped ? EnemyActionJump : lastAction,
                500 / abs((int)roundf(pos.x - entity_pos_get(globalPlayer).x)));
        }

        if(closestBullet.y < pos.y && (pos.y - closestBullet.y) < 1.0f) {
            update_weights(
                game_ai_weights,
                game_ai_features,
                EnemyActionJump,
                500 / abs((int)roundf(pos.x - entity_pos_get(globalPlayer).x)));
        }

        enum EnemyAction action = (enum EnemyAction)decide_action(
            pos.x,
            pos.y,
            closestBullet.x,
            closestBullet.y,
            playerPos.x,
            playerPos.y,
            game_ai_weights);

        lastAction = action;
        switch(action) {
        case EnemyActionAttack:

            bool playerSpawning = hasSpawnedFirstMob &&
                                  furi_get_tick() - firstMobSpawnTicks < 9000;
            pos.x = CLAMP(
                lerp(pos.x, playerPos.x + ((pos.x - playerPos.x > 0) ? 30 : -30), enemySpeed),
                WORLD_BORDER_RIGHT_X,
                //Prevent from approaching too much during player spawning
                playerSpawning ? (WORLD_BORDER_RIGHT_X - 14) : WORLD_BORDER_LEFT_X);
            break;
        case EnemyActionRetreat:
            pos.x = CLAMP(
                lerp(pos.x, pos.x + (pos.x - playerPos.x), enemySpeed),
                WORLD_BORDER_RIGHT_X,
                WORLD_BORDER_LEFT_X);
            break;
        case EnemyActionStand:
            break;
        case EnemyActionJump:
            //Prevent from jumping during start of first level (player spawning)
            if(furi_get_tick() - firstMobSpawnTicks < 3000) {
                break;
            }
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(enemies[i].instance != self) continue;
                //Is on ground and can jump
                if(!enemies[i].jumping) {
                    enemies[i].jumping = true;
                    //Enemies have a higher jump height
                    enemies[i].targetY = CLAMP(
                        WORLD_BORDER_BOTTOM_Y - (enemyJumpHeight),
                        WORLD_BORDER_BOTTOM_Y,
                        WORLD_BORDER_TOP_Y);
                    return;
                }
                break;
            }
            break;
        }
    }

    //Gravity & movement interpolation
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance != self) continue;
        pos.y = lerp(pos.y, enemies[i].targetY, jumpSpeed);

        if(enemies[i].jumping && roundf(pos.y) == roundf(enemies[i].targetY)) {
            enemies[i].targetY = WORLD_BORDER_BOTTOM_Y;
        }

        if(enemies[i].jumping && roundf(pos.y) == WORLD_BORDER_BOTTOM_Y) {
            enemies[i].jumping = false;
        }
        break;
    }

    //Enemy shooting
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance != self) continue;
        enemy_shoot_handler(&pos, &enemies[i].lastShot);
        break;
    }

    //Update position of npc
    entity_pos_set(self, pos);

    //Check player input when they're dead, since enemy is alive, processing must proceed here.
    InputState input = game_manager_input_get(manager);
    if(input.pressed & GameKeyBack) {
        if(health == 0) {
            //If dead, restart game
            health = STARTING_PLAYER_HEALTH;
            player_spawn(gameLevel, manager);
            entity_pos_set(globalPlayer, (Vector){WORLD_BORDER_LEFT_X, WORLD_BORDER_BOTTOM_Y});
        }
    }
}

static const EntityDescription player_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = player_update, // called every frame
    .render = player_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size =
        sizeof(PlayerContext), // size of entity context, will be automatically allocated and freed
};

static const EntityDescription enemy_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = enemy_update, // called every frame
    .render = enemy_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size =
        sizeof(PlayerContext), // size of entity context, will be automatically allocated and freed
};

/****** Entities: Target ******/

static void target_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    if(game_menu_quit_selected) return;
    UNUSED(context);
    UNUSED(manager);

    // Get target position
    Vector pos = entity_pos_get(self);

    // Draw target
    canvas_draw_disc(canvas, pos.x, pos.y, 2);
}

static const EntityDescription target_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = NULL, // called every frame
    .render = target_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size = 0, // size of entity context, will be automatically allocated and freed
};

static const EntityDescription target_enemy_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = NULL, // called every frame
    .render = target_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size = 0, // size of entity context, will be automatically allocated and freed
};

static void tutorial_level_alloc(Level* level, GameManager* manager) {
    if(!game_menu_tutorial_selected) return;
    //Add enemy to level
    if(firstKillTick == 0) enemy_spawn(level, manager, (Vector){110, 49});
}

/****** Level ******/

static void level_alloc(Level* level, GameManager* manager, void* context) {
    if(game_menu_quit_selected) return;
    UNUSED(manager);
    UNUSED(context);

    // Add player entity to the level
    player_spawn(level, manager);

    tutorial_level_alloc(level, manager);

    // Add target entity to the level
    //level_add_entity(level, &target_desc);

    //level_add_entity(level, &target_enemy_desc);

    gameLevel = level;
}

/*
    Alloc/free is called once, when level is added/removed from the game. 
    It useful if you have small amount of levels and entities, that can be allocated at once.

    Start/stop is called when level is changed to/from this level.
    It will save memory, because you can allocate entities only for current level.
*/
static const LevelBehaviour level = {
    .alloc = level_alloc, // called once, when level allocated
    .free = NULL, // called once, when level freed
    .start = NULL, // called when level is changed to this level
    .stop = NULL, // called when level is changed from this level
    .context_size = 0, // size of level context, will be automatically allocated and freed
};

/****** Game ******/

static void game_start_post_menu(GameManager* game_manager, void* ctx) {
    // Do some initialization here, for example you can load score from storage.
    // For simplicity, we will just set it to 0.
    GameContext* game_context = ctx;
    game_context->score = 0;

    globalGameManager = game_manager;

    // Add level to the game
    game_manager_add_level(game_manager, &level);

    //Instantiate all bullets

    for(int i = 0; i < MAX_BULLETS; i++) {
        bullets[i] = NULL;
        enemyBullets[i] = NULL;
    }

    //TODO ANIMATIONS FOR GAME, such as particles going up like fire on player
    //TODO walk animation, optional shooting animation
}

/* 
    Write here the start code for your game, for example: creating a level and so on.
    Game context is allocated (game.context_size) and passed to this function, you can use it to store your game data.
*/
static void game_start(GameManager* game_manager, void* ctx) {
    UNUSED(game_manager);
    if(game_menu_quit_selected) return;

    //Instantiate the lock
    game_menu_open(game_manager, false);

    if(game_menu_settings_selected) {
        // End access to the GUI API.
        //furi_record_close(RECORD_GUI);

        Gui* gui = furi_record_open(RECORD_GUI);

        Submenu* submenu = submenu_alloc();
        submenu_add_item(submenu, "A", 0, game_settings_menu_button_callback, game_manager);
        submenu_add_item(submenu, "B", 1, game_settings_menu_button_callback, game_manager);
        submenu_add_item(submenu, "C", 2, game_settings_menu_button_callback, game_manager);
        submenu_add_item(submenu, "BACK", 3, game_settings_menu_button_callback, game_manager);
        ViewHolder* view_holder = view_holder_alloc();
        view_holder_set_view(view_holder, submenu_get_view(submenu));
        view_holder_attach_to_gui(view_holder, gui);
        api_lock_wait_unlock(game_menu_exit_lock);
    }

    game_start_post_menu((game_manager), ctx);

    if(game_menu_quit_selected) {
        game_manager_game_stop(game_manager);
    }
}

/* 
    Write here the stop code for your game, for example: freeing memory, if it was allocated.
    You don't need to free level, sprites or entities, it will be done automatically.
    Also, you don't need to free game_context, it will be done automatically, after this function.
*/

static void game_stop(void* ctx);

/*
    Yor game configuration, do not rename this variable, but you can change it's content here.  
*/
const Game game = {
    .target_fps = 130, // target fps, game will try to keep this value
    .show_fps = false, // show fps counter on the screen
    .always_backlight = false, // keep display backlight always on
    .start = game_start, // will be called once, when game starts
    .stop = game_stop, // will be called once, when game stops
    .context_size = sizeof(GameContext), // size of game context
};

int32_t relaunch_game() {
    GameManager* game_manager = game_manager_alloc();

    GameEngineSettings settings = game_engine_settings_init();
    settings.target_fps = game.target_fps;
    settings.show_fps = game.show_fps;
    settings.always_backlight = game.always_backlight;
    settings.frame_callback = frame_cb;
    settings.context = game_manager;

    GameEngine* engine = game_engine_alloc(settings);
    game_manager_engine_set(game_manager, engine);

    void* game_context = NULL;
    if(game.context_size > 0) {
        game_context = malloc(game.context_size);
        game_manager_game_context_set(game_manager, game_context);
    }
    game_start_post_menu(game_manager, game_context);
    game_engine_run(engine);
    game_engine_free(engine);

    game_manager_free(game_manager);

    game_stop(game_context);
    if(game_context) {
        free(game_context);
    }
    return 0;
}

static void game_stop(void* ctx) {
    //Leave immediately if they want to quit.
    if(game_menu_quit_selected) {
        return;
    }
    // Do some deinitialization here, for example you can save score to storage.
    // For simplicity, we will just print it.

    Gui* gui = furi_record_open(RECORD_GUI);
    Submenu* submenu = submenu_alloc();
    submenu_add_item(submenu, "RESUME GAME", 0, game_menu_button_callback, ctx);
    submenu_add_item(
        submenu,
        game_menu_tutorial_selected ? "RESUME TUTORIAL" : "TUTORIAL",
        1,
        game_menu_button_callback,
        ctx);
    submenu_add_item(submenu, "SETTINGS", 2, game_menu_button_callback, ctx);
    submenu_add_item(submenu, "QUIT", 3, game_menu_button_callback, ctx);
    ViewHolder* view_holder = view_holder_alloc();
    view_holder_set_view(view_holder, submenu_get_view(submenu));
    view_holder_attach_to_gui(view_holder, gui);
    api_lock_wait_unlock(game_menu_exit_lock);

    if(game_menu_settings_selected) {
        // End access to the GUI API.
        //furi_record_close(RECORD_GUI);

        Gui* gui = furi_record_open(RECORD_GUI);

        Submenu* submenu = submenu_alloc();
        submenu_add_item(submenu, "A", 0, game_settings_menu_button_callback, globalGameManager);
        submenu_add_item(submenu, "B", 1, game_settings_menu_button_callback, globalGameManager);
        submenu_add_item(submenu, "C", 2, game_settings_menu_button_callback, globalGameManager);
        submenu_add_item(
            submenu, "BACK", 3, game_settings_menu_button_callback, globalGameManager);
        ViewHolder* view_holder = view_holder_alloc();
        view_holder_set_view(view_holder, submenu_get_view(submenu));
        view_holder_attach_to_gui(view_holder, gui);
        api_lock_wait_unlock(game_menu_exit_lock);
    }

    //Do they want to quit? Or do we relaunch
    if(!game_menu_quit_selected) {
        relaunch_game();
    } else {
        view_holder_set_view(view_holder, NULL);
        // Delete everything to prevent memory leaks.
        view_holder_free(view_holder);
        submenu_free(submenu);
        // End access to the GUI API.
        furi_record_close(RECORD_GUI);
    }
}
