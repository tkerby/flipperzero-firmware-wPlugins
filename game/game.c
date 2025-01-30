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
    game_context->fps = atof_(fps_choices_str[fps_index]);
    game_context->player_context = NULL;
    game_context->ended_early = false;
    game_context->current_level = 0;
    game_context->level_count = 0;
    game_context->enemy_count = 0;
    game_context->npc_count = 0;

    // set all levels to NULL
    for (int i = 0; i < MAX_LEVELS; i++)
        game_context->levels[i] = NULL;

    // set all enemies to NULL
    for (int i = 0; i < MAX_ENEMIES; i++)
        game_context->enemies[i] = NULL;

    // set all npcs to NULL
    for (int i = 0; i < MAX_NPCS; i++)
        game_context->npcs[i] = NULL;

    // attempt to allocate all levels
    for (int i = 0; i < MAX_LEVELS; i++)
    {
        if (!allocate_level(game_manager, i))
        {
            if (i == 0)
            {
                game_context->levels[0] = game_manager_add_level(game_manager, training_world());
                game_context->level_count = 1;
            }
            break;
        }
        else
            game_context->level_count++;
    }

    // imu
    game_context->imu = imu_alloc();
    game_context->imu_present = imu_present(game_context->imu);
}

/*
    Write here the stop code for your game, for example, freeing memory, if it was allocated.
    You don't need to free level, sprites or entities, it will be done automatically.
    Also, you don't need to free game_context, it will be done automatically, after this function.
*/
static void game_stop(void *ctx)
{
    furi_check(ctx);
    GameContext *game_context = ctx;
    imu_free(game_context->imu);
    game_context->imu = NULL;

    if (game_context->player_context)
    {
        if (!game_context->ended_early)
            easy_flipper_dialog("Game Over", "Thanks for playing FlipWorld!\nHit BACK then wait for\nthe game to save.");
        else
            easy_flipper_dialog("Game Over", "Ran out of memory so the\ngame ended early.\nHit BACK to exit.");
        save_player_context_api(game_context->player_context);
        easy_flipper_dialog("Game Saved", "Hit BACK to exit.");
    }
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
