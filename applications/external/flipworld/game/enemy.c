// enemy.c
#include <game/enemy.h>
#include <notification/notification_messages.h>

static EnemyContext* enemy_context_generic;

// Allocation function
static EnemyContext* enemy_generic_alloc(
    const char* id,
    int index,
    Vector size,
    Vector start_position,
    Vector end_position,
    float move_timer, // Wait duration before moving again
    float speed,
    float attack_timer,
    float strength,
    float health) {
    if(!enemy_context_generic) {
        enemy_context_generic = malloc(sizeof(EnemyContext));
    }
    if(!enemy_context_generic) {
        FURI_LOG_E("Game", "Failed to allocate EnemyContext");
        return NULL;
    }
    snprintf(enemy_context_generic->id, sizeof(enemy_context_generic->id), "%s", id);
    enemy_context_generic->index = index;
    enemy_context_generic->size = size;
    enemy_context_generic->start_position = start_position;
    enemy_context_generic->end_position = end_position;
    enemy_context_generic->move_timer = move_timer; // Set wait duration
    enemy_context_generic->elapsed_move_timer = 0.0f; // Initialize elapsed timer
    enemy_context_generic->speed = speed;
    enemy_context_generic->attack_timer = attack_timer;
    enemy_context_generic->strength = strength;
    enemy_context_generic->health = health;
    // Initialize other fields as needed
    enemy_context_generic->sprite_right = NULL; // Assign appropriate sprite
    enemy_context_generic->sprite_left = NULL; // Assign appropriate sprite
    enemy_context_generic->direction = ENEMY_RIGHT; // Default direction
    enemy_context_generic->state = ENEMY_MOVING_TO_END; // Start in IDLE state
    // Set radius based on size, for example, average of size.x and size.y divided by 2
    enemy_context_generic->radius = (size.x + size.y) / 4.0f;
    return enemy_context_generic;
}

// Free function
static void enemy_generic_free(void* context) {
    if(!context) {
        FURI_LOG_E("Game", "Enemy generic free: Invalid context");
        return;
    }
    free(context);
    context = NULL;
    if(enemy_context_generic) {
        free(enemy_context_generic);
        enemy_context_generic = NULL;
    }
}

// Enemy start function
static void enemy_start(Entity* self, GameManager* manager, void* context) {
    UNUSED(manager);
    if(!self || !context) {
        FURI_LOG_E("Game", "Enemy start: Invalid parameters");
        return;
    }
    if(!enemy_context_generic) {
        FURI_LOG_E("Game", "Enemy start: Enemy context not set");
        return;
    }

    EnemyContext* enemy_context = (EnemyContext*)context;
    // Copy fields from generic context
    snprintf(enemy_context->id, sizeof(enemy_context->id), "%s", enemy_context_generic->id);
    enemy_context->index = enemy_context_generic->index;
    enemy_context->size = enemy_context_generic->size;
    enemy_context->start_position = enemy_context_generic->start_position;
    enemy_context->end_position = enemy_context_generic->end_position;
    enemy_context->move_timer = enemy_context_generic->move_timer;
    enemy_context->elapsed_move_timer = enemy_context_generic->elapsed_move_timer;
    enemy_context->speed = enemy_context_generic->speed;
    enemy_context->attack_timer = enemy_context_generic->attack_timer;
    enemy_context->strength = enemy_context_generic->strength;
    enemy_context->health = enemy_context_generic->health;
    enemy_context->sprite_right = enemy_context_generic->sprite_right;
    enemy_context->sprite_left = enemy_context_generic->sprite_left;
    enemy_context->direction = enemy_context_generic->direction;
    enemy_context->state = enemy_context_generic->state;
    enemy_context->radius = enemy_context_generic->radius;

    // Set enemy's initial position based on start_position
    entity_pos_set(self, enemy_context->start_position);

    // Add collision circle based on the enemy's radius
    entity_collider_add_circle(self, enemy_context->radius);
}

