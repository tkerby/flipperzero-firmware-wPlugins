#include <game/player.h>
#include <game/storage.h>
/****** Entities: Player ******/
static Level *get_next_level(GameManager *manager)
{
    GameContext *game_context = game_manager_game_context_get(manager);
    if (!game_context)
    {
        FURI_LOG_E(TAG, "Failed to get game context");
        return NULL;
    }
    for (int i = game_context->current_level + 1; i < game_context->level_count; i++)
    {
        if (!game_context->levels[i])
        {
            if (!allocate_level(manager, i))
            {
                FURI_LOG_E(TAG, "Failed to allocate level %d", i);
                return NULL;
            }
        }
        game_context->current_level = i;
        return game_context->levels[i];
    }
    FURI_LOG_I(TAG, "No more levels to load");
    return NULL;
}

void player_spawn(Level *level, GameManager *manager)
{
    if (!level || !manager)
    {
        FURI_LOG_E(TAG, "Invalid arguments to player_spawn");
        return;
    }

    GameContext *game_context = game_manager_game_context_get(manager);
    if (!game_context)
    {
        FURI_LOG_E(TAG, "Failed to get game context");
        return;
    }

    game_context->player = level_add_entity(level, &player_desc);
    if (!game_context->player)
    {
        FURI_LOG_E(TAG, "Failed to add player entity to level");
        return;
    }

    // Set player position.
    entity_pos_set(game_context->player, (Vector){WORLD_WIDTH / 2, WORLD_HEIGHT / 2});

    // Box is centered in player x and y, and its size
    entity_collider_add_rect(game_context->player, 13, 11);

    // Get player context
    PlayerContext *player_context = entity_context_get(game_context->player);
    if (!player_context)
    {
        FURI_LOG_E(TAG, "Failed to get player context");
        return;
    }

    // player context must be set each level or NULL pointer will be dereferenced
    if (!load_player_context(player_context))
    {
        FURI_LOG_E(TAG, "Loading player context failed. Initializing default values.");

        // Initialize default player context
        player_context->sprite_right = game_manager_sprite_load(manager, "player_right_sword_15x11px.fxbm");
        player_context->sprite_left = game_manager_sprite_load(manager, "player_left_sword_15x11px.fxbm");
        player_context->direction = PLAYER_RIGHT; // default direction
        player_context->health = 100;
        player_context->strength = 10;
        player_context->level = 1;
        player_context->xp = 0;
        player_context->start_position = entity_pos_get(game_context->player);
        player_context->attack_timer = 0.1f;
        player_context->elapsed_attack_timer = player_context->attack_timer;
        player_context->health_regen = 1; // 1 health per second
        player_context->elapsed_health_regen = 0;
        player_context->max_health = 100 + ((player_context->level - 1) * 10); // 10 health per level

        // Set player username
        if (!load_char("Flip-Social-Username", player_context->username, sizeof(player_context->username)))
        {
            // If loading username fails, default to "Player"
            snprintf(player_context->username, sizeof(player_context->username), "Player");
        }

        game_context->player_context = player_context;

        // Save the initialized context
        if (!save_player_context(player_context))
        {
            FURI_LOG_E(TAG, "Failed to save player context after initialization");
        }

        return;
    }

    // Load player sprite (we'll add this to the JSON later when players can choose their sprite)
    player_context->sprite_right = game_manager_sprite_load(manager, "player_right_sword_15x11px.fxbm");
    player_context->sprite_left = game_manager_sprite_load(manager, "player_left_sword_15x11px.fxbm");

    player_context->start_position = entity_pos_get(game_context->player);

    // Update player stats based on XP using iterative method
    // Function to get the current level based on XP iteratively
    int get_player_level_iterative(uint32_t xp)
    {
        int level = 1;
        uint32_t xp_required = 100; // Base XP for level 2

        while (level < 100 && xp >= xp_required) // Maximum level supported
        {
            level++;
            xp_required = (uint32_t)(xp_required * 1.5); // 1.5 growth factor per level
        }

        return level;
    }

    // Determine the player's level based on XP
    player_context->level = get_player_level_iterative(player_context->xp);

    // Update strength and max health based on the new level
    player_context->strength = 10 + (player_context->level * 1);           // 1 strength per level
    player_context->max_health = 100 + ((player_context->level - 1) * 10); // 10 health per level

    // Assign loaded player context to game context
    game_context->player_context = player_context;
}

