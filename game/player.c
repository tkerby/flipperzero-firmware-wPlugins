#include <game/player.h>
#include <game/storage.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <engine/entity_i.h>
/****** Entities: Player ******/
static Level *next_level(GameManager *manager)
{
    GameContext *game_context = game_manager_game_context_get(manager);
    if (!game_context)
    {
        FURI_LOG_E(TAG, "Failed to get game context");
        game_context->is_switching_level = false;
        return NULL;
    }
    // check if there are more levels to load
    if (game_context->current_level + 1 >= game_context->level_count)
    {
        game_context->current_level = 0;
        if (!game_context->levels[game_context->current_level])
        {
            if (!allocate_level(manager, game_context->current_level))
            {
                FURI_LOG_E(TAG, "Failed to allocate level %d", game_context->current_level);
                game_context->is_switching_level = false;
                furi_delay_ms(100);
                return NULL;
            }
        }
        game_context->is_switching_level = false;
        furi_delay_ms(100);
        return game_context->levels[game_context->current_level];
    }
    for (int i = game_context->current_level + 1; i < game_context->level_count; i++)
    {
        if (!game_context->levels[i])
        {
            if (!allocate_level(manager, i))
            {
                FURI_LOG_E(TAG, "Failed to allocate level %d", i);
                game_context->is_switching_level = false;
                furi_delay_ms(100);
                return NULL;
            }
        }
        game_context->current_level = i;
        game_context->is_switching_level = false;
        furi_delay_ms(100);
        return game_context->levels[i];
    }
    game_context->is_switching_level = false;
    furi_delay_ms(100);
    return NULL;
}

// Update player stats based on XP using iterative method
static int get_player_level_iterative(uint32_t xp)
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

    // Get player context
    PlayerContext *pctx = entity_context_get(game_context->player);
    if (!pctx)
    {
        FURI_LOG_E(TAG, "Failed to get player context");
        return;
    }

    SpriteContext *sprite_context = get_sprite_context(player_sprite_choices[player_sprite_index]);
    if (!sprite_context)
    {
        FURI_LOG_E(TAG, "Failed to get sprite context");
        return;
    }

    // add a collider to the player entity
    entity_collider_add_rect(game_context->player, sprite_context->width, sprite_context->height);

    // player context must be set each level or NULL pointer will be dereferenced
    if (!load_player_context(pctx))
    {
        FURI_LOG_E(TAG, "Loading player context failed. Initializing default values.");

        // Initialize default player context
        pctx->sprite_right = game_manager_sprite_load(manager, sprite_context->right_file_name);
        pctx->sprite_left = game_manager_sprite_load(manager, sprite_context->left_file_name);
        pctx->direction = ENTITY_RIGHT; // default direction
        pctx->left = false;             // default sprite direction
        pctx->health = 100;
        pctx->strength = 10;
        pctx->level = 1;
        pctx->xp = 0;
        pctx->start_position = entity_pos_get(game_context->player);
        pctx->attack_timer = 0.1f;
        pctx->elapsed_attack_timer = pctx->attack_timer;
        pctx->health_regen = 1; // 1 health per second
        pctx->elapsed_health_regen = 0;
        pctx->max_health = 100 + ((pctx->level - 1) * 10); // 10 health per level

        // Set player username
        if (!load_char("Flip-Social-Username", pctx->username, sizeof(pctx->username)))
        {
            // check if data/player/username
            if (!load_char("player/username", pctx->username, sizeof(pctx->username)))
            {
                // If loading username fails, default to "Player"
                snprintf(pctx->username, sizeof(pctx->username), "Player");
            }
        }

        game_context->player_context = pctx;

        // Save the initialized context
        if (!save_player_context(pctx))
        {
            FURI_LOG_E(TAG, "Failed to save player context after initialization");
        }
        free(sprite_context);
        return;
    }

    // Load player sprite
    pctx->sprite_right = game_manager_sprite_load(manager, sprite_context->right_file_name);
    pctx->sprite_left = game_manager_sprite_load(manager, sprite_context->left_file_name);

    pctx->start_position = entity_pos_get(game_context->player);

    // Determine the player's level based on XP
    pctx->level = get_player_level_iterative(pctx->xp);

    // Update strength and max health based on the new level
    pctx->strength = 10 + (pctx->level * 1);           // 1 strength per level
    pctx->max_health = 100 + ((pctx->level - 1) * 10); // 10 health per level

    // set the player's left sprite direction
    pctx->left = pctx->direction == ENTITY_LEFT ? true : false;

    // Assign loaded player context to game context
    game_context->player_context = pctx;
    free(sprite_context);
}

