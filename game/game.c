#include <game/game.h>
#include <game/storage.h>

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
    game_context->fps = game_fps_choices_2[game_fps_index];
    game_context->player_context = NULL;

    // open the world list from storage, then create a level for each world
    char file_path[128];
    snprintf(file_path, sizeof(file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list.json");
    FuriString *world_list = flipper_http_load_from_file(file_path);
    if (!world_list)
    {
        FURI_LOG_E("Game", "Failed to load world list");
        game_context->levels[0] = game_manager_add_level(game_manager, generic_level("town_world_v2", 0));
        game_context->level_count = 1;
        return;
    }
    for (int i = 0; i < 10; i++)
    {
        FuriString *world_name = get_json_array_value_furi("worlds", i, world_list);
        if (!world_name)
        {
            break;
        }
        game_context->levels[i] = game_manager_add_level(game_manager, generic_level(furi_string_get_cstr(world_name), i));
        furi_string_free(world_name);
        game_context->level_count++;
    }
    furi_string_free(world_list);

    game_context->current_level = 0;
}

/*
    Write here the stop code for your game, for example, freeing memory, if it was allocated.
    You don't need to free level, sprites or entities, it will be done automatically.
    Also, you don't need to free game_context, it will be done automatically, after this function.
*/
static void game_stop(void *ctx)
{
    GameContext *game_context = ctx;
    save_player_context(game_context->player_context);
    if (game_context->player_context)
    {
        free(game_context->player_context);
    }
    for (int i = 0; i < game_context->level_count; i++)
    {
        game_context->levels[i] = NULL;
    }
    game_context->level_count = 0;
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
