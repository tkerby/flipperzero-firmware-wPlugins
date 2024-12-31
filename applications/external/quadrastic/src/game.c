#include "game.h"

#include <core/record.h>
#include <furi.h>
#include <notification/notification.h>

#include "engine/engine.h"

#include "game_settings.h"

#include "levels/level_about/level_about.h"
#include "levels/level_game/level_game.h"
#include "levels/level_game_over/level_game_over.h"
#include "levels/level_menu/level_menu.h"
#include "levels/level_settings/level_settings.h"

static void game_start(GameManager* game_manager, void* _game_context) {
    GameContext* game_context = _game_context;

    game_read_settings(game_context);

    game_context->notification = furi_record_open(RECORD_NOTIFICATION);

    game_context->levels.menu = game_manager_add_level(game_manager, &level_menu);
    game_context->levels.about = game_manager_add_level(game_manager, &level_about);
    game_context->levels.game = game_manager_add_level(game_manager, &level_game);
    game_context->levels.game_over = game_manager_add_level(game_manager, &level_game_over);
    game_context->levels.settings = game_manager_add_level(game_manager, &level_settings);

    FURI_LOG_I(GAME_NAME, "Game has started");
}

static void game_stop(void* _game_context) {
    UNUSED(_game_context);
    furi_record_close(RECORD_NOTIFICATION);
    FURI_LOG_I(GAME_NAME, "Game over");
}

const Game game = {
    .target_fps = 30,
    .show_fps = false,
    .always_backlight = true,
    .start = game_start,
    .stop = game_stop,
    .context_size = sizeof(GameContext), // size of game context
};
