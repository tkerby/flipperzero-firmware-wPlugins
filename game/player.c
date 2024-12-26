#include <game/player.h>
#include <game/storage.h>
/****** Entities: Player ******/

static Level *get_next_level(GameManager *manager)
{
    Level *current_level = game_manager_current_level_get(manager);
    GameContext *game_context = game_manager_game_context_get(manager);
    for (int i = 0; i < game_context->level_count; i++)
    {
        if (game_context->levels[i] == current_level)
        {
            // check if i+1 is out of bounds, if so, return the first level
            game_context->current_level = (i + 1) % game_context->level_count;
            return game_context->levels[(i + 1) % game_context->level_count] ? game_context->levels[(i + 1) % game_context->level_count] : game_context->levels[0];
        }
    }
    game_context->current_level = 0;
    return game_context->levels[0] ? game_context->levels[0] : game_manager_add_level(manager, generic_level("town_world", 0));
}

void player_spawn(Level *level, GameManager *manager)
{
    GameContext *game_context = game_manager_game_context_get(manager);
    game_context->players[0] = level_add_entity(level, &player_desc);

    // Set player position.
    entity_pos_set(game_context->players[0], (Vector){WORLD_WIDTH / 2, WORLD_HEIGHT / 2});

    // Add collision box to player entity
    // Box is centered in player x and y, and it's size is 10x10
    entity_collider_add_rect(game_context->players[0], 10 + PLAYER_COLLISION_HORIZONTAL, 10 + PLAYER_COLLISION_VERTICAL);

    // Get player context
    PlayerContext *player_context = entity_context_get(game_context->players[0]);
    PlayerContext *loaded_player_context = load_player_context();

    if (!loaded_player_context)
    {
        // Load player sprite
        player_context->sprite_right = game_manager_sprite_load(manager, "player_right.fxbm");
        player_context->sprite_left = game_manager_sprite_load(manager, "player_left.fxbm");
        player_context->direction = PLAYER_RIGHT; // default direction
        player_context->health = 100;
        player_context->strength = 10;
        player_context->level = 1;
        player_context->xp = 0;
        player_context->start_position = entity_pos_get(game_context->players[0]);
        player_context->attack_timer = 0.5f;
        player_context->elapsed_attack_timer = player_context->attack_timer;
        player_context->health_regen = 1; // 1 health per second
        player_context->elapsed_health_regen = 0;
        player_context->max_health = 100 + ((player_context->level - 1) * 10); // 10 health per level

        // Set player username
        if (!load_char("Flip-Social-Username", player_context->username, 32))
        {
            snprintf(player_context->username, 32, "Player");
        }

        game_context->player_context = player_context;
        return;
    }

    // Copy loaded player context to player context
    memcpy(player_context, loaded_player_context, sizeof(PlayerContext));
    game_context->player_context = player_context;

    // Free loaded player context
    free(loaded_player_context);

    // Load player sprite (we'll add this to the JSON later when players can choose their sprite)
    player_context->sprite_right = game_manager_sprite_load(manager, "player_right.fxbm");
    player_context->sprite_left = game_manager_sprite_load(manager, "player_left.fxbm");

    // save the player context to storage
    save_player_context(player_context);
}

// Modify player_update to track direction
static void player_update(Entity *self, GameManager *manager, void *context)
{
    PlayerContext *player = (PlayerContext *)context;
    InputState input = game_manager_input_get(manager);
    Vector pos = entity_pos_get(self);
    GameContext *game_context = game_manager_game_context_get(manager);

    // apply health regeneration
    player->elapsed_health_regen += 1.0f / game_context->fps;
    if (player->elapsed_health_regen >= 1.0f && player->health < player->max_health)
    {
        player->health += (player->health_regen + player->health > player->max_health) ? player->max_health - player->health : player->health_regen;
        player->elapsed_health_regen = 0;
    }

    // Increment the elapsed_attack_timer for the player
    player->elapsed_attack_timer += 1.0f / game_context->fps;

    // Store previous direction
    int prev_dx = player->dx;
    int prev_dy = player->dy;

    // Reset movement deltas each frame
    player->dx = 0;
    player->dy = 0;

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
    if (input.held & GameKeyOk)
    {
        // if all enemies are dead, allow the "OK" button to switch levels
        // otherwise the "OK" button will be used to attack
        if (game_context->enemy_count == 0)
        {
            game_manager_next_level_set(manager, get_next_level(manager));
            furi_delay_ms(500);
        }
        else
        {
            game_context->user_input = GameKeyOk;
            furi_delay_ms(100);
        }
        return;
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
    // Get player context
    UNUSED(manager);
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

    // draw username over player's head
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    canvas_draw_str(canvas, pos.x - camera_x - (strlen(player->username) * 2), pos.y - camera_y - 7, player->username);
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