// Enemy render function
static void enemy_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    if(!self || !context || !canvas || !manager) return;

    EnemyContext* enemy_context = (EnemyContext*)context;
    GameContext* game_context = game_manager_game_context_get(manager);

    // Get the position of the enemy
    Vector pos = entity_pos_get(self);

    // Choose sprite based on direction
    Sprite* current_sprite = NULL;
    if(enemy_context->direction == ENEMY_LEFT) {
        current_sprite = enemy_context->sprite_left;
    } else {
        current_sprite = enemy_context->sprite_right;
    }

    // Draw enemy sprite relative to camera, centered on the enemy's position
    canvas_draw_sprite(
        canvas,
        current_sprite,
        pos.x - camera_x - (enemy_context->size.x / 2),
        pos.y - camera_y - (enemy_context->size.y / 2));

    // instead of username, draw health
    char health_str[32];
    snprintf(health_str, sizeof(health_str), "%.0f", (double)enemy_context->health);
    draw_username(canvas, pos, health_str);

    // Draw user stats (this has to be done for all enemies)
    draw_user_stats(canvas, (Vector){0, 50}, manager);

    // draw player username from GameContext
    Vector posi = entity_pos_get(game_context->player);
    draw_username(canvas, posi, game_context->player_context->username);
}

static void send_attack_notification(
    GameContext* game_context,
    EnemyContext* enemy_context,
    bool player_attacked) {
    if(!game_context || !enemy_context) {
        FURI_LOG_E("Game", "Send attack notification: Invalid parameters");
        return;
    }

    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    const bool vibration_allowed = strstr(yes_or_no_choices[game_vibration_on_index], "Yes") !=
                                   NULL;
    const bool sound_allowed = strstr(yes_or_no_choices[game_sound_on_index], "Yes") != NULL;

    if(player_attacked) {
        if(vibration_allowed && sound_allowed) {
            notification_message(notifications, &sequence_success);
        } else if(vibration_allowed && !sound_allowed) {
            notification_message(notifications, &sequence_single_vibro);
        } else if(!vibration_allowed && sound_allowed) {
            // change this to sound later
            notification_message(notifications, &sequence_blink_blue_100);
        } else {
            notification_message(notifications, &sequence_blink_blue_100);
        }
        FURI_LOG_I("Game", "Player attacked enemy '%s'!", enemy_context->id);
    } else {
        if(vibration_allowed && sound_allowed) {
            notification_message(notifications, &sequence_error);
        } else if(vibration_allowed && !sound_allowed) {
            notification_message(notifications, &sequence_single_vibro);
        } else if(!vibration_allowed && sound_allowed) {
            // change this to sound later
            notification_message(notifications, &sequence_blink_red_100);
        } else {
            notification_message(notifications, &sequence_blink_red_100);
        }

        FURI_LOG_I("Game", "Enemy '%s' attacked the player!", enemy_context->id);
    }

    // close the notifications
    furi_record_close(RECORD_NOTIFICATION);
}