// code from Derek Jamison
// eventually we'll add dynamic positioning based on how much pitch/roll is detected
// instead of assigning a fixed value
static int player_x_from_pitch(float pitch)
{
    if (pitch > 6.0)
    {
        return 1;
    }
    else if (pitch < -8.0)
    {
        return -1;
    }
    return 0;
}

static int player_y_from_roll(float roll)
{
    if (roll > 9.0)
    {
        return 1;
    }
    else if (roll < -20.0)
    {
        return -1;
    }
    return 0;
}

static void player_update(Entity *self, GameManager *manager, void *context)
{
    if (!self || !manager || !context)
        return;

    PlayerContext *player = (PlayerContext *)context;
    InputState input = game_manager_input_get(manager);
    Vector pos = entity_pos_get(self);
    GameContext *game_context = game_manager_game_context_get(manager);

    // Store previous direction
    int prev_dx = player->dx;
    int prev_dy = player->dy;

    // Reset movement deltas each frame
    player->dx = 0;
    player->dy = 0;

    if (game_context->imu_present)
    {
        player->dx = player_x_from_pitch(-imu_pitch_get(game_context->imu));
        player->dy = player_y_from_roll(-imu_roll_get(game_context->imu));

        switch (player->dx)
        {
        case -1:
            player->direction = PLAYER_LEFT;
            pos.x -= 1;
            break;
        case 1:
            player->direction = PLAYER_RIGHT;
            pos.x += 1;
            break;
        default:
            break;
        }

        switch (player->dy)
        {
        case -1:
            player->direction = PLAYER_UP;
            pos.y -= 1;
            break;
        case 1:
            player->direction = PLAYER_DOWN;
            pos.y += 1;
            break;
        default:
            break;
        }
    }

    // Apply health regeneration
    player->elapsed_health_regen += 1.0f / game_context->fps;
    if (player->elapsed_health_regen >= 1.0f && player->health < player->max_health)
    {
        player->health += (player->health_regen + player->health > player->max_health)
                              ? (player->max_health - player->health)
                              : player->health_regen;
        player->elapsed_health_regen = 0;
    }

    // Increment the elapsed_attack_timer for the player
    player->elapsed_attack_timer += 1.0f / game_context->fps;

    // Handle movement input
    if (input.held & GameKeyUp)
    {
        pos.y -= 2;
        player->dy = -1;
        player->direction = PLAYER_UP;
        game_context->user_input = GameKeyUp;
    }
    if (input.held & GameKeyDown)
    {
        pos.y += 2;
        player->dy = 1;
        player->direction = PLAYER_DOWN;
        game_context->user_input = GameKeyDown;
    }
    if (input.held & GameKeyLeft)
    {
        pos.x -= 2;
        player->dx = -1;
        player->direction = PLAYER_LEFT;
        game_context->user_input = GameKeyLeft;
    }
    if (input.held & GameKeyRight)
    {
        pos.x += 2;
        player->dx = 1;
        player->direction = PLAYER_RIGHT;
        game_context->user_input = GameKeyRight;
    }

    // Clamp the player's position to stay within world bounds
    pos.x = CLAMP(pos.x, WORLD_WIDTH - 5, 5);
    pos.y = CLAMP(pos.y, WORLD_HEIGHT - 5, 5);

    // Update player position
    entity_pos_set(self, pos);

    // switch levels if holding OK
    if (input.pressed & GameKeyOk)
    {
        // if all enemies are dead, allow the "OK" button to switch levels
        // otherwise the "OK" button will be used to attack
        if (game_context->enemy_count == 0)
        {
            FURI_LOG_I(TAG, "Switching levels");
            save_player_context(player);
            game_manager_next_level_set(manager, get_next_level(manager));
            furi_delay_ms(500);
            return;
        }
        else
        {
            game_context->user_input = GameKeyOk;
            // furi_delay_ms(100);
        }
    }

    // If the player is not moving, retain the last movement direction
    if (player->dx == 0 && player->dy == 0)
    {
        player->dx = prev_dx;
        player->dy = prev_dy;
        player->state = PLAYER_IDLE;
        game_context->user_input = -1; // reset user input
    }
    else
    {
        player->state = PLAYER_MOVING;
    }

    // Handle back button to stop the game
    if (input.pressed & GameKeyBack)
    {
        game_manager_game_stop(manager);
    }
}

