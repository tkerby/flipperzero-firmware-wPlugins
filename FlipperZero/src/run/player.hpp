#pragma once
#include "engine/draw.hpp"
#include "engine/entity.hpp"
#include "engine/game.hpp"
#include "engine/level.hpp"
#include "flipper_http/flipper_http.h"
#include <furi.h>
#include "run/general.hpp"
#include "run/loading.hpp"

typedef enum
{
    GameViewTitle = 0,        // title, start, and menu (menu)
    GameViewSystemMenu = 1,   // profile, settings, about (menu)
    GameViewLobbyMenu = 2,    // local or online (menu)
    GameViewGameLocal = 3,    // game view local (gameplay)
    GameViewGameOnline = 4,   // game view online (gameplay)
    GameViewWelcome = 5,      // welcome view
    GameViewLogin = 6,        // login view
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
    uint8_t getCurrentGameState() const noexcept { return gameState; }
    GameMainView getCurrentMainView() const { return currentMainView; }
    HTTPState getHttpState();
    bool httpRequestIsFinished();
    void render(Draw *canvas, Game *game) override;
    void setFlipWorldRun(FlipWorldRun *run) { flipWorldRun = run; }
    void setGameState(GameState state) { gameState = state; }
    bool setHttpState(HTTPState state);
    void setInputKey(InputKey key) { lastInput = key; }
    void update(Game *game) override;

private:
    TitleIndex currentTitleIndex = TitleIndexStory; // current title index (must be in the GameViewTitle)
    FlipWorldRun *flipWorldRun = nullptr;           // Reference to the main run instance
    MenuIndex currentMenuIndex = MenuIndexProfile;  // current menu index (must be in the GameViewSystemMenu)
    GameMainView currentMainView = GameViewWelcome; // current main view of the game
    GameState gameState = GameStatePlaying;         // current game state
    bool hasBeenPositioned = false;                 // Track if player has been positioned to prevent repeated resets
    bool inputHeld = false;                         // whether input is held
    bool justStarted = true;                        // whether the player just started the game
    bool justSwitchedLevels = false;                // whether the player just switched levels
    InputKey lastInput = InputKeyMAX;               // Last input key pressed
};