// Enemy collision function
static void enemy_collision(Entity* self, Entity* other, GameManager* manager, void* context) {
    if(!self || !other || !context || !manager) {
        FURI_LOG_E("Game", "Enemy collision: Invalid parameters");
        return;
    }

    // Check if the enemy collided with the player
    if(entity_description_get(other) == &player_desc) {
        // Retrieve enemy context
        EnemyContext* enemy_context = (EnemyContext*)context;
        GameContext* game_context = game_manager_game_context_get(manager);
        if(!enemy_context) {
            FURI_LOG_E("Game", "Enemy collision: EnemyContext is NULL");
            return;
        }
        if(!game_context) {
            FURI_LOG_E("Game", "Enemy collision: GameContext is NULL");
            return;
        }

        // Get positions of the enemy and the player
        Vector enemy_pos = entity_pos_get(self);
        Vector player_pos = entity_pos_get(other);

        // Determine if the enemy is facing the player or player is facing the enemy
        bool enemy_is_facing_player = false;
        bool player_is_facing_enemy = false;

        // Determine if the enemy is facing the player
        if((enemy_context->direction == ENEMY_LEFT && player_pos.x < enemy_pos.x) ||
           (enemy_context->direction == ENEMY_RIGHT && player_pos.x > enemy_pos.x) ||
           (enemy_context->direction == ENEMY_UP && player_pos.y < enemy_pos.y) ||
           (enemy_context->direction == ENEMY_DOWN && player_pos.y > enemy_pos.y)) {
            enemy_is_facing_player = true;
        }

        // Determine if the player is facing the enemy
        if((game_context->player_context->direction == PLAYER_LEFT &&
            enemy_pos.x < player_pos.x) ||
           (game_context->player_context->direction == PLAYER_RIGHT &&
            enemy_pos.x > player_pos.x) ||
           (game_context->player_context->direction == PLAYER_UP && enemy_pos.y < player_pos.y) ||
           (game_context->player_context->direction == PLAYER_DOWN &&
            enemy_pos.y > player_pos.y)) {
            player_is_facing_enemy = true;
        }

        // Handle Player Attacking Enemy (Press OK, facing enemy, and enemy not facing player)
        if(player_is_facing_enemy && game_context->user_input == GameKeyOk &&
           !enemy_is_facing_player) {
            if(game_context->player_context->elapsed_attack_timer >=
               game_context->player_context->attack_timer) {
                send_attack_notification(game_context, enemy_context, true);

                // Reset player's elapsed attack timer
                game_context->player_context->elapsed_attack_timer = 0.0f;
                enemy_context->elapsed_attack_timer =
                    0.0f; // Reset enemy's attack timer to block enemy attack

                // Increase XP by the enemy's strength
                game_context->player_context->xp += enemy_context->strength;

                // Increase healthy by 10% of the enemy's strength
                game_context->player_context->health += enemy_context->strength * 0.1f;
                if(game_context->player_context->health >
                   game_context->player_context->max_health) {
                    game_context->player_context->health =
                        game_context->player_context->max_health;
                }

                // Decrease enemy health by player strength
                enemy_context->health -= game_context->player_context->strength;

                if(enemy_context->health <= 0) {
                    FURI_LOG_I(
                        "Game",
                        "Enemy '%s' is dead.. resetting enemy position and health",
                        enemy_context->id);
                    enemy_context->state = ENEMY_DEAD;

                    // Reset enemy position and health
                    enemy_context->health = 100; // this needs to be set to the enemy's max health

                    // remove from game context and set in safe zone
                    game_context->enemies[enemy_context->index] = NULL;
                    game_context->enemy_count--;
                    entity_collider_remove(self);
                    entity_pos_set(self, (Vector){-100, -100});
                    return;
                } else {
                    FURI_LOG_I(
                        "Game",
                        "Enemy '%s' took %f damage from player",
                        enemy_context->id,
                        (double)game_context->player_context->strength);
                    enemy_context->state = ENEMY_ATTACKED;

                    // Bounce the enemy back by X units opposite their last movement direction
                    enemy_pos.x -= game_context->player_context->dx * enemy_context->radius;
                    enemy_pos.y -= game_context->player_context->dy * enemy_context->radius;
                    entity_pos_set(self, enemy_pos);

                    // Reset enemy's movement direction to prevent immediate re-collision
                    game_context->player_context->dx = 0;
                    game_context->player_context->dy = 0;
                }
            } else {
                FURI_LOG_I(
                    "Game",
                    "Player attack on enemy '%s' is on cooldown: %f seconds remaining",
                    enemy_context->id,
                    (double)(game_context->player_context->attack_timer -
                             game_context->player_context->elapsed_attack_timer));
            }
        }
        // Handle Enemy Attacking Player (enemy facing player)
        else if(enemy_is_facing_player) {
            if(enemy_context->elapsed_attack_timer >= enemy_context->attack_timer) {
                send_attack_notification(game_context, enemy_context, false);

                // Reset enemy's elapsed attack timer
                enemy_context->elapsed_attack_timer = 0.0f;

                // Decrease player health by enemy strength
                game_context->player_context->health -= enemy_context->strength;

                if(game_context->player_context->health <= 0) {
                    FURI_LOG_I("Game", "Player is dead.. resetting player position and health");
                    game_context->player_context->state = PLAYER_DEAD;

                    // Reset player position and health
                    entity_pos_set(other, game_context->player_context->start_position);
                    game_context->player_context->health =
                        game_context->player_context->max_health;

                    // subtract player's XP by the enemy's strength
                    game_context->player_context->xp -= enemy_context->strength;
                    if((int)game_context->player_context->xp < 0) {
                        game_context->player_context->xp = 0;
                    }
                } else {
                    FURI_LOG_I(
                        "Game",
                        "Player took %f damage from enemy '%s'",
                        (double)enemy_context->strength,
                        enemy_context->id);
                    game_context->player_context->state = PLAYER_ATTACKED;

                    // Bounce the player back by X units opposite their last movement direction
                    player_pos.x -= game_context->player_context->dx * enemy_context->radius;
                    player_pos.y -= game_context->player_context->dy * enemy_context->radius;
                    entity_pos_set(other, player_pos);

                    // Reset player's movement direction to prevent immediate re-collision
                    game_context->player_context->dx = 0;
                    game_context->player_context->dy = 0;
                }
            } else {
                FURI_LOG_I(
                    "Game",
                    "Enemy '%s' attack on player is on cooldown: %f seconds remaining",
                    enemy_context->id,
                    (double)(enemy_context->attack_timer - enemy_context->elapsed_attack_timer));
            }
        } else // handle other collisions
        {
            // bounce player and enemy away from each other
            Vector player_pos = entity_pos_get(other);
            Vector enemy_pos = entity_pos_get(self);

            // Calculate the direction vector from player to enemy
            Vector direction_vector = {enemy_pos.x - player_pos.x, enemy_pos.y - player_pos.y};

            // Normalize the direction vector
            float length = sqrt(
                direction_vector.x * direction_vector.x + direction_vector.y * direction_vector.y);
            if(length != 0) {
                direction_vector.x /= length;
                direction_vector.y /= length;
            }

            // Move the player and enemy away from each other
            player_pos.y -= direction_vector.y * 3;
            entity_pos_set(other, player_pos);

            enemy_pos.x += direction_vector.x * 3;
            entity_pos_set(self, enemy_pos);

            // Reset player's movement direction to prevent immediate re-collision
            game_context->player_context->dx = 0;
            game_context->player_context->dy = 0;
        }

        // Reset enemy's state
        enemy_context->state = ENEMY_IDLE;
        enemy_context->elapsed_move_timer = 0.0f;

        if(game_context->player_context->state == PLAYER_DEAD) {
            // Reset player's position and health
            entity_pos_set(other, game_context->player_context->start_position);
            game_context->player_context->health = 100;
        }
    }
}

