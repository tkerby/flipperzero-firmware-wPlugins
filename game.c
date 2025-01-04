#include "game.h"

/****** Entities: Player ******/

//Useful for the reinforcement learning
enum EnemyAction {
    EnemyActionAttack,
    EnemyActionRetreat,
    EnemyActionStand,
    EnemyActionJump
};

Enemy enemies[MAX_ENEMIES];

#define MAX_BULLETS 30
Entity* bullets[MAX_BULLETS];
bool bulletsDirection[MAX_BULLETS];

Entity* enemyBullets[MAX_BULLETS];
bool enemyBulletsDirection[MAX_BULLETS];

// Forward declaration of player_desc, because it's used in player_spawn function.

/* Begin of variables from header */

//Configurable values
//TODO Reloading with faster shooting.

#define DEBUGGING

#define TRAINING_RESET_VALUE 100
#ifdef DEBUGGING
uint32_t shootingDelay = 6500;
#else
uint32_t shootingDelay = 500;
#endif
#ifdef DEBUGGING
uint32_t enemyShootingDelay = 6000;
#else
uint32_t enemyShootingDelay = 1500;
#endif
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
bool horizontalGame = true;
bool horizontalView = true;
int16_t transitionLeftTicks = 0;
int16_t transitionRightTicks = 0;
//Internal vars
int firstMobSpawnTicks = 0;
//While debugging we increase all lives for longer testing/gameplay.
#define LEARNING_RATE 0.05f

#ifdef DEBUGGING
int ENEMY_LIVES = 100;
int health = 200;
#else
int ENEMY_LIVES = STARTING_ENEMY_HEALTH;
int health = STARTING_PLAYER_HEALTH;
#endif
uint32_t firstKillTick = 0;
uint32_t secondKillTick = 0;
uint32_t gameBeginningTick = 0;
Level* gameLevel = NULL;
GameManager* globalGameManager = NULL;

/* End of header variables */

//Enemy, and bullet storage

Entity* globalPlayer = NULL;

//Internal variables
//static const EntityDescription player_desc;
static const EntityDescription target_desc;
//static const EntityDescription enemy_desc;
static const EntityDescription target_enemy_desc;
static const EntityDescription global_desc;

//More internal variables
uint32_t lastShotTick;
uint32_t lastBehaviorTick;

enum EnemyAction lastAction = EnemyActionStand;

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

