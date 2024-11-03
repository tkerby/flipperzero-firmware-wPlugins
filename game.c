#include "game.h"

/****** Entities: Player ******/

//Useful for the reinforcement learning
enum EnemyAction {
    EnemyActionAttack,
    EnemyActionRetreat,
    EnemyActionStand,
    EnemyActionJump
};

//Enemy structure
typedef struct {
    Entity* instance;
    bool jumping;
    float targetY;
    int lives;
    uint32_t spawnTime;
    uint32_t mercyTicks;
    uint32_t lastShot;
} Enemy;

// Forward declaration of player_desc, because it's used in player_spawn function.

/* Begin of variables from header */

//Configurable values
uint32_t shootingDelay = 1700;
uint32_t enemyShootingDelay = 1500;
float bulletMoveSpeed = 0.55f;
float speed = 0.6f;
float enemySpeed = 0.13f;
float jumpHeight = 22.0F;
float jumpSpeed = 0.074f;
float enemyJumpHeight = 22.0F + 10.0F;
//Tracking player data
uint32_t kills = 0;
bool jumping = false;
float targetY = WORLD_BORDER_BOTTOM_Y;
float targetX = 10.0F;
//Internal vars
int firstMobSpawnTicks = 0;
//While debugging we increase all lives for longer testing/gameplay.
#ifdef DEBUGGING
int ENEMY_LIVES = 30;
int health = 100;
#else
int ENEMY_LIVES = STARTING_ENEMY_HEALTH;
int health = STARTING_PLAYER_HEALTH;
#endif
uint32_t firstKillTick = 0;
uint32_t secondKillTick = 0;
uint32_t gameBeginningTick = 0;
Level* gameLevel = NULL;

/* End of header variables */

//Enemy, and bullet storage
#define MAX_ENEMIES 30
Enemy enemies[MAX_ENEMIES];

#define MAX_BULLETS 30
Entity* bullets[MAX_BULLETS];
bool bulletsDirection[MAX_BULLETS];

Entity* enemyBullets[MAX_BULLETS];
bool enemyBulletsDirection[MAX_BULLETS];

Entity* globalPlayer = NULL;

//Internal variables
static const EntityDescription player_desc;
static const EntityDescription target_desc;
static const EntityDescription enemy_desc;
static const EntityDescription target_enemy_desc;

//More internal variables
uint32_t lastShotTick;
uint32_t lastBehaviorTick;

enum EnemyAction lastAction = EnemyActionStand;

GameManager* globalGameManager;

float lerp(float y1, float y2, float t) {
    return y1 + t * (y2 - y1);
}

static void frame_cb(GameEngine* engine, Canvas* canvas, InputState input, void* context) {
    UNUSED(engine);
    GameManager* game_manager = context;
    game_manager_input_set(game_manager, input);
    game_manager_update(game_manager);
    game_manager_render(game_manager, canvas);
}
/*
static Vector random_pos(void) {
    return (Vector){rand() % 120 + 4, rand() % 58 + 4};
}
*/

void player_spawn(Level* level, GameManager* manager) {
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

void enemy_spawn(
    Level* level,
    GameManager* manager,
    Vector spawnPosition,
    uint32_t mercyTicks,
    bool right) {
    int enemyIndex = -1;
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance != NULL) continue;
        enemyIndex = i;
        enemies[i].instance = level_add_entity(level, &enemy_desc);
        //FURI_LOG_I("deadzone", "ALLOCATING ENEMY TO MEMORY!");
        enemies[i].jumping = false;
        enemies[i].targetY = WORLD_BORDER_BOTTOM_Y;
        enemies[i].lives = ENEMY_LIVES;
        enemies[i].spawnTime = furi_get_tick();
        enemies[i].mercyTicks = mercyTicks;
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
    player_context->sprite_left = game_manager_sprite_load(manager, "enemy_left.fxbm");
    player_context->sprite_right = game_manager_sprite_load(manager, "enemy_right.fxbm");
    if(right) {
        player_context->sprite = player_context->sprite_right;
    } else {
        player_context->sprite = player_context->sprite_left;
    }
}