// Enemy update function
static void enemy_update(Entity* self, GameManager* manager, void* context) {
    if(!self || !context || !manager) return;

    EnemyContext* enemy_context = (EnemyContext*)context;
    if(!enemy_context || enemy_context->state == ENEMY_DEAD) {
        return;
    }

    GameContext* game_context = game_manager_game_context_get(manager);
    if(!game_context) {
        FURI_LOG_E("Game", "Enemy update: Failed to get GameContext");
        return;
    }

    float delta_time = 1.0f / game_context->fps;

    // Increment the elapsed_attack_timer for the enemy
    enemy_context->elapsed_attack_timer += delta_time;

    switch(enemy_context->state) {
    case ENEMY_IDLE:
        // Increment the elapsed_move_timer
        enemy_context->elapsed_move_timer += delta_time;

        // Check if it's time to move again
        if(enemy_context->elapsed_move_timer >= enemy_context->move_timer) {
            // Determine the next state based on the current position
            Vector current_pos = entity_pos_get(self);
            if(fabs(current_pos.x - enemy_context->start_position.x) < (double)1.0 &&
               fabs(current_pos.y - enemy_context->start_position.y) < (double)1.0) {
                enemy_context->state = ENEMY_MOVING_TO_END;
            } else {
                enemy_context->state = ENEMY_MOVING_TO_START;
            }
            enemy_context->elapsed_move_timer = 0.0f;
        }
        break;

    case ENEMY_MOVING_TO_END:
    case ENEMY_MOVING_TO_START: {
        // Determine the target position based on the current state
        Vector target_position = (enemy_context->state == ENEMY_MOVING_TO_END) ?
                                     enemy_context->end_position :
                                     enemy_context->start_position;

        // Get current position
        Vector current_pos = entity_pos_get(self);
        Vector direction_vector = {0, 0};

        // Calculate direction towards the target
        if(current_pos.x < target_position.x) {
            direction_vector.x = 1.0f;
            enemy_context->direction = ENEMY_RIGHT;
        } else if(current_pos.x > target_position.x) {
            direction_vector.x = -1.0f;
            enemy_context->direction = ENEMY_LEFT;
        }

        if(current_pos.y < target_position.y) {
            direction_vector.y = 1.0f;
            enemy_context->direction = ENEMY_DOWN;
        } else if(current_pos.y > target_position.y) {
            direction_vector.y = -1.0f;
            enemy_context->direction = ENEMY_UP;
        }

        // Normalize direction vector
        float length = sqrt(
            direction_vector.x * direction_vector.x + direction_vector.y * direction_vector.y);
        if(length != 0) {
            direction_vector.x /= length;
            direction_vector.y /= length;
        }

        // Update position based on direction and speed
        Vector new_pos = current_pos;
        new_pos.x += direction_vector.x * enemy_context->speed * delta_time;
        new_pos.y += direction_vector.y * enemy_context->speed * delta_time;

        // Clamp the position to the target to prevent overshooting
        if((direction_vector.x > 0.0f && new_pos.x > target_position.x) ||
           (direction_vector.x < 0.0f && new_pos.x < target_position.x)) {
            new_pos.x = target_position.x;
        }

        if((direction_vector.y > 0.0f && new_pos.y > target_position.y) ||
           (direction_vector.y < 0.0f && new_pos.y < target_position.y)) {
            new_pos.y = target_position.y;
        }

        entity_pos_set(self, new_pos);

        // Check if the enemy has reached or surpassed the target_position
        bool reached_x = fabs(new_pos.x - target_position.x) < (double)1.0;
        bool reached_y = fabs(new_pos.y - target_position.y) < (double)1.0;

        // If reached the target position on both axes, transition to IDLE
        if(reached_x && reached_y) {
            enemy_context->state = ENEMY_IDLE;
            enemy_context->elapsed_move_timer = 0.0f;
        }
    } break;

    default:
        break;
    }
}

