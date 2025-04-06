#include "Arduino.h"
#include "VGMGameEngine.h"
#include "player.h"
#include "icon.h"
#include "assets.h"
/*
    Board Manager: Raspberry Pi Pico
    Flash Size: 2MB (Sketch: 1984KB, FS: 64KB)
    CPU Speed: 200MHz
*/
void setup()
{
    // Setup file system (must be called in setup)
    setup_fs();

    // Create the game instance with its name, start/stop callbacks, and colors.
    Game *game = new Game(
        "FlipWorld",      // name
        Vector(320, 240), // size
        NULL,             // start
        NULL,             // stop
        TFT_RED,          // foreground
        TFT_WHITE,        // background
        true              // use 8-bit?
    );

    // set world size
    game->world_size = Vector(768, 384);

    // UART buttons
    ButtonUART *uart = new ButtonUART();

    // Add input buttons
    game->input_add(new Input(uart));

    // Create and add a level to the game.
    Level *level = new Level("Level 1", Vector(768, 384), game, NULL, NULL);
    game->level_add(level);

    // set game position to center of player
    game->pos = Vector(384, 192);
    game->old_pos = game->pos;

    // spawn icons from json
    icon_spawn_json(level, shadow_woods_v4);

    // spawn enemys from json
    enemy_spawn_json(level, shadow_woods_v4);

    // Add the player entity to the level
    player_spawn(level, "sword", Vector(384, 192));

    // Create the game engine (with 30 frames per second target).
    GameEngine *engine = new GameEngine("Pico Game Engine", 30, game);

    // Run the game engine's main loop.
    engine->run();
}

void loop()
{
    // nothing to do here
}