static int vgm_increase(float value, float increase)
{
    const int val = abs((int)(round(value + increase) / 2));
    return val < 1 ? 1 : val;
}

static void vgm_direction(Imu *imu, PlayerContext *player, Vector *pos)
{
    const float pitch = -imu_pitch_get(imu);
    const float roll = -imu_roll_get(imu);
    const float min_x = atof_(vgm_levels[vgm_x_index]) + 5.0; // minimum of 3
    const float min_y = atof_(vgm_levels[vgm_y_index]) + 5.0; // minimum of 3
    if (pitch > min_x)
    {
        pos->x += vgm_increase(pitch, min_x);
        player->dx = 1;
        player->direction = ENTITY_RIGHT;
    }
    else if (pitch < -min_x)
    {
        pos->x += -vgm_increase(pitch, min_x);
        player->dx = -1;
        player->direction = ENTITY_LEFT;
    }
    if (roll > min_y)
    {
        pos->y += vgm_increase(roll, min_y);
        player->dy = 1;
        player->direction = ENTITY_DOWN;
    }
    else if (roll < -min_y)
    {
        pos->y += -vgm_increase(roll, min_y);
        player->dy = -1;
        player->direction = ENTITY_UP;
    }
}

static void player_update(Entity *self, GameManager *manager, void *context)
{
    if (!self || !manager || !context)
        return;

    PlayerContext *player = (PlayerContext *)context;
    InputState input = game_manager_input_get(manager);
    Vector pos = entity_pos_get(self);
    player->old_position = pos;
    GameContext *game_context = game_manager_game_context_get(manager);

    // Determine the player's level based on XP
    player->level = get_player_level_iterative(player->xp);
    player->strength = 10 + (player->level * 1);           // 1 strength per level
    player->max_health = 100 + ((player->level - 1) * 10); // 10 health per level

    // Store previous direction
    int prev_dx = player->dx;
    int prev_dy = player->dy;

    // Reset movement deltas each frame
    player->dx = 0;
    player->dy = 0;

    if (game_context->imu_present)
    {
        // update position using the IMU
        vgm_direction(game_context->imu, player, &pos);
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
        if (game_context->last_button == GameKeyUp)
            game_context->elapsed_button_timer += 1;
        else
            game_context->elapsed_button_timer = 0;

        if (!game_context->is_menu_open)
        {
            pos.y -= (1 + game_context->icon_offset);
            player->dy = -1;
            player->direction = ENTITY_UP;
        }
        else
        {
            // next menu view
            // we can only go up to info from settings
            game_context->menu_screen = GAME_MENU_INFO;
        }
        game_context->last_button = GameKeyUp;
    }
    if (input.held & GameKeyDown)
    {
        if (game_context->last_button == GameKeyDown)
            game_context->elapsed_button_timer += 1;
        else
            game_context->elapsed_button_timer = 0;

        if (!game_context->is_menu_open)
        {
            pos.y += (1 + game_context->icon_offset);
            player->dy = 1;
            player->direction = ENTITY_DOWN;
        }
        else
        {
            // next menu view
            // we can only go down to more from info
            game_context->menu_screen = GAME_MENU_MORE;
        }
        game_context->last_button = GameKeyDown;
    }
    if (input.held & GameKeyLeft)
    {
        if (game_context->last_button == GameKeyLeft)
            game_context->elapsed_button_timer += 1;
        else
            game_context->elapsed_button_timer = 0;

        if (!game_context->is_menu_open)
        {
            pos.x -= (1 + game_context->icon_offset);
            player->dx = -1;
            player->direction = ENTITY_LEFT;
        }
        else
        {
            // if the menu is open, move the selection left
            if (game_context->menu_selection < 1)
            {
                game_context->menu_selection += 1;
            }
        }
        game_context->last_button = GameKeyLeft;
    }
    if (input.held & GameKeyRight)
    {
        if (game_context->last_button == GameKeyRight)
            game_context->elapsed_button_timer += 1;
        else
            game_context->elapsed_button_timer = 0;

        if (!game_context->is_menu_open)
        {
            pos.x += (1 + game_context->icon_offset);
            player->dx = 1;
            player->direction = ENTITY_RIGHT;
        }
        else
        {
            // if the menu is open, move the selection right
            if (game_context->menu_selection < 1)
            {
                game_context->menu_selection += 1;
            }
        }
        game_context->last_button = GameKeyRight;
    }
    if (input.held & GameKeyOk)
    {
        if (game_context->last_button == GameKeyOk)
            game_context->elapsed_button_timer += 1;
        else
            game_context->elapsed_button_timer = 0;

        game_context->last_button = GameKeyOk;

        // if all enemies are dead, allow the "OK" button to switch levels
        // otherwise the "OK" button will be used to attack
        if (game_context->enemy_count == 0 && !game_context->is_switching_level)
        {
            game_context->is_switching_level = true;
            save_player_context(player);
            furi_delay_ms(100);
            game_manager_next_level_set(manager, next_level(manager));
            return;
        }

        // if the OK button is held for 1 seconds,show the menu
        if (game_context->elapsed_button_timer > (1 * game_context->fps))
        {
            // open up menu on the INFO screen
            game_context->menu_screen = GAME_MENU_INFO;
            game_context->menu_selection = 0;
            game_context->is_menu_open = true;
        }
    }
    if (input.held & GameKeyBack)
    {
        if (game_context->last_button == GameKeyBack)
            game_context->elapsed_button_timer += 1;
        else
            game_context->elapsed_button_timer = 0;

        game_context->last_button = GameKeyBack;

        if (game_context->is_menu_open)
        {
            game_context->is_menu_open = false;
        }

        // if the back button is held for 1 seconds, stop the game
        if (game_context->elapsed_button_timer > (1 * game_context->fps))
        {
            if (!game_context->is_menu_open)
            {
                save_player_context(player);
                furi_delay_ms(100);
                game_manager_game_stop(manager);
                return;
            }
        }
    }

    // adjust tutorial step
    if (game_context->game_mode == GAME_MODE_STORY)
    {
        switch (game_context->tutorial_step)
        {
        case 0:
            if (input.held & GameKeyLeft)
                game_context->tutorial_step++;
            break;
        case 1:
            if (input.held & GameKeyRight)
                game_context->tutorial_step++;
            break;
        case 2:
            if (input.held & GameKeyUp)
                game_context->tutorial_step++;
            break;
        case 3:
            if (input.held & GameKeyDown)
                game_context->tutorial_step++;
            break;
        case 5:
            if (input.held & GameKeyOk && game_context->is_menu_open)
                game_context->tutorial_step++;
            break;
        case 6:
            if (input.held & GameKeyBack)
                game_context->tutorial_step++;
            break;
        case 7:
            if (input.held & GameKeyBack)
                game_context->tutorial_step++;
            break;
        }
    }

    // Clamp the player's position to stay within world bounds
    pos.x = CLAMP(pos.x, WORLD_WIDTH - 5, 5);
    pos.y = CLAMP(pos.y, WORLD_HEIGHT - 5, 5);

    // Update player position
    entity_pos_set(self, pos);

    // If the player is not moving, retain the last movement direction
    if (player->dx == 0 && player->dy == 0)
    {
        player->dx = prev_dx;
        player->dy = prev_dy;
        player->state = ENTITY_IDLE;
    }
    else
        player->state = ENTITY_MOVING;
}