static void player_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    UNUSED(manager);
    if (!self || !context || !canvas)
        return;
    // Get player context
    PlayerContext *player = context;

    // Get player position
    Vector pos = entity_pos_get(self);

    // Draw background (updates camera_x and camera_y)
    draw_background(canvas, pos);

    // Draw player sprite relative to camera, centered on the player's position
    canvas_draw_sprite(
        canvas,
        player->direction == PLAYER_RIGHT ? player->sprite_right : player->sprite_left,
        pos.x - camera_x - 5, // Center the sprite horizontally
        pos.y - camera_y - 5  // Center the sprite vertically
    );
}

const EntityDescription player_desc = {
    .start = NULL,                         // called when entity is added to the level
    .stop = NULL,                          // called when entity is removed from the level
    .update = player_update,               // called every frame
    .render = player_render,               // called every frame, after update
    .collision = NULL,                     // called when entity collides with another entity
    .event = NULL,                         // called when entity receives an event
    .context_size = sizeof(PlayerContext), // size of entity context, will be automatically allocated and freed
};

static SpriteContext *sprite_generic_alloc(const char *id, bool is_enemy, uint8_t width, uint8_t height)
{
    SpriteContext *ctx = malloc(sizeof(SpriteContext));
    if (!ctx)
    {
        FURI_LOG_E("Game", "Failed to allocate SpriteContext");
        return NULL;
    }
    snprintf(ctx->id, sizeof(ctx->id), "%s", id);
    ctx->width = width;
    ctx->height = height;
    if (!is_enemy)
    {
        snprintf(ctx->right_file_name, sizeof(ctx->right_file_name), "player_right_%s_%dx%dpx.fxbm", id, width, height);
        snprintf(ctx->left_file_name, sizeof(ctx->left_file_name), "player_left_%s_%dx%dpx.fxbm", id, width, height);
    }
    else
    {
        snprintf(ctx->right_file_name, sizeof(ctx->right_file_name), "enemy_right_%s_%dx%dpx.fxbm", id, width, height);
        snprintf(ctx->left_file_name, sizeof(ctx->left_file_name), "enemy_left_%s_%dx%dpx.fxbm", id, width, height);
    }
    return ctx;
}

SpriteContext *get_sprite_context(const char *name)
{
    if (strcmp(name, "axe") == 0)
    {
        return sprite_generic_alloc("axe", false, 15, 11);
    }
    else if (strcmp(name, "bow") == 0)
    {
        return sprite_generic_alloc("bow", false, 13, 11);
    }
    else if (strcmp(name, "naked") == 0)
    {
        return sprite_generic_alloc("naked", false, 10, 10);
    }
    else if (strcmp(name, "sword") == 0)
    {
        return sprite_generic_alloc("sword", false, 15, 11);
    }
    else if (strcmp(name, "cyclops") == 0)
    {
        return sprite_generic_alloc("cyclops", true, 10, 11);
    }
    else if (strcmp(name, "ghost") == 0)
    {
        return sprite_generic_alloc("ghost", true, 15, 15);
    }
    else if (strcmp(name, "ogre") == 0)
    {
        return sprite_generic_alloc("ogre", true, 10, 13);
    }

    // If no match is found
    FURI_LOG_E("Game", "Sprite not found: %s", name);
    return NULL;
}