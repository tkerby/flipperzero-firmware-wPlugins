#include "game.h"

/****** Entities: Player ******/
static Level *levels[10];
static int level_count = 0;
static bool has_pressed_ok = false;

static Level *get_next_level(GameManager *manager)
{
    has_pressed_ok = false;
    Level *current_level = game_manager_current_level_get(manager);
    for (int i = 0; i < level_count; i++)
    {
        if (levels[i] == current_level)
        {
            // check if i+1 is out of bounds, if so, return the first level
            return levels[(i + 1) % level_count] ? levels[(i + 1) % level_count] : levels[0];
        }
    }
    return levels[0] ? levels[0] : game_manager_add_level(manager, generic_level("town_world", 0));
}

void player_spawn(Level *level, GameManager *manager)
{
    Entity *player = level_add_entity(level, &player_desc);

    // Set player position.
    // Depends on your game logic, it can be done in start entity function, but also can be done here.
    entity_pos_set(player, (Vector){WORLD_WIDTH / 2, WORLD_HEIGHT / 2});

    // Add collision box to player entity
    // Box is centered in player x and y, and it's size is 10x10
    entity_collider_add_rect(player, 10 + PLAYER_COLLISION_HORIZONTAL, 10 + PLAYER_COLLISION_VERTICAL);

    // Get player context
    PlayerContext *player_context = entity_context_get(player);

    // Load player sprite
    player_context->sprite_right = game_manager_sprite_load(manager, "player_right.fxbm");
    player_context->sprite_left = game_manager_sprite_load(manager, "player_left.fxbm");
    player_context->is_looking_left = false; // player starts looking right
}

// Modify player_update to track direction
static void player_update(Entity *self, GameManager *manager, void *context)
{
    PlayerContext *player = (PlayerContext *)context;
    InputState input = game_manager_input_get(manager);
    Vector pos = entity_pos_get(self);

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
    }
    if (input.held & GameKeyDown)
    {
        pos.y += 2;
        player->dy = 1;
    }
    if (input.held & GameKeyLeft)
    {
        pos.x -= 2;
        player->dx = -1;
        player->is_looking_left = true;
    }
    if (input.held & GameKeyRight)
    {
        pos.x += 2;
        player->dx = 1;
        player->is_looking_left = false;
    }

    // switch levels if holding OK
    if (input.held & GameKeyOk)
    {
        if (!has_pressed_ok)
        {
            has_pressed_ok = true;
            game_manager_next_level_set(manager, get_next_level(manager));
            furi_delay_ms(500);
        }
        return;
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
        player->is_looking_left ? player->sprite_left : player->sprite_right,
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

/****** Game ******/
/*
    Write here the start code for your game, for example: creating a level and so on.
    Game context is allocated (game.context_size) and passed to this function, you can use it to store your game data.
*/
static void game_start(GameManager *game_manager, void *ctx)
{
    // Do some initialization here, for example you can load score from storage.
    // For simplicity, we will just set it to 0.
    GameContext *game_context = ctx;
    game_context->score = 0;

    // open the world list from storage, then create a level for each world
    char file_path[128];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list.json");
    FuriString *world_list = flipper_http_load_from_file(file_path);
    if (!world_list)
    {
        FURI_LOG_E("Game", "Failed to load world list");
        levels[0] = game_manager_add_level(game_manager, generic_level("town_world", 0));
        level_count = 1;
        return;
    }
    for (int i = 0; i < 10; i++)
    {
        FuriString *world_name = get_json_array_value_furi("worlds", i, world_list);
        if (!world_name)
        {
            break;
        }
        levels[i] = game_manager_add_level(game_manager, generic_level(furi_string_get_cstr(world_name), i));
        furi_string_free(world_name);
        level_count++;
    }
    furi_string_free(world_list);
}

/*
    Write here the stop code for your game, for example, freeing memory, if it was allocated.
    You don't need to free level, sprites or entities, it will be done automatically.
    Also, you don't need to free game_context, it will be done automatically, after this function.
*/
static void game_stop(void *ctx)
{
    UNUSED(ctx);
    // If you want to do other final logic (like saving scores), do it here.
    // But do NOT free levels[] if the engine manages them.

    // Just clear out your pointer array if you like (not strictly necessary)
    for (int i = 0; i < level_count; i++)
    {
        levels[i] = NULL;
    }
    level_count = 0;
}

/*
    Your game configuration, do not rename this variable, but you can change its content here.
*/

const Game game = {
    .target_fps = 0,                     // set to 0 because we set this in game_app (callback.c line 22)
    .show_fps = false,                   // show fps counter on the screen
    .always_backlight = true,            // keep display backlight always on
    .start = game_start,                 // will be called once, when game starts
    .stop = game_stop,                   // will be called once, when game stops
    .context_size = sizeof(GameContext), // size of game context
};