static void draw_tutorial(Canvas *canvas, GameManager *manager)
{
    GameContext *game_context = game_manager_game_context_get(manager);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 45, 12, "Tutorial");
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    switch (game_context->tutorial_step)
    {
    case 0:
        canvas_draw_str(canvas, 15, 20, "Press LEFT to move left");
        break;
    case 1:
        canvas_draw_str(canvas, 15, 20, "Press RIGHT to move right");
        break;
    case 2:
        canvas_draw_str(canvas, 15, 20, "Press UP to move up");
        break;
    case 3:
        canvas_draw_str(canvas, 15, 20, "Press DOWN to move down");
        break;
    case 4:
        canvas_draw_str(canvas, 0, 20, "Press OK + collide with an enemy to attack");
        break;
    case 5:
        canvas_draw_str(canvas, 15, 20, "Hold OK to open the menu");
        break;
    case 6:
        canvas_draw_str(canvas, 15, 20, "Press BACK to escape the menu");
        break;
    case 7:
        canvas_draw_str(canvas, 15, 20, "Hold BACK to save and exit");
        break;
    case 8:
        // end of tutorial so quit
        game_context->tutorial_step = 0;
        game_context->is_menu_open = false;
        game_context->is_switching_level = true;
        game_manager_game_stop(manager);
        return;
    default:
        break;
    }
}