// Free function for the entity
static void enemy_free(Entity* self, GameManager* manager, void* context) {
    UNUSED(self);
    UNUSED(manager);
    if(context) enemy_generic_free(context);
}

// Enemy behavior structure
static const EntityDescription _generic_enemy = {
    .start = enemy_start,
    .stop = enemy_free,
    .update = enemy_update,
    .render = enemy_render,
    .collision = enemy_collision,
    .event = NULL,
    .context_size = sizeof(EnemyContext),
};

// Enemy function to return the entity description
const EntityDescription* enemy(
    GameManager* manager,
    const char* id,
    int index,
    Vector start_position,
    Vector end_position,
    float move_timer, // Wait duration before moving again
    float speed,
    float attack_timer,
    float strength,
    float health) {
    SpriteContext* sprite_context = get_sprite_context(id);
    if(!sprite_context) {
        FURI_LOG_E("Game", "Failed to get SpriteContext");
        return NULL;
    }

    // Allocate a new EnemyContext with provided parameters
    enemy_context_generic = enemy_generic_alloc(
        id,
        index,
        (Vector){sprite_context->width, sprite_context->height},
        start_position,
        end_position,
        move_timer, // Set wait duration
        speed,
        attack_timer,
        strength,
        health);
    if(!enemy_context_generic) {
        FURI_LOG_E("Game", "Failed to allocate EnemyContext");
        return NULL;
    }

    enemy_context_generic->sprite_right =
        game_manager_sprite_load(manager, sprite_context->right_file_name);
    enemy_context_generic->sprite_left =
        game_manager_sprite_load(manager, sprite_context->left_file_name);

    // Set initial direction based on start and end positions
    if(start_position.x < end_position.x) {
        enemy_context_generic->direction = ENEMY_RIGHT;
    } else {
        enemy_context_generic->direction = ENEMY_LEFT;
    }

    // Set initial state based on movement
    if(start_position.x != end_position.x || start_position.y != end_position.y) {
        enemy_context_generic->state = ENEMY_MOVING_TO_END;
    } else {
        enemy_context_generic->state = ENEMY_IDLE;
    }

    return &_generic_enemy;
}