void featureCalculation(Entity* enemy, double* features) {
    //Pre processing
    Vector enemyPos = entity_pos_get(enemy);
    Vector closestBullet = (Vector){-1.0F, -1.0F};
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(bullets[i]);
        float distXSq = (bulletPos.x - enemyPos.x) * (bulletPos.x - enemyPos.x);
        float distYSq = (bulletPos.y - enemyPos.y) * (bulletPos.y - enemyPos.y);
        float distance = sqrtf(distXSq + distYSq);

        float closestBulletDistX = (closestBullet.x - enemyPos.x) * (closestBullet.x - enemyPos.x);
        float closestBulletDistY = (closestBullet.y - enemyPos.y) * (closestBullet.y - enemyPos.y);
        float closestBulletDist = sqrtf(closestBulletDistX + closestBulletDistY);
        if(distance <= closestBulletDist) {
            closestBullet = bulletPos;
        }
    }

    float distToBullet = fabs(enemyPos.x - closestBullet.x);
    UNUSED(distToBullet);
    float distToPlayer = fabs(entity_pos_get(globalPlayer).x - enemyPos.x);
    /*return (Vector){
        distToBullet < 25 && closestBullet.x != -1 ? 0.5 : -0.5,
        distToPlayer > 90 ? 0.5 :
        distToPlayer < 65 ? -0.5 :
                            0};*/
    features[0] = closestBullet.x;
    features[1] = closestBullet.y;
    features[2] = enemyPos.x;
    features[3] = enemyPos.y;
    features[4] = distToPlayer;
}

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
    player_context->sprite_forward = game_manager_sprite_load(manager, "player_forward.fxbm");

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
        //FURI_LOG_I("deadzone", "PRE ALLOCATION!");
        enemies[i].instance = level_add_entity(level, &enemy_desc);
        enemies[i].direction = right;
        //FURI_LOG_I("deadzone", "ALLOCATING ENEMY TO MEMORY!");
        enemies[i].jumping = false;
        enemies[i].targetY = WORLD_BORDER_BOTTOM_Y;
        enemies[i].lives = ENEMY_LIVES;
        enemies[i].spawnTime = furi_get_tick();
        enemies[i].mercyTicks = mercyTicks;
        enemies[i].lastShot = furi_get_tick() + 2000;
        enemies[i].ai = genann_init(5, 2, 15, 2);

        double weights[362] = {
            0.226409,  0.268792,  0.462289,  0.107297,  -0.006758, -0.491368, -0.451068, -0.267161,
            -0.285436, -0.285914, 0.333097,  -0.362451, -0.212848, 0.395701,  -0.290229, 0.490438,
            -0.042899, 0.451248,  -0.431080, 0.186390,  -0.063223, -0.509010, 0.007649,  0.095175,
            -0.414880, 0.099654,  -0.035782, -0.396215, 0.338234,  -0.494474, -0.222023, -0.091676,
            -0.335384, -0.444136, -0.377813, 0.444023,  -0.364012, -0.400617, 0.450545,  0.127066,
            -0.229777, -0.346571, -0.066315, 0.751485,  -0.178302, 0.339927,  -0.122488, 0.456789,
            -0.134740, 0.107734,  -0.335763, -1.116590, -0.135493, -0.368667, 0.040381,  -0.088017,
            -0.424452, -0.175063, 0.297495,  0.450491,  0.089436,  0.419278,  -0.271802, 0.213183,
            -0.017692, 0.010840,  -0.317622, 0.051958,  -0.557679, -0.392417, -0.785579, -1.219056,
            0.292143,  -0.101738, 0.105616,  -0.226466, -0.398386, 0.065294,  -0.448426, -0.103761,
            0.198751,  -0.347099, 0.294456,  0.092711,  -0.383366, 0.212600,  0.042372,  -0.504524,
            0.067993,  -0.185861, 1.103913,  -0.660966, 0.346752,  -0.664331, -0.474754, 0.455617,
            0.300919,  0.026157,  -0.189773, 0.342382,  -1.085806, -0.851300, 0.302623,  -0.219559,
            -0.027721, 0.139915,  0.852105,  -2.268763, 0.054006,  -0.641483, -0.331565, 0.059930,
            -0.403168, -0.116639, 0.624315,  -0.055035, -0.496349, -0.175561, -0.020988, -0.270041,
            -0.256507, -0.308748, 0.824737,  -1.548700, -0.494925, -0.578608, -0.105978, -0.019450,
            -0.442901, 0.124253,  0.081015,  0.129442,  -0.474773, -0.819088, 0.405788,  -0.444668,
            -0.155328, 0.408472,  0.671445,  -2.380961, -0.195457, -0.126481, -0.168835, 0.100084,
            -0.034022, 0.333235,  -0.108913, 0.356546,  -0.339009, -0.034980, 0.118545,  0.478602,
            -0.166463, 0.233579,  1.076242,  -0.359646, 0.479183,  -0.563340, -0.279116, 0.368250,
            -0.139772, 0.357644,  -0.319847, -0.165932, -0.960265, -1.260443, -0.421939, 0.234696,
            0.152264,  0.482531,  1.264831,  -0.796035, -0.145103, -1.076213, -0.228577, 0.193847,
            -0.403165, -0.098832, 0.135107,  -0.094424, -0.885660, -0.590713, -0.360195, -0.388541,
            -0.043622, 0.086037,  0.359798,  -1.392439, 0.438639,  -0.800281, 0.269881,  0.320851,
            0.324700,  0.225588,  -0.349118, 0.255533,  -0.358827, -0.945519, -0.342831, 0.414656,
            0.258488,  0.207167,  0.156535,  -1.323227, -0.287665, -0.527171, -0.218238, -0.380022,
            -0.032190, 0.447766,  -0.418408, -0.020913, -0.954437, -0.838485, 0.317570,  0.106358,
            0.068577,  -0.085219, 1.146716,  -0.722223, -0.016367, -1.049473, -0.472906, 0.408251,
            -0.369407, -0.259705, -0.340264, -0.498245, -0.691266, -0.558752, 0.157543,  0.492918,
            0.308035,  0.127870,  0.917246,  -0.017343, -0.121023, -1.146187, 0.494160,  0.278263,
            0.203987,  -0.419360, -0.385779, -0.387958, -1.028847, -0.872891, 0.262359,  -0.362893,
            -0.194513, -0.114334, 0.931189,  -0.722764, -0.319995, -0.527780, -0.068619, -0.425929,
            0.256320,  0.480641,  -0.732865, 0.136107,  -1.242456, -0.391987, -0.120990, -0.472046,
            0.085961,  0.038079,  0.462364,  -1.623666, 0.170796,  -0.599832, -0.205591, -0.380911,
            -0.064044, -0.080865, 0.213942,  -0.158855, -0.840867, -0.818363, 0.374552,  -0.077628,
            0.214637,  -0.395527, 0.971118,  -0.642512, -0.220213, -0.966670, 0.289494,  0.239901,
            -0.053469, 0.456824,  0.156758,  0.012216,  -1.044596, -1.053266, 0.265781,  -0.052867,
            0.171821,  0.164914,  0.233097,  -2.733220, -0.158545, -0.448137, -0.329886, 0.240294,
            0.368689,  0.104424,  0.834357,  0.435598,  0.218894,  -0.276858, 0.156540,  -0.411588,
            0.359536,  -0.362781, 1.313216,  -0.419396, 0.267775,  -0.629433, -0.327155, -0.063460,
            -0.165845, 0.303552,  -0.161947, -0.483245, -0.863209, -1.157557, -0.031172, 0.262868,
            -0.424300, 0.257120,  0.906641,  0.104407,  0.338958,  0.202972,  0.372778,  0.015913,
            0.068704,  0.177207,  0.173126,  0.103119,  -0.020823, 0.125812,  0.223043,  0.060970,
            0.652505,  0.033537,  1.390213,  0.879808,  3.005649,  1.686152,  3.203962,  0.257860,
            0.725549,  1.387875,  1.385051,  0.786731,  -0.008892, 0.815875,  1.736890,  0.715506,
            4.206503,  0.408027};

        for(int j = 0; j < enemies[i].ai->total_weights; j++) {
            enemies[i].ai->weight[j] = weights[j];
        }
        FURI_LOG_I("DEADZONE", "Loaded the weights!");
        //genann_randomize(enemies[i].ai);
        break;
    }

    // Too many enemies
    if(enemyIndex == -1) return;

    // Set enemy position.
    // Depends on your game logic, it can be done in start entity function, but also can be done here.
    entity_pos_set(enemies[enemyIndex].instance, spawnPosition);
    //FURI_LOG_I("deadzone", "SET ENEMY POS");

    // Add collision box to player entity
    // Box is centered in player x and y, and it's size is 16x16
    entity_collider_add_rect(enemies[enemyIndex].instance, 16, 16);
    //FURI_LOG_I("deadzone", "SET ENEMY RECT");

    // Get enemy context
    PlayerContext* player_context = entity_context_get(enemies[enemyIndex].instance);
    //FURI_LOG_I("deadzone", "GET PLAYER CTX FOR ENEMY");

    // Load enemy sprite
    player_context->sprite_left = game_manager_sprite_load(manager, "enemy_left.fxbm");
    player_context->sprite_right = game_manager_sprite_load(manager, "enemy_right.fxbm");
    if(right) {
        player_context->sprite = player_context->sprite_right;
    } else {
        player_context->sprite = player_context->sprite_left;
    }
    //FURI_LOG_I("deadzone", "DONE SPAWNING");
}