void player_jump_handler(PlayerContext* playerCtx, Vector* pos, InputState* input) {
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

void player_shoot_handler(PlayerContext* playerCtx, InputState* input, Vector* pos) {
    //If we are not facing right or left, we cannot shoot
    bool canShoot = playerCtx->sprite != playerCtx->sprite_jump &&
                    playerCtx->sprite != playerCtx->sprite_stand &&
                    playerCtx->sprite != playerCtx->sprite_left_recoil &&
                    playerCtx->sprite != playerCtx->sprite_right_recoil &&
                    playerCtx->sprite != playerCtx->sprite_left_shadowed &&
                    playerCtx->sprite != playerCtx->sprite_right_shadowed;

    uint32_t currentTick = furi_get_tick();
    //Shooting action
    if((input->held & GameKeyOk) && (currentTick - lastShotTick >= shootingDelay) && canShoot) {
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

void enemy_shoot_handler(Enemy* enemy, Vector* pos, uint32_t* enemyLastShotTick, void* context) {
    PlayerContext* enemyCtx = context;
    uint32_t currentTick = furi_get_tick();

    bool gracePeriod = furi_get_tick() < (enemy->spawnTime + enemy->mercyTicks);
    float shootingRateFactor = gracePeriod ? 45.0F : 1;
    //Shooting action
    if(currentTick - *enemyLastShotTick >= (enemyShootingDelay * (shootingRateFactor)) &&
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
        Vector bulletPos;
        enemyBulletsDirection[bulletIndex] = enemyCtx->sprite == enemyCtx->sprite_right;
        if(enemyBulletsDirection[bulletIndex]) {
            bulletPos = (Vector){pos->x + 10, pos->y};
        } else {
            bulletPos = (Vector){pos->x - 10, pos->y};
        }
        entity_pos_set(enemyBullets[bulletIndex], bulletPos);
    }

    //Bullet movement mechanics
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(enemyBullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(enemyBullets[i]);
        //TODO Currently movement is dependent on enemy's trajectory, no metadata of bullet.
        float deltaX = enemyBulletsDirection[i] ? bulletMoveSpeed : -bulletMoveSpeed;
        bulletPos.x =
            CLAMP(bulletPos.x + deltaX, WORLD_BORDER_RIGHT_X + 5, WORLD_BORDER_LEFT_X - 5);
        entity_pos_set(enemyBullets[i], bulletPos);

        //Once bullet reaches end, destroy it
        if(roundf(bulletPos.x) >= WORLD_BORDER_RIGHT_X ||
           roundf(bulletPos.x) <= WORLD_BORDER_LEFT_X) {
            level_remove_entity(gameLevel, enemyBullets[i]);
            enemyBullets[i] = NULL;
        }
    }
}

uint32_t lastSwitchRight = 0;

void player_update(Entity* self, GameManager* manager, void* context) {
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

    game_level_player_update(self, manager, playerCtx, &pos);
    tutorial_level_player_update(self, manager, playerCtx, &pos);

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

void player_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
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

    game_level_player_render(manager, canvas, player);
    tutorial_level_player_render(manager, canvas, player);

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

void enemy_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
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

    game_level_enemy_render(manager, canvas, context);
    game_level_player_render(manager, canvas, context);

    if(health == 0) {
        canvas_printf(canvas, 30, 10, "You're dead.");
        canvas_printf(canvas, 20, 20, "Press Back to respawn.");
    }
}

void enemy_update(Entity* self, GameManager* manager, void* context) {
    if(game_menu_quit_selected) return;
    PlayerContext* enemyCtx = context;
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
            Enemy* enemy = NULL;
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(enemies[i].instance != self) continue;
                enemy = &enemies[i];
                break;
            }

            bool gracePeriod = furi_get_tick() < (enemy->spawnTime + enemy->mercyTicks);

            if(enemyCtx->sprite == enemyCtx->sprite_left) {
                pos.x = CLAMP(
                    lerp(pos.x, playerPos.x + ((pos.x - playerPos.x > 0) ? 30 : -30), enemySpeed),
                    WORLD_BORDER_RIGHT_X,
                    //Prevent from approaching too much during player spawning / grace period
                    gracePeriod ? (WORLD_BORDER_RIGHT_X - 14) : WORLD_BORDER_LEFT_X);
            } else {
                pos.x = CLAMP(
                    lerp(pos.x, pos.x + (pos.x - playerPos.x), enemySpeed),
                    WORLD_BORDER_RIGHT_X,
                    WORLD_BORDER_LEFT_X);
                break;
            }
            break;
        case EnemyActionRetreat:
            pos.x = CLAMP(
                lerp(
                    pos.x,
                    pos.x + (enemyCtx->sprite == enemyCtx->sprite_left ? (pos.x - playerPos.x) :
                                                                         (playerPos.x - pos.x)),
                    enemySpeed),
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
        enemy_shoot_handler(&enemies[i], &pos, &enemies[i].lastShot, context);
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
    if(firstKillTick == 0) enemy_spawn(level, manager, (Vector){110, 49}, 3000, false);
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

        view_holder_set_view(view_holder, NULL);
        // Delete everything to prevent memory leaks.
        view_holder_free(view_holder);
        submenu_free(submenu);
        furi_record_close(RECORD_GUI);
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

//Declare method
int32_t relaunch_game();

static void game_stop(void* ctx) {
    //Leave immediately if they want to quit.
    if(game_menu_quit_selected) {
        return;
    }
    // Do some deinitialization here, for example you can save score to storage.
    // For simplicity, we will just print it.

    game_menu_exit_lock = api_lock_alloc_locked();
    Gui* gui = furi_record_open(RECORD_GUI);
    Submenu* submenu = submenu_alloc();
    submenu_add_item(
        submenu,
        game_menu_game_selected ? "RESUME GAME" : "GAME",
        0,
        game_menu_button_callback,
        ctx);
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
    api_lock_wait_unlock_and_free(game_menu_exit_lock);

    if(game_menu_settings_selected) {
        //Clear previous menu
        view_holder_set_view(view_holder, NULL);
        // Delete everything to prevent memory leaks.
        view_holder_free(view_holder);
        submenu_free(submenu);
        furi_record_close(RECORD_GUI);

        game_menu_exit_lock = api_lock_alloc_locked();
        Gui* gui = furi_record_open(RECORD_GUI);

        Submenu* submenu = submenu_alloc();
        submenu_add_item(submenu, "A", 0, game_settings_menu_button_callback, globalGameManager);
        submenu_add_item(submenu, "B", 1, game_settings_menu_button_callback, globalGameManager);
        submenu_add_item(submenu, "C", 2, game_settings_menu_button_callback, globalGameManager);
        submenu_add_item(
            submenu, "BACK", 3, game_settings_menu_button_callback, globalGameManager);
        view_holder = view_holder_alloc();
        view_holder_set_view(view_holder, submenu_get_view(submenu));
        view_holder_attach_to_gui(view_holder, gui);
        api_lock_wait_unlock_and_free(game_menu_exit_lock);
        furi_record_close(RECORD_GUI);
    }

    //Do they want to quit? Or do we relaunch
    if(!game_menu_quit_selected) {
        //Clear previous menu
        view_holder_set_view(view_holder, NULL);
        // Delete everything to prevent memory leaks.
        view_holder_free(view_holder);
        submenu_free(submenu);
        furi_record_close(RECORD_GUI);
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