void spawn_enemy_json_furi(Level* level, GameManager* manager, FuriString* json) {
    if(!level) {
        FURI_LOG_E("Game", "Level is NULL");
        return;
    }
    if(!json) {
        FURI_LOG_E("Game", "JSON is NULL");
        return;
    }
    if(!manager) {
        FURI_LOG_E("Game", "GameManager is NULL");
        return;
    }
    // parameters: id, index, size.x, size.y, start_position.x, start_position.y, end_position.x, end_position.y, move_timer, speed, attack_timer, strength, health
    FuriString* id = get_json_value_furi("id", json);
    FuriString* _index = get_json_value_furi("index", json);
    //
    FuriString* start_position = get_json_value_furi("start_position", json);
    FuriString* start_position_x = get_json_value_furi("x", start_position);
    FuriString* start_position_y = get_json_value_furi("y", start_position);
    //
    FuriString* end_position = get_json_value_furi("end_position", json);
    FuriString* end_position_x = get_json_value_furi("x", end_position);
    FuriString* end_position_y = get_json_value_furi("y", end_position);
    //
    FuriString* move_timer = get_json_value_furi("move_timer", json);
    FuriString* speed = get_json_value_furi("speed", json);
    FuriString* attack_timer = get_json_value_furi("attack_timer", json);
    FuriString* strength = get_json_value_furi("strength", json);
    FuriString* health = get_json_value_furi("health", json);
    //

    if(!id || !_index || !start_position || !start_position_x || !start_position_y ||
       !end_position || !end_position_x || !end_position_y || !move_timer || !speed ||
       !attack_timer || !strength || !health) {
        FURI_LOG_E("Game", "Failed to parse JSON values");
        return;
    }

    GameContext* game_context = game_manager_game_context_get(manager);
    if(game_context && game_context->enemy_count < MAX_ENEMIES &&
       !game_context->enemies[game_context->enemy_count]) {
        game_context->enemies[game_context->enemy_count] = level_add_entity(
            level,
            enemy(
                manager,
                furi_string_get_cstr(id),
                atoi(furi_string_get_cstr(_index)),
                (Vector){
                    strtod(furi_string_get_cstr(start_position_x), NULL),
                    strtod(furi_string_get_cstr(start_position_y), NULL)},
                (Vector){
                    strtod(furi_string_get_cstr(end_position_x), NULL),
                    strtod(furi_string_get_cstr(end_position_y), NULL)},
                strtod(furi_string_get_cstr(move_timer), NULL),
                strtod(furi_string_get_cstr(speed), NULL),
                strtod(furi_string_get_cstr(attack_timer), NULL),
                strtod(furi_string_get_cstr(strength), NULL),
                strtod(furi_string_get_cstr(health), NULL)));
        game_context->enemy_count++;
    }

    furi_string_free(id);
    furi_string_free(_index);
    furi_string_free(start_position);
    furi_string_free(start_position_x);
    furi_string_free(start_position_y);
    furi_string_free(end_position);
    furi_string_free(end_position_x);
    furi_string_free(end_position_y);
    furi_string_free(move_timer);
    furi_string_free(speed);
    furi_string_free(attack_timer);
    furi_string_free(strength);
    furi_string_free(health);
}
