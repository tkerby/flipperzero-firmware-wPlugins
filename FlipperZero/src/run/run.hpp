#pragma once
#include "easy_flipper/easy_flipper.h"
#include "engine/engine.hpp"
#include "run/general.hpp"
#include "run/player.hpp"

/* Last thing to do is Multiplayer Implementation (PVE mode)

Steps:
1. Login/Fetch User Info as usual (done)
2. Fetch and display the list of available lobbies/servers (done)
3. Start a websocket session to the selected lobby (app->websocketStart())
4. Handle incoming messages from the websocket to update the game state/draw other users/set other users as enemies

*/

class FlipWorldApp;

class FlipWorldRun
{
private:
    std::unique_ptr<Draw> draw;         // Draw instance
    std::unique_ptr<GameEngine> engine; // Engine instance
    bool inputHeld = false;             // Flag to check if input is held
    bool isGameRunning = false;         // Flag to check if the game is running
    InputKey lastInput = InputKeyMAX;   // Last input key pressed
    std::unique_ptr<Player> player;     // Player instance
    bool shouldReturnToMenu = false;    // Flag to signal return to menu
    //
    int atoi(const char *nptr) { return (int)strtol(nptr, NULL, 10); } // convert string to integer
    void debounceInput();                                              // debounce input to prevent multiple actions from a single press
    void inputManager();                                               // manage input for the game, called from updateInput
public:
    FlipWorldRun();
    ~FlipWorldRun();
    //
    void *appContext = nullptr;                   // Pointer to the application context for accessing app-specific functionality
    IconGroupContext *currentIconGroup = nullptr; // Pointer to the current icon group context
    bool shouldDebounce = false;                  // public for Player access
    //
    void endGame();                                                                // end the game and return to the submenu
    InputKey getCurrentInput() const { return lastInput; }                         // Get the last input key pressed
    const char *getLevelJson(LevelIndex index) const;                              // Get the JSON data for a level by index
    GameEngine *getEngine() const { return engine.get(); }                         // Get the game engine instance
    Draw *getDraw() const { return draw.get(); }                                   // Get the Draw instance
    LevelIndex getCurrentLevelIndex() const;                                       // Get the current level index
    IconSpec getIconSpec(const char *name) const;                                  // Get the icon specification by name
    std::unique_ptr<Level> getLevel(LevelIndex index, Game *game = nullptr) const; // Get a level by index
    const char *getLevelName(LevelIndex index) const;                              // Get the name of a level by index
    bool isActive() const { return shouldReturnToMenu == false; }                  // Check if the game is active
    bool isRunning() const { return isGameRunning; }                               // Check if the game engine is running
    void resetInput() { lastInput = InputKeyMAX; }                                 // Reset input after processing
    bool setAppContext(void *context);                                             // Set the application context for accessing app-specific functionality
    bool setIconGroup(LevelIndex index);                                           // Set the current icon group based on the level index
    bool startGame();                                                              // start the actual game
    void updateDraw(Canvas *canvas);                                               // update and draw the run
    void updateInput(InputEvent *event);                                           // update input for the run
};
