#include "Arduino.h"
#include "PicoGameEngine.h"
#include "player.hpp"
// Translated from https://github.com/xMasterX/all-the-plugins/blob/dev/base_pack/arkanoid/arkanoid_game.c
// All credits to @xMasterX @gotnull

/*
- Board Manager: Raspberry Pi Pico
- Flash Size: 2MB (Sketch: 1984KB, FS: 64KB)
- CPU Speed: 200MHz
*/
auto board = VGMConfig; // Video Game Module Configuration
void setup()
{
    // Setup file system (must be called in setup)
    setup_fs();

    // Create the game instance with its name, start/stop callbacks, and colors.
    Game *game = new Game(
        "VGM Game Engine",                 // Game name
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
    game->world_size = Vector(board.width, board.height);

    // UART buttons
    ButtonUART *uart = new ButtonUART();

    // Add input buttons
    game->input_add(new Input(uart));

    // Create and add a level to the game.
    Level *level = new Level("Level 1", Vector(board.width, board.height), game);
    game->level_add(level);

    // Add the player entity to the level
    player_spawn(level, game);

    // Create the game engine (with 120 frames per second target).
    GameEngine *engine = new GameEngine("VGM Game Engine", 120, game);

    // Run the game engine's main loop.
    engine->run();
}

void loop()
{
    // nothing to do here
}