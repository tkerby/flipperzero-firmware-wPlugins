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

typedef enum
{
    LoginCredentialsMissing = -1, // Credentials missing
    LoginSuccess = 0,             // Login successful
    LoginUserNotFound = 1,        // User not found
    LoginWrongPassword = 2,       // Wrong password
    LoginWaiting = 3,             // Waiting for response
    LoginNotStarted = 4,          // Login not started
    LoginRequestError = 5,        // Request error
} LoginStatus;

typedef enum
{
    RegistrationCredentialsMissing = -1, // Credentials missing
    RegistrationSuccess = 0,             // Registration successful
    RegistrationUserExists = 1,          // User already exists
    RegistrationRequestError = 2,        // Request error
    RegistrationNotStarted = 3,          // Registration not started
    RegistrationWaiting = 4,             // Waiting for response
} RegistrationStatus;

typedef enum
{
    UserInfoCredentialsMissing = -1, // Credentials missing
    UserInfoSuccess = 0,             // User info fetched successfully
    UserInfoRequestError = 1,        // Request error
    UserInfoNotStarted = 2,          // User info request not started
    UserInfoWaiting = 3,             // Waiting for response
    UserInfoParseError = 4,          // Error parsing user info
} UserInfoStatus;

typedef enum
{
    RequestTypeLogin = 0,        // Request login
    RequestTypeRegistration = 1, // Request registration
    RequestTypeUserInfo = 2,     // Request user info
} RequestType;

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
    void userRequest(RequestType requestType);

private:
    TitleIndex currentTitleIndex = TitleIndexStory;                 // current title index (must be in the GameViewTitle)
    MenuIndex currentSystemMenuIndex = MenuIndexProfile;            // current menu index (must be in the GameViewSystemMenu)
    GameMainView currentMainView = GameViewTitle;                   // current main view of the game
    FlipWorldRun *flipWorldRun = nullptr;                           // Reference to the main run instance
    GameState gameState = GameStatePlaying;                         // current game state
    bool hasBeenPositioned = false;                                 // Track if player has been positioned to prevent repeated resets
    bool inputHeld = false;                                         // whether input is held
    bool justStarted = true;                                        // whether the player just started the game
    bool justSwitchedLevels = false;                                // whether the player just switched levels
    float levelCompletionCooldown = 0;                              // cooldown timer for level completion checks
    InputKey lastInput = InputKeyMAX;                               // Last input key pressed
    ToggleState leaveGame = ToggleOff;                              // leave game toggle state
    std::unique_ptr<Loading> loading;                               // loading animation instance
    LoginStatus loginStatus = LoginNotStarted;                      // Current login status
    uint8_t rainFrame = 0;                                          // frame counter for rain effect
    RegistrationStatus registrationStatus = RegistrationNotStarted; // Current registration status
    UserInfoStatus userInfoStatus = UserInfoNotStarted;             // Current user info status

    bool areAllEnemiesDead(Game *game);           // Check if all enemies in the current level are dead
    void checkForLevelCompletion(Game *game);     // Check if all enemies are dead and switch to next level
    void drawLoginView(Draw *canvas);             // draw the login view
    void drawRainEffect(Draw *canvas);            // draw the rain effect
    void drawRegistrationView(Draw *canvas);      // draw the registration view
    void drawSystemMenuView(Draw *canvas);        // draw the system menu view
    void drawTitleView(Draw *canvas);             // draw the title view
    void drawUserInfoView(Draw *canvas);          // draw the user info view
    void drawUsername(Vector pos, Game *game);    // draw the username at the specified position
    void drawUserStats(Vector pos, Draw *canvas); // draw the user stats at the specified position
    void updateStats();                           // update player stats
};