static void player_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    if (!self || !context || !canvas || !manager)
        return;

    // Get game context
    GameContext *game_context = game_manager_game_context_get(manager);

    // Get player context
    PlayerContext *player = context;

    // Get player position
    Vector pos = entity_pos_get(self);

    // Calculate camera offset to center the player
    camera_x = pos.x - (SCREEN_WIDTH / 2);
    camera_y = pos.y - (SCREEN_HEIGHT / 2);

    // Clamp camera position to prevent showing areas outside the world
    camera_x = CLAMP(camera_x, WORLD_WIDTH - SCREEN_WIDTH, 0);
    camera_y = CLAMP(camera_y, WORLD_HEIGHT - SCREEN_HEIGHT, 0);

    // if player is moving right or left, draw the corresponding sprite
    if (player->direction == ENTITY_RIGHT || player->direction == ENTITY_LEFT)
    {
        canvas_draw_sprite(
            canvas,
            player->direction == ENTITY_RIGHT ? player->sprite_right : player->sprite_left,
            pos.x - camera_x - 5, // Center the sprite horizontally
            pos.y - camera_y - 5  // Center the sprite vertically
        );
        player->left = false;
    }
    else // otherwise
    {
        // Default to last sprite direction
        canvas_draw_sprite(
            canvas,
            player->left ? player->sprite_left : player->sprite_right,
            pos.x - camera_x - 5, // Center the sprite horizontally
            pos.y - camera_y - 5  // Center the sprite vertically
        );
    }

    // Draw the outer bounds adjusted by camera offset
    canvas_draw_frame(canvas, -camera_x, -camera_y, WORLD_WIDTH, WORLD_HEIGHT);

    // render tutorial
    if (game_context->game_mode == GAME_MODE_STORY)
    {
        draw_tutorial(canvas, manager);

        if (game_context->is_menu_open)
        {
            background_render(canvas, manager);
        }
    }
    else
    {
        // render background
        background_render(canvas, manager);
    }
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

static SpriteContext *sprite_generic_alloc(const char *id, const char *type, uint8_t width, uint8_t height)
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
    if (is_str(type, "player"))
    {
        snprintf(ctx->right_file_name, sizeof(ctx->right_file_name), "player_right_%s_%dx%dpx.fxbm", id, width, height);
        snprintf(ctx->left_file_name, sizeof(ctx->left_file_name), "player_left_%s_%dx%dpx.fxbm", id, width, height);
    }
    else if (is_str(type, "enemy"))
    {
        snprintf(ctx->right_file_name, sizeof(ctx->right_file_name), "enemy_right_%s_%dx%dpx.fxbm", id, width, height);
        snprintf(ctx->left_file_name, sizeof(ctx->left_file_name), "enemy_left_%s_%dx%dpx.fxbm", id, width, height);
    }
    else if (is_str(type, "npc"))
    {
        snprintf(ctx->right_file_name, sizeof(ctx->right_file_name), "npc_right_%s_%dx%dpx.fxbm", id, width, height);
        snprintf(ctx->left_file_name, sizeof(ctx->left_file_name), "npc_left_%s_%dx%dpx.fxbm", id, width, height);
    }
    return ctx;
}

SpriteContext *get_sprite_context(const char *name)
{
    if (is_str(name, "axe"))
        return sprite_generic_alloc("axe", "player", 15, 11);
    else if (is_str(name, "bow"))
        return sprite_generic_alloc("bow", "player", 13, 11);
    else if (is_str(name, "naked"))
        return sprite_generic_alloc("naked", "player", 10, 10);
    else if (is_str(name, "sword"))
        return sprite_generic_alloc("sword", "player", 15, 11);
    //
    else if (is_str(name, "cyclops"))
        return sprite_generic_alloc("cyclops", "enemy", 10, 11);
    else if (is_str(name, "ghost"))
        return sprite_generic_alloc("ghost", "enemy", 15, 15);
    else if (is_str(name, "ogre"))
        return sprite_generic_alloc("ogre", "enemy", 10, 13);
    //
    else if (is_str(name, "funny"))
        return sprite_generic_alloc("funny", "npc", 15, 21);

    // If no match is found
    FURI_LOG_E("Game", "Sprite not found: %s", name);
    return NULL;
}