#include "Arduino.h"
#include "VGMGameEngine.h"
#include "player.h"
// Translated from https://github.com/bmstr-ru/furious-birds/blob/main/furious_birds.c
// All credits to @bmstr-ru (Dmitriy Ermashov)

/*
- Board Manager: Raspberry Pi Pico
- Flash Size: 2MB (Sketch: 1984KB, FS: 64KB)
- CPU Speed: 200MHz
*/

void setup()
{
    // Setup file system (must be called in setup)
    setup_fs();

    // Create the game instance with its name, start/stop callbacks, and colors.
    Game *game = new Game("Furious Birds", Vector(320, 240), NULL, NULL, TFT_BLACK, TFT_WHITE);

    // set world size
    game->world_size = Vector(320, 240);

    // UART buttons
    ButtonUART *uart = new ButtonUART();

    // Add input buttons
    game->input_add(new Input(uart));

    // Create and add a level to the game.
    Level *level = new Level("Level 1", Vector(320, 240), game);
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