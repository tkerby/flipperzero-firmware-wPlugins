#pragma once
#include "engine/draw.hpp"
#include "engine/entity.hpp"
#include "engine/game.hpp"
#include "engine/level.hpp"
#include "flipper_http/flipper_http.h"
#include <furi.h>
#include "run/assets.hpp"
#include "run/general.hpp"
#include "run/loading.hpp"

typedef enum
{
    GameViewTitle = 0,        // story, pvp, and pve (menu)
    GameViewSystemMenu = 1,   // profile, settings (menu)
    GameViewGame = 2,         // story mode
    GameViewLogin = 3,        // login view
    GameViewRegistration = 7, // registration view
    GameViewUserInfo = 8      // user info view
} GameMainView;

class FlipWorldRun;

class Player : public Entity
{
public:
    Player();
    ~Player();

    char player_name[64] = {0};
    //
    void drawCurrentView(Draw *canvas);
    GameState getCurrentGameState() const noexcept { return gameState; }
    GameMainView getCurrentMainView() const { return currentMainView; }
    TitleIndex getCurrentTitleIndex() const noexcept { return currentTitleIndex; }
    HTTPState getHttpState();
    IconGroupContext *getIconGroupContext() const; // get the current icon group context from FlipWorldRun
    bool httpRequestIsFinished();
    void iconGroupRender(Game *game);
    void processInput();
    void render(Draw *canvas, Game *game) override;
    void setFlipWorldRun(FlipWorldRun *run) { flipWorldRun = run; }
    void setGameState(GameState state) { gameState = state; }
    bool setHttpState(HTTPState state);
    void setInputKey(InputKey key) { lastInput = key; }
    bool shouldLeaveGame() const noexcept { return leaveGame == ToggleOn; }
    void update(Game *game) override;

private:
    TitleIndex currentTitleIndex = TitleIndexStory;      // current title index (must be in the GameViewTitle)
    FlipWorldRun *flipWorldRun = nullptr;                // Reference to the main run instance
    MenuIndex currentSystemMenuIndex = MenuIndexProfile; // current menu index (must be in the GameViewSystemMenu)
    GameMainView currentMainView = GameViewTitle;        // current main view of the game
    GameState gameState = GameStatePlaying;              // current game state
    bool hasBeenPositioned = false;                      // Track if player has been positioned to prevent repeated resets
    bool inputHeld = false;                              // whether input is held
    bool justStarted = true;                             // whether the player just started the game
    bool justSwitchedLevels = false;                     // whether the player just switched levels
    InputKey lastInput = InputKeyMAX;                    // Last input key pressed
    ToggleState leaveGame = ToggleOff;                   // leave game toggle state
    uint8_t rainFrame = 0;                               // frame counter for rain effect

    void drawRainEffect(Draw *canvas);            // draw the rain effect
    void drawSystemMenuView(Draw *canvas);        // draw the system menu view
    void drawTitleView(Draw *canvas);             // draw the title view
    void drawUsername(Vector pos, Game *game);    // draw the username at the specified position
    void drawUserStats(Vector pos, Draw *canvas); // draw the user stats at the specified position
    void updateStats();                           // update player stats
};