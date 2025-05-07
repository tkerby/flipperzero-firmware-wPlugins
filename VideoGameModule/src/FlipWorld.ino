#include "Arduino.h"
#include "PicoGameEngine.h"
#include "player.h"
#include "icon.h"
#include "assets.h"
/*
    Board Manager: Raspberry Pi Pico
    Flash Size: 2MB (Sketch: 1984KB, FS: 64KB)
    CPU Speed: 200MHz
*/
auto board = VGMConfig; // Video Game Module Configuration
void setup()
{
    // Setup file system (must be called in setup)
    setup_fs();

    // Create the game instance with its name, start/stop callbacks, and colors.
    Game *game = new Game(
        "FlipWorld",                       // Game name
        Vector(board.width, board.height), // Game size
        NULL,                              // start callback
        NULL,                              // stop callback
        TFT_BLACK,                         // Foreground color
        TFT_WHITE,                         // Background color
        true,                              // Use 8-bit graphics?
        board,                             // Board configuration
        true                               // Use double buffering for TFT?
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
