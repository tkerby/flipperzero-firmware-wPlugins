#include "Arduino.h"
#include "VGMGameEngine.h"
#include "assets.h"
#include "player.h"
// Translated from https://github.com/xMasterX/all-the-plugins/tree/dev/apps_source_code/t-rex-runner
// All credits to @Rrycbarm and @xMasterX
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
    Game *game = new Game("T-Rex Runner", Vector(320, 240), NULL, NULL, TFT_BLACK, TFT_WHITE);

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
    player_spawn(level);

    // Create the game engine (with 30 frames per second target).
    GameEngine *engine = new GameEngine("VGM Game Engine", 30, game);

    // Run the game engine's main loop.
    engine->run();
}

void loop()
{
    // nothing to do here
}