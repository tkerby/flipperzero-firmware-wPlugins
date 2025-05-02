#include <Arduino.h>
#include "VGMGameEngine.h"
// Note: Use the example_8bit for no screen flickering/tearing
/*
    Board Manager: Raspberry Pi Pico
    Flash Size: 2MB (Sketch: 1984KB, FS: 64KB)
    CPU Speed: 200MHz
*/
auto board = VGMConfig; // Video Game Module Configuration
const PROGMEM uint8_t player_10x10px[200] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF};

/* Update the player entity using current game input */
void player_update(Entity *self, Game *game)
{
    Vector oldPos = self->position;
    Vector newPos = oldPos;

    // Move according to input
    if (game->input == BUTTON_UP)
        newPos.y -= 10;
    else if (game->input == BUTTON_DOWN)
        newPos.y += 10;
    else if (game->input == BUTTON_LEFT)
        newPos.x -= 10;
    else if (game->input == BUTTON_RIGHT)
        newPos.x += 10;

    // set new position
    self->position_set(newPos);

    // check if new position is within the level boundaries
    if (newPos.x < 0 || newPos.x + self->size.x > game->current_level->size.x ||
        newPos.y < 0 || newPos.y + self->size.y > game->current_level->size.y)
    {
        // restore old position
        self->position_set(oldPos);
    }

    // Store the current camera position before updating
    game->old_pos = game->pos;

    // Update camera position to center the player
    float camera_x = self->position.x - (game->size.x / 2);
    float camera_y = self->position.y - (game->size.y / 2);

    // Clamp camera position to the world boundaries
    camera_x = constrain(camera_x, 0, game->current_level->size.x - game->size.x);
    camera_y = constrain(camera_y, 0, game->current_level->size.y - game->size.y);

    // Set the new camera position
    game->pos = Vector(camera_x, camera_y);
}

/* Render the player entity along with game information */
void player_render(Entity *player, Draw *draw, Game *game)
{
    /*
        Draw anything extra here
        The engine will draw the player entity
    */
    game->draw->text(Vector(110, 10), "VGM Game Engine");
}

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
        TFT_RED,                           // Foreground color
        TFT_WHITE,                         // Background color
        false,                             // Use 8-bit graphics?
        board,                             // Board configuration
        false                              // Use double buffering for TFT
    );

    // set world size
    game->world_size = game->size;

    // UART buttons
    ButtonUART *uart = new ButtonUART();

    // Add input buttons
    game->input_add(new Input(uart));

    // Create and add a level to the game.
    Level *level = new Level("Level 1", Vector(board.width, board.height), game);
    game->level_add(level);

    Entity *player = new Entity(
        "Player",
        ENTITY_PLAYER,
        Vector(board.width / 2, board.height / 2), // Initial position
        Vector(10, 10),
        player_10x10px,
        NULL,          // No sprite left
        NULL,          // No sprite right
        NULL,          // No custom initialization routine
        NULL,          // No custom destruction routine
        player_update, // Update callback
        player_render, // Render callback
        NULL,          // No collision callback
        false          // 8-bit sprite?
    );

    // Add the player entity to the level
    level->entity_add(player);

    // Create the game engine (with 30 frames per second target).
    GameEngine *engine = new GameEngine("VGM Game Engine", 30, game);

    // Run the game engine's main loop.
    engine->run();
}

void loop()
{
    // nothing to do here
}
