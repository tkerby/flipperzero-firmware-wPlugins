#include <gui/view_holder.h>
#include <game/game.h>
#include <game/storage.h>
#include <alloc/alloc.h>

/****** Game ******/
/*
    Write here the start code for your game, for example: creating a level and so on.
    Game context is allocated (game.context_size) and passed to this function, you can use it to store your game data.
*/
static void game_start(GameManager *game_manager, void *ctx)
{
    // Do some initialization here, for example you can load score from storage.
    // check if enough memory
    if (!is_enough_heap(sizeof(GameContext), true))
    {
        FURI_LOG_E("Game", "Not enough heap memory.. ending game early.");
        GameContext *game_context = ctx;
        game_context->ended_early = true;
        game_manager_game_stop(game_manager); // end game early
        return;
    }
    // For simplicity, we will just set it to 0.
    GameContext *game_context = ctx;
    game_context->fps = atof_(fps_choices_str[fps_index]);
    game_context->player = NULL;
    game_context->ended_early = false;
    game_context->current_level = 0;
    game_context->level_count = 0;
    game_context->enemy_count = 0;
    game_context->npc_count = 0;

    game_context->game_mode = game_mode_index;

    // set all levels to NULL
    for (int i = 0; i < MAX_LEVELS; i++)
        game_context->levels[i] = NULL;

    // set all enemies to NULL
    for (int i = 0; i < MAX_ENEMIES; i++)
        game_context->enemies[i] = NULL;

    // set all npcs to NULL
    for (int i = 0; i < MAX_NPCS; i++)
        game_context->npcs[i] = NULL;

    if (game_context->game_mode == GAME_MODE_PVE)
    {
        // attempt to allocate all levels
        for (int i = 0; i < MAX_LEVELS; i++)
        {
            if (!allocate_level(game_manager, i))
            {
                if (i == 0)
                {
                    game_context->levels[0] = game_manager_add_level(game_manager, world_training());
                    game_context->level_count = 1;
                }
                break;
            }
            else
                game_context->level_count++;
        }
    }
    else if (game_context->game_mode == GAME_MODE_STORY)
    {
        // show tutorial only for now
        game_context->levels[0] = game_manager_add_level(game_manager, world_training());
        game_context->level_count = 1;
    }
    else if (game_context->game_mode == GAME_MODE_PVP)
    {
        // show pvp
        game_context->levels[0] = game_manager_add_level(game_manager, world_pvp());
        game_context->level_count = 1;
    }

    // imu
    game_context->imu = imu_alloc();
    game_context->imu_present = imu_present(game_context->imu);

    // FlipperHTTP
    if (game_context->game_mode == GAME_MODE_PVP)
    {
        // check if enough memory
        if (!is_enough_heap(sizeof(FlipperHTTP), true))
        {
            FURI_LOG_E("Game", "Not enough heap memory.. ending game early.");
            game_context->ended_early = true;
            game_manager_game_stop(game_manager); // end game early
            return;
        }
        game_context->fhttp = flipper_http_alloc();
        if (!game_context->fhttp)
        {
            FURI_LOG_E("Game", "Failed to allocate FlipperHTTP");
            game_context->ended_early = true;
            game_manager_game_stop(game_manager); // end game early
            return;
        }
    }
}

static void thanks(Canvas *canvas, void *context)
{
    UNUSED(context);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 35, 8, "Saving game");
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 0, 50, "Please wait while your");
    canvas_draw_str(canvas, 0, 60, "game is saved.");
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
    const size_t heap_size = memmgr_heap_get_max_free_block();
    imu_free(game_context->imu);
    game_context->imu = NULL;

    // clear current level early
    if (game_context->levels[game_context->current_level])
    {
        level_clear(game_context->levels[game_context->current_level]);
    }

    if (game_context->game_mode == GAME_MODE_PVP)
    {
        if (game_context->fhttp)
        {
            flipper_http_websocket_stop(game_context->fhttp); // close websocket
            remove_player_from_lobby(game_context->fhttp);    // remove player from lobby
            flipper_http_free(game_context->fhttp);
        }
    }

    PlayerContext *player_context = malloc(sizeof(PlayerContext));
    if (!player_context)
    {
        FURI_LOG_E("Game", "Failed to allocate PlayerContext");
        return;
    }

    if (!game_context->ended_early)
        easy_flipper_dialog(
            "Game Over",
            "Thanks for playing FlipWorld!\nHit BACK then wait for\nthe game to save.");
    else
    {
        char message[128];
        snprintf(message, sizeof(message), "Ran out of memory so the\ngame ended early. There were\n%zu bytes free.\n\nHit BACK to exit.", heap_size);
        easy_flipper_dialog("Game Over", message);
    }

    // save the player context
    if (load_player_context(player_context))
    {
        ViewPort *view_port = view_port_alloc();
        view_port_draw_callback_set(view_port, thanks, NULL);
        Gui *gui = furi_record_open(RECORD_GUI);
        gui_add_view_port(gui, view_port, GuiLayerFullscreen);
        uint32_t tick_count = furi_get_tick();
        furi_delay_ms(800);

        // save the player context to the API
        game_context->fhttp = flipper_http_alloc();
        if (game_context->fhttp)
        {
            save_player_context_api(player_context, game_context->fhttp);
            flipper_http_free(game_context->fhttp);
        }

        const uint32_t delay = 3500;
        tick_count = (tick_count + delay) - furi_get_tick();
        if (tick_count <= delay)
        {
            furi_delay_ms(tick_count);
        }

        easy_flipper_dialog("Game Saved", "Hit BACK to exit.");

        flip_world_show_submenu();

        gui_remove_view_port(gui, view_port);
        furi_record_close(RECORD_GUI);
    }

    // free the player context
    if (player_context)
    {
        free(player_context);
        player_context = NULL;
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