void player_jump_handler(PlayerContext* playerCtx, Vector* pos, InputState* input) {
    Sprite* jumpingSprite = horizontalGame ? playerCtx->sprite_jump : playerCtx->sprite_forward;
    Sprite* standingSprite = horizontalGame ? playerCtx->sprite_stand : playerCtx->sprite_forward;

    //Initiate jump process (first jump) if we are on ground (and are not jumping)
    if(input->held & GameKeyUp && !jumping && roundf(pos->y) == WORLD_BORDER_BOTTOM_Y) {
        //Start jumping
        jumping = true;

        //Set target Y (based on maximum jump height)
        targetY = CLAMP(pos->y - jumpHeight, WORLD_BORDER_BOTTOM_Y, WORLD_BORDER_TOP_Y);

        //Set player sprite to jumping
        //playerCtx->sprite = jumpingSprite;
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

    if(roundf(pos->y) == WORLD_BORDER_BOTTOM_Y && !jumping && playerCtx->sprite == jumpingSprite) {
        playerCtx->sprite = standingSprite;
    }
}

void player_shoot_handler(PlayerContext* playerCtx, InputState* input, Vector* pos) {
    UNUSED(input);
    //If we are not facing right or left, we cannot shoot
    bool canShoot = playerCtx->sprite != playerCtx->sprite_jump &&
                    playerCtx->sprite != playerCtx->sprite_stand &&
                    playerCtx->sprite != playerCtx->sprite_left_recoil &&
                    playerCtx->sprite != playerCtx->sprite_right_recoil &&
                    playerCtx->sprite != playerCtx->sprite_left_shadowed &&
                    playerCtx->sprite != playerCtx->sprite_right_shadowed &&
                    playerCtx->sprite !=
                        playerCtx->sprite_forward; //TODO For now, can't shoot forward

    uint32_t currentTick = furi_get_tick();
    //Shooting action
    if(/*(input->held & GameKeyOk) && */ (currentTick - lastShotTick >= shootingDelay) &&
       canShoot) {
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
}

uint32_t lastSwitchRight = 0;

bool damage_player(Entity* self) {
    const NotificationSequence* damageSound = &sequence_single_vibro;

    if(--health == 0) {
        //Ran out of lives
        level_remove_entity(gameLevel, self);

        //Replace damage sound with death sound
        damageSound = &sequence_double_vibro;

        /*
                //Destroy all associated bullets
                for(int i = 0; i < MAX_BULLETS; i++) {
                    if(bullets[i] == NULL) continue;
                    level_remove_entity(gameLevel, bullets[i]);
                    bullets[i] = NULL;
                }*/
    }

    //Play sound of getting hit
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notifications, damageSound);
    notification_message(notifications, &sequence_blink_red_100);
    return health == 0;
}

int trainCount = 1000;

double err = 100;
double lastErr = 0;

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
            damage_player(self);
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

    if(input.held & GameKeyLeft && playerCtx->sprite != playerCtx->sprite_left_recoil) {
        if(playerCtx->sprite == playerCtx->sprite_right_shadowed) {
            transitionLeftTicks = TRANSITION_FRAMES;
        }
        //Switch sprite to left direction
        if((playerCtx->sprite != playerCtx->sprite_left_shadowed &&
            playerCtx->sprite != playerCtx->sprite_right_shadowed &&
            playerCtx->sprite != playerCtx->sprite_left)) {
            if(playerCtx->sprite == playerCtx->sprite_stand) {
                //transitionLeftTicks++;
            }
            if(transitionLeftTicks < TRANSITION_FRAMES) {
                transitionLeftTicks++;
                if(playerCtx->sprite != playerCtx->sprite_stand &&
                   playerCtx->sprite != playerCtx->sprite_jump) {
                    playerCtx->sprite = playerCtx->sprite_forward;
                }
            } else if(
                playerCtx->sprite != playerCtx->sprite_left_shadowed &&
                transitionLeftTicks >= TRANSITION_FRAMES) {
                transitionLeftTicks = 0;
                if(playerCtx->sprite == playerCtx->sprite_right_shadowed) {
                    playerCtx->sprite = playerCtx->sprite_left_shadowed;
                } else {
                    playerCtx->sprite = playerCtx->sprite_left;
                }
            }
        }

        if(playerCtx->sprite == playerCtx->sprite_left_shadowed ||
           playerCtx->sprite == playerCtx->sprite_left) {
            targetX -= speed;
            targetX = CLAMP(targetX, WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
            pos.x = CLAMP(pos.x - speed, WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
        }
    }

    if(input.held & GameKeyRight && playerCtx->sprite != playerCtx->sprite_right_recoil) {
        //Switch sprite to left direction

        if(playerCtx->sprite == playerCtx->sprite_left_shadowed) {
            transitionRightTicks = TRANSITION_FRAMES;
        }
        if(playerCtx->sprite != playerCtx->sprite_right_shadowed &&
           playerCtx->sprite != playerCtx->sprite_right) {
            if(playerCtx->sprite == playerCtx->sprite_forward) {
                //transitionRightTicks++;
            }
            if(transitionRightTicks < TRANSITION_FRAMES) {
                transitionRightTicks++;
                if(playerCtx->sprite != playerCtx->sprite_forward &&
                   playerCtx->sprite != playerCtx->sprite_jump) {
                    playerCtx->sprite = playerCtx->sprite_stand;
                }
            } else if(
                playerCtx->sprite != playerCtx->sprite_right_shadowed &&
                transitionRightTicks >= TRANSITION_FRAMES) {
                transitionRightTicks = 0;
                if(playerCtx->sprite == playerCtx->sprite_left_shadowed) {
                    playerCtx->sprite = playerCtx->sprite_right_shadowed;
                } else {
                    playerCtx->sprite = playerCtx->sprite_right;
                }
            }
        }

        //Switch to right direction
        if(playerCtx->sprite == playerCtx->sprite_right_shadowed ||
           playerCtx->sprite == playerCtx->sprite_right) {
            targetX += speed;
            targetX = CLAMP(targetX, WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
            pos.x = CLAMP(pos.x + speed, WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
        }
    }

    //TODO Add more animation states for turning around, so it's smoother.
    //TODO Is this necessary?: (Switch to left/right when jumping up as part of jump animation)
    if(input.pressed & GameKeyDown && playerCtx->sprite != playerCtx->sprite_left_recoil &&
       playerCtx->sprite != playerCtx->sprite_right_recoil) {
        //playerCtx->sprite = playerCtx->sprite_forward;
        if(trainCount < TRAINING_RESET_VALUE) {
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(enemies[i].instance == NULL) continue;
                double features[5];
                featureCalculation(enemies[i].instance, features);
                double outputs[2] = {EnemyActionRetreat, EnemyActionStand};
                genann_train(enemies[i].ai, features, outputs, LEARNING_RATE);
                trainCount++;
                enemies[i].lives += 20;
                break;
            }
        }
    }

    // Clamp player position to screen bounds
    pos.x = CLAMP(lerp(pos.x, targetX, jumpSpeed), WORLD_BORDER_RIGHT_X, WORLD_BORDER_LEFT_X);
    pos.y = CLAMP(pos.y, WORLD_BORDER_BOTTOM_Y, WORLD_BORDER_TOP_Y);

    //Screen space begins at top left corner.
    //Y increases as we go down.
    //X increases as we go right

    // Set new player position
    entity_pos_set(self, pos);

    // Control game exit
    if(input.pressed & GameKeyBack) {
        //Only reaches if player is alive. Send them to the main menu
        //game_manager_game_stop(manager);
        if(trainCount < TRAINING_RESET_VALUE) {
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(enemies[i].instance == NULL) continue;
                double features[5];
                featureCalculation(enemies[i].instance, features);
                double outputs[2] = {EnemyActionRetreat, EnemyActionAttack};
                genann_train(enemies[i].ai, features, outputs, LEARNING_RATE);
                enemies[i].lives += 20;
                break;
            }
        }
    }
}

int successfulJumpCycles = 0;

void global_update(Entity* self, GameManager* manager, void* context) {
    UNUSED(self);
    UNUSED(context);
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

    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance == NULL || globalPlayer == NULL) continue;
        Vector playerPos = entity_pos_get(globalPlayer);
        Vector enemyPos = entity_pos_get(enemies[i].instance);

        if(playerPos.x - enemyPos.x > 0) {
            enemies[i].direction = true;
        } else {
            enemies[i].direction = false;
        }
    }

    //Player bullet movement mechanics
    for(int i = 0; i < MAX_BULLETS; i++) {
        if(bullets[i] == NULL) continue;
        Vector bulletPos = entity_pos_get(bullets[i]);

        float deltaX = bulletsDirection[i] ? bulletMoveSpeed : -bulletMoveSpeed;
        bulletPos.x =
            CLAMP(bulletPos.x + deltaX, WORLD_BORDER_RIGHT_X + 5, WORLD_BORDER_LEFT_X - 5);
        //bulletPos.y += 0.07;
        entity_pos_set(bullets[i], bulletPos);

        //Once bullet reaches end, destroy it
        if(roundf(bulletPos.x) >= WORLD_BORDER_RIGHT_X ||
           roundf(bulletPos.x) <= WORLD_BORDER_LEFT_X) {
            level_remove_entity(gameLevel, bullets[i]);
            bullets[i] = NULL;
            if(successfulJumpCycles > 2 && trainCount != 1000) {
                //STOP training
                trainCount = 1000;
                FURI_LOG_I("DEADZONE", "Finished training! Weights:");
                for(int i = 0; i < enemies[0].ai->total_weights; i++) {
                    FURI_LOG_I("DEADZONE", "Weights: %f", enemies[0].ai->weight[i]);
                }
            } else {
                lastErr -= 20;
                successfulJumpCycles++;
                FURI_LOG_I("DEADZONE", "Err reduced by 20: %f", lastErr);
            }
        }
    }

    //Enemy bullet movement mechanics
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

void global_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(self);
    UNUSED(manager);
    UNUSED(context);
    UNUSED(context);
    if(health == 0) {
        canvas_printf(canvas, 30, 10, "You're dead.");
        canvas_printf(canvas, 20, 20, "Press Back to respawn.");
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

    /*if((transitionLeftTicks < TRANSITION_FRAMES && transitionLeftTicks != 0) ||
       (transitionRightTicks < TRANSITION_FRAMES && transitionRightTicks != 0)) {
        canvas_draw_line(canvas, pos.x - 3, pos.y, pos.x - 5, pos.y);
        canvas_draw_line(canvas, pos.x + 3, pos.y, pos.x + 5, pos.y);
    }*/

    if(horizontalView) {
        canvas_draw_frame(canvas, 2, -4, 124, 61);
        canvas_draw_line(canvas, 1, 57, 0, 60);
        canvas_draw_line(canvas, 126, 57, 128, 61);
    } else {
        canvas_draw_frame(canvas, 32, -4, 64, 30);
        canvas_draw_line(canvas, 32, 26, 0, 57);
        canvas_draw_line(canvas, 64 + 32, 26, 128, 57);
    }
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
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(enemies[i].instance == NULL) continue;
        for(int j = 0; j < enemies[i].ai->total_weights; j++) {
            //canvas_printf(canvas, 20 + j * 5, 50, "Weights: %.1f", enemies[i].ai->weight[j]);
        }
        break;
    }
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
        if(enemies[i].direction) {
            player->sprite = player->sprite_right;
        } else {
            player->sprite = player->sprite_left;
        }
        canvas_printf(canvas, pos.x + 1, pos.y - 10, "%d", enemies[i].lives);
        break;
    }

    // Draw enemy sprite
    // We subtract 5 from x and y, because collision box is 16x16, and we want to center sprite in it.
    canvas_draw_sprite(canvas, player->sprite, pos.x - 5, pos.y - 5);

    //This must be processed here, as the player object will be deinstantiated

    game_level_enemy_render(manager, canvas, context);
    game_level_player_render(manager, canvas, context);
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

                    enemies[i].instance = NULL;
                    kills++;
                    if(kills == 1) {
                        firstKillTick = furi_get_tick();
                    }

                    if(enemies[i].instance != NULL) {
                        /*double features[5];
                        featureCalculation(self, features);
                        double outputs[2] = {EnemyActionRetreat, EnemyActionAttack};
                        genann_train(enemies[i].ai, features, outputs, LEARNING_RATE);
                        trainCount++;*/

                /*
                        update_weights(
                            game_ai_weights,
                            game_ai_features,
                            enemies[i].jumping ? EnemyActionJump : lastAction,
                            -400 / abs((int)roundf(
                                       entity_pos_get(enemies[i].instance).x -
                                       entity_pos_get(globalPlayer).x)));
                  */  }
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
    if(currentTick - lastBehaviorTick > 100) {
        lastBehaviorTick = currentTick;
        Vector playerPos = entity_pos_get(globalPlayer);
        float distXSqToPlayer = (playerPos.x - pos.x) * (playerPos.x - pos.x);
        float distYSqToPlayer = (playerPos.y - pos.y) * (playerPos.y - pos.y);
        float distanceToPlayer = sqrtf(distXSqToPlayer + distYSqToPlayer);

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

        Enemy* enemy = NULL;
        for(int i = 0; i < MAX_ENEMIES; i++) {
            if(enemies[i].instance != self) continue;
            enemy = &enemies[i];
            break;
        }
        if(enemy != NULL) {
            double features[5];
            featureCalculation(self, features);
            //FURI_LOG_I("DEADZONE", "The dist to bullet is: %f", (double)features[0]);
            if(trainCount < TRAINING_RESET_VALUE) {
                bool shouldJump = fabs(features[0] - features[2]) < 20 && features[0] != -1;
                bool attack = distanceToPlayer > 85;
                double outputs[2] = {
                    attack ? EnemyActionAttack : EnemyActionRetreat,
                    shouldJump ? EnemyActionAttack : EnemyActionRetreat};
                genann_train(enemy->ai, features, outputs, LEARNING_RATE);
                trainCount++;
            }

            if(roundf(pos.y) == WORLD_BORDER_BOTTOM_Y) {
                //update_weights(game_ai_weights, game_ai_features, lastAction, 10.0f);
            }

            /*bool jumped = false;
            for(int i = 0; i < MAX_ENEMIES; i++) {
                if(enemies[i].instance != self) continue;
                jumped = enemies[i].jumping;
                break;
             }/

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
            game_ai_weights);*/

            //featureCalculation(self, features);

            const double* outputs = genann_run(enemy->ai, features);

            int actionValue = (int)round(outputs[0]);
            enum EnemyAction firstAction;
            switch(actionValue) {
            case EnemyActionAttack:
                firstAction = EnemyActionAttack;
                break;
            case EnemyActionRetreat:
                firstAction = EnemyActionRetreat;
                break;
            default:
                firstAction = EnemyActionRetreat;
                break;
            }
            actionValue = (int)round(outputs[1]);
            enum EnemyAction secondAction;
            switch(actionValue) {
            case EnemyActionAttack:
                secondAction = EnemyActionJump;
                break;
            case EnemyActionRetreat:
                secondAction = EnemyActionStand;
                break;
            default:
                secondAction = EnemyActionJump;
                break;
            }

            enum EnemyAction action = secondAction != EnemyActionStand ? secondAction :
                                                                         firstAction;

            if(trainCount < TRAINING_RESET_VALUE) {
                if(fabs(features[0] - features[2]) < 15 && action != EnemyActionJump) {
                    //Correct jumping state
                    lastErr += 2;
                } else if(distanceToPlayer > 85 && action != EnemyActionAttack) {
                    lastErr += (double)0.5;
                } else if(distanceToPlayer < 55 && action != EnemyActionRetreat) {
                    lastErr += (double)0.5;
                }
            }

            if(trainCount == 1) {
                enemy->lastAI = genann_copy(enemy->ai);
            }

            if(trainCount == TRAINING_RESET_VALUE) {
                FURI_LOG_I("DEADZONE", "Error value: %f, previous one: %f.", lastErr, err);
                if(lastErr < err) {
                    //Continue using this model, it has improved
                    //lastErr = err;
                    err = lastErr;
                } else {
                    //More errors, revert.
                    genann_free(enemy->ai);
                    enemy->ai = enemy->lastAI;
                }
                trainCount = 0;
                lastErr = 0;
            }

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
                        lerp(
                            pos.x,
                            playerPos.x + ((pos.x - playerPos.x > 0) ? 30 : -30),
                            enemySpeed),
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
                        pos.x + (enemyCtx->sprite == enemyCtx->sprite_left ?
                                     (pos.x - playerPos.x) :
                                     (playerPos.x - pos.x)),
                        enemySpeed),
                    WORLD_BORDER_RIGHT_X,
                    WORLD_BORDER_LEFT_X);
                break;
            case EnemyActionStand:
                break;
            case EnemyActionJump:
                FURI_LOG_I("DEADZONE", "PLEASE JUMP!");
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
}

/****** Entities: Target ******/

static void target_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    if(game_menu_quit_selected) return;
    UNUSED(context);
    UNUSED(manager);

    // Get target position
    Vector pos = entity_pos_get(self);

    // Draw target
    //canvas_draw_box(canvas, pos.x, pos.y, 8, 5);
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

static const EntityDescription global_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = global_update, // called every frame
    .render = global_render, // called every frame, after update
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

    level_add_entity(level, &global_desc);

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
        game_menu_game_selected ? "RESUME GAME" : "PLAY GAME",
        4,
        game_menu_button_callback,
        ctx);
    submenu_add_item(
        submenu,
        game_menu_tutorial_selected ? "RESUME TUTORIAL" : "TUTORIAL",
        5,
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
        gui = furi_record_open(RECORD_GUI);

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
    .target_fps = 125, // target fps, game will try to keep this value
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
