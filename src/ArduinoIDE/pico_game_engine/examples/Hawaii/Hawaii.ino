#include "Arduino.h"
#include "PicoGameEngine.h"
#include "frames.h"
#include <stdint.h>
// from https://ia801300.us.archive.org/23/items/Architects_of_Tomorrow/2007-01-23-02-11-17_512kb.mp4
/*
- Board Manager: Raspberry Pi Pico
- Flash Size: 2MB (Sketch: 1984KB, FS: 64KB)
- CPU Speed: 200MHz
*/
int next_frame = 0;
int max_frame = 24;
int direction = 1;
const PROGMEM uint8_t *frame_data(uint8_t index)
{
    return index == 1 ? frame_1 : index == 2 ? frame_2
                              : index == 3   ? frame_3
                              : index == 4   ? frame_4
                              : index == 5   ? frame_5
                              : index == 6   ? frame_6
                              : index == 7   ? frame_7
                              : index == 8   ? frame_8
                              : index == 9   ? frame_9
                              : index == 10  ? frame_10
                              : index == 11  ? frame_11
                              : index == 12  ? frame_12
                              : index == 13  ? frame_13
                              : index == 14  ? frame_14
                              : index == 15  ? frame_15
                              : index == 16  ? frame_16
                              : index == 17  ? frame_17
                              : index == 18  ? frame_18
                              : index == 19  ? frame_19
                              : index == 20  ? frame_20
                              : index == 21  ? frame_21
                              : index == 22  ? frame_22
                              : index == 23  ? frame_23
                                             : frame_24;
}

static void player_render(Entity *self, Draw *draw, Game *game)
{
    // draw frame
    draw->image(Vector(0, 0), frame_data(next_frame), Vector(320, 240));

    next_frame += direction;
    if (next_frame == max_frame)
    {
        direction = -1;
    }
    if (next_frame == 1)
    {
        direction = 1;
    }
}
static void animation(Game *game, Level *level)
{
    // Create a blank entity
    Entity *player = new Entity("Player",
                                ENTITY_PLAYER,
                                Vector(-100, -100),
                                Vector(0, 0),
                                NULL, NULL, NULL, NULL, NULL,
                                NULL,
                                player_render,
                                NULL,
                                true);
    level->entity_add(player);
    next_frame = 1;
}
void setup()
{
    // Setup file system (must be called in setup)
    setup_fs();

    // Create the game instance with its name, start/stop callbacks, and colors.
    Game *game = new Game("Hawaii", Vector(320, 240), NULL, NULL, TFT_RED, TFT_WHITE, true);

    // set world size
    game->world_size = Vector(320, 240);

    // Create and add a level to the game.
    Level *level = new Level("Level 1", Vector(320, 240), game);
    game->level_add(level);

    // Add the player entity to the level
    animation(game, level);

    // Create the game engine (with target set at the amount of available frames)
    GameEngine *engine = new GameEngine("VGM Game Engine", max_frame, game);

    // Run the game engine's main loop.
    engine->run();
}

void loop()
{
    // nothing to do here
}