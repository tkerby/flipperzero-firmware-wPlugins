#include "run/player.hpp"
#include "flip_world_icons.h"
#include "run/run.hpp"
#include "run/general.hpp"
#include "app.hpp"
#include "jsmn/jsmn.h"
#include <math.h>

Player::Player() : Entity("Player", ENTITY_PLAYER, Vector(384, 192), Vector(15, 11), player_left_sword_15x11px, player_left_sword_15x11px, player_right_sword_15x11px)
{
    is_player = true;                                        // Mark this entity as a player (so level doesn't delete it)
    end_position = Vector(384, 192);                         // Initialize end position
    start_position = Vector(384, 192);                       // Initialize start position
    strncpy(player_name, "Player", sizeof(player_name) - 1); // Copy default player name
    player_name[sizeof(player_name) - 1] = '\0';             // Ensure null termination
    name = player_name;                                      // Point Entity's name to our writable buffer
    // Initialize player stats
    level = 1;
    health = 100;
    max_health = 100;
    strength = 10;
    attack_timer = 1;
    health_regen = 1;
}

Player::~Player()
{
    // nothing to clean up for now
}

bool Player::areAllEnemiesDead(Game *game)
{
    // Ensure we have a valid game and current level
    if (!game || !game->current_level)
    {
        FURI_LOG_E("Player", "areAllEnemiesDead: Invalid game or current level");
        return false;
    }

    // Get the current level
    Level *currentLevel = game->current_level;
    int totalEnemies = 0;
    int deadEnemies = 0;

    // Check all entities in the current level
    for (int i = 0; i < currentLevel->getEntityCount(); i++)
    {
        Entity *entity = currentLevel->getEntity(i);

        // Skip null entities
        if (!entity)
        {
            continue;
        }

        // Check if this is an enemy entity
        if (entity->type == ENTITY_ENEMY)
        {
            totalEnemies++;

            if (entity->state == ENTITY_DEAD)
            {
                deadEnemies++;
            }
            else
            {
                return false;
            }
        }
    }

    // Only return true if there were enemies to begin with and they're all dead
    if (totalEnemies == 0)
    {
        FURI_LOG_E("Player", "No enemies found in current level!");
        return false; // Don't switch levels if there are no enemies
    }

    return deadEnemies == totalEnemies;
}

void Player::checkForLevelCompletion(Game *game)
{
    // Only check for level completion if we're in the game view and the game is running
    if (!flipWorldRun || !flipWorldRun->isRunning() || currentMainView != GameViewGame)
    {
        return;
    }

    // Update cooldown timer
    levelCompletionCooldown -= 1.0 / 30; // 30 fps
    if (levelCompletionCooldown > 0)
    {
        return; // Still in cooldown, don't check yet
    }

    // Don't check immediately after switching levels
    if (justSwitchedLevels)
    {
        justSwitchedLevels = false;    // Reset the flag
        levelCompletionCooldown = 1.0; // Set a 1 second cooldown
        return;
    }

    // Check if all enemies are dead
    if (areAllEnemiesDead(game))
    {
        // Get current level index
        LevelIndex currentLevelIndex = flipWorldRun->getCurrentLevelIndex();

        // Determine next level
        LevelIndex nextLevelIndex = LevelUnknown;
        switch (currentLevelIndex)
        {
        case LevelHomeWoods:
            nextLevelIndex = LevelRockWorld;
            break;
        case LevelRockWorld:
            nextLevelIndex = LevelForestWorld;
            break;
        case LevelForestWorld:
            // End of available levels - could reset to first level or show completion
            nextLevelIndex = LevelHomeWoods;
            break;
        default:
            // Unknown level, start from the beginning
            nextLevelIndex = LevelHomeWoods;
            break;
        }

        // Switch to the next level if valid
        if (nextLevelIndex != LevelUnknown && flipWorldRun->getEngine() && flipWorldRun->getEngine()->getGame())
        {
            // Set game state to switching levels
            setGameState(GameStateSwitchingLevels);

            // Switch to the next level
            flipWorldRun->getEngine()->getGame()->level_switch((int)nextLevelIndex);

            // Set the icon group for the new level
            flipWorldRun->setIconGroup(nextLevelIndex);

            // Reset player position to start position
            position = start_position;
            position_set(start_position);

            // Mark that we just switched levels
            justSwitchedLevels = true;

            // Reset player health to full for new level
            health = max_health;

            // Set cooldown to prevent immediate re-checking (2 seconds)
            levelCompletionCooldown = 2.0;

            // Return to playing state
            setGameState(GameStatePlaying);
        }
    }
}

void Player::drawCurrentView(Draw *canvas)
{
    if (!canvas)
        return;

    switch (currentMainView)
    {
    case GameViewTitle:
        drawTitleView(canvas);
        break;
    case GameViewGame:
        // In-game view, render the game engine
        if (flipWorldRun->isRunning())
        {
            if (flipWorldRun->getEngine())
            {
                if (shouldLeaveGame())
                {
                    flipWorldRun->endGame();
                    return;
                }
                flipWorldRun->getEngine()->updateGameInput(flipWorldRun->getCurrentInput());
                // Reset the input after processing to prevent it from being continuously pressed
                flipWorldRun->resetInput();
                flipWorldRun->getEngine()->runAsync(true);
            }
            return;
        }
        else
        {
            canvas->fillScreen(ColorWhite);
            canvas->setFont(FontPrimary);
            canvas->text(Vector(25, 32), "Starting Game...", ColorBlack);
            bool gameStarted = flipWorldRun->startGame();
            if (gameStarted && flipWorldRun->getEngine())
            {
                flipWorldRun->getEngine()->runAsync(true); // Run the game engine immediately
            }
        }
        break;
    case GameViewLogin:
        drawLoginView(canvas);
        break;
    case GameViewRegistration:
        drawRegistrationView(canvas);
        break;
    case GameViewUserInfo:
        drawUserInfoView(canvas);
        break;
    default:
        canvas->fillScreen(ColorWhite);
        canvas->text(Vector(0, 10), "Unknown View", ColorBlack);
        break;
    }
}

void Player::drawLoginView(Draw *canvas)
{
    canvas->fillScreen(ColorWhite);
    canvas->setFont(FontPrimary);
    static bool loadingStarted = false;
    switch (loginStatus)
    {
    case LoginWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Logging in...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            char response[256];
            FlipWorldApp *app = static_cast<FlipWorldApp *>(flipWorldRun->appContext);
            if (app && app->loadChar("login", response, sizeof(response)))
            {
                if (strstr(response, "[SUCCESS]") != NULL)
                {
                    loginStatus = LoginSuccess;
                    currentMainView = GameViewUserInfo; // switch to user info view
                    userRequest(RequestTypeUserInfo);
                    userInfoStatus = UserInfoWaiting; // set user info status to waiting
                }
                else if (strstr(response, "User not found") != NULL)
                {
                    loginStatus = LoginNotStarted;
                    currentMainView = GameViewRegistration;
                    userRequest(RequestTypeRegistration);
                    registrationStatus = RegistrationWaiting;
                }
                else
                {
                    loginStatus = LoginRequestError;
                }
            }
            else
            {
                loginStatus = LoginRequestError;
            }
        }
        break;
    case LoginSuccess:
        canvas->text(Vector(0, 10), "Login successful!", ColorBlack);
        canvas->text(Vector(0, 20), "Press OK to continue.", ColorBlack);
        break;
    case LoginCredentialsMissing:
        canvas->text(Vector(0, 10), "Missing credentials!", ColorBlack);
        canvas->text(Vector(0, 20), "Please set your username", ColorBlack);
        canvas->text(Vector(0, 30), "and password in the app.", ColorBlack);
        break;
    case LoginRequestError:
        canvas->text(Vector(0, 10), "Login request failed!", ColorBlack);
        canvas->text(Vector(0, 20), "Check your network and", ColorBlack);
        canvas->text(Vector(0, 30), "try again later.", ColorBlack);
        break;
    default:
        canvas->text(Vector(0, 10), "Logging in...", ColorBlack);
        break;
    }
}

void Player::drawRainEffect(Draw *canvas)
{
    // rain droplets/star droplets effect
    for (int i = 0; i < 8; i++)
    {
        // Use pseudo-random offsets based on frame and droplet index
        uint8_t seed = (rainFrame + i * 37) & 0xFF;
        uint8_t x = (rainFrame + seed * 13) & 0x7F;
        uint8_t y = (rainFrame * 2 + seed * 7 + i * 23) & 0x3F;

        // Draw star-like droplet
        canvas->drawPixel(Vector(x, y), ColorBlack);
        canvas->drawPixel(Vector(x - 1, y), ColorBlack);
        canvas->drawPixel(Vector(x + 1, y), ColorBlack);
        canvas->drawPixel(Vector(x, y - 1), ColorBlack);
        canvas->drawPixel(Vector(x, y + 1), ColorBlack);
    }

    rainFrame += 1;
    if (rainFrame > 128)
    {
        rainFrame = 0;
    }
}

void Player::drawRegistrationView(Draw *canvas)
{
    canvas->fillScreen(ColorWhite);
    canvas->setFont(FontPrimary);
    static bool loadingStarted = false;
    switch (registrationStatus)
    {
    case RegistrationWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Registering...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            if (loading)
            {
                loading->stop();
            }
            loadingStarted = false;
            char response[256];
            FlipWorldApp *app = static_cast<FlipWorldApp *>(flipWorldRun->appContext);
            if (app && app->loadChar("register", response, sizeof(response)))
            {
                if (strstr(response, "[SUCCESS]") != NULL)
                {
                    registrationStatus = RegistrationSuccess;
                    currentMainView = GameViewUserInfo; // switch to user info view
                    userRequest(RequestTypeUserInfo);
                    userInfoStatus = UserInfoWaiting; // set user info status to waiting
                }
                else if (strstr(response, "Username or password not provided") != NULL)
                {
                    registrationStatus = RegistrationCredentialsMissing;
                }
                else if (strstr(response, "User already exists") != NULL)
                {
                    registrationStatus = RegistrationUserExists;
                }
                else
                {
                    registrationStatus = RegistrationRequestError;
                }
            }
            else
            {
                registrationStatus = RegistrationRequestError;
            }
        }
        break;
    case RegistrationSuccess:
        canvas->text(Vector(0, 10), "Registration successful!", ColorBlack);
        canvas->text(Vector(0, 20), "Press OK to continue.", ColorBlack);
        break;
    case RegistrationCredentialsMissing:
        canvas->text(Vector(0, 10), "Missing credentials!", ColorBlack);
        canvas->text(Vector(0, 20), "Please update your username", ColorBlack);
        canvas->text(Vector(0, 30), "and password in the settings.", ColorBlack);
        break;
    case RegistrationRequestError:
        canvas->text(Vector(0, 10), "Registration request failed!", ColorBlack);
        canvas->text(Vector(0, 20), "Check your network and", ColorBlack);
        canvas->text(Vector(0, 30), "try again later.", ColorBlack);
        break;
    default:
        canvas->text(Vector(0, 10), "Registering...", ColorBlack);
        break;
    }
}

void Player::drawSystemMenuView(Draw *canvas)
{
    switch (currentSystemMenuIndex)
    {
    case MenuIndexProfile:
        // draw info
        // first option is highlighted
        char health_str[32];
        char xp_str[32];
        char level_str[32];
        char strength_str[32];

        snprintf(level_str, sizeof(level_str), "Level   : %f", (double)level);
        snprintf(health_str, sizeof(health_str), "Health  : %f", (double)health);
        snprintf(xp_str, sizeof(xp_str), "XP      : %f", (double)xp);
        snprintf(strength_str, sizeof(strength_str), "Strength: %f", (double)strength);

        canvas->setFont(FontPrimary);
        canvas->text(Vector(7, 16), name);
        canvas->setFontCustom(FONT_SIZE_SMALL);
        canvas->text(Vector(7, 30), level_str);
        canvas->text(Vector(7, 37), health_str);
        canvas->text(Vector(7, 44), xp_str);
        canvas->text(Vector(7, 51), strength_str);

        canvas->drawRect(Vector(80, 18), Vector(36, 30), ColorBlack);
        canvas->setFont(FontPrimary);
        canvas->text(Vector(86, 30), "Info");
        canvas->setFont(FontSecondary);
        canvas->text(Vector(86, 42), "More");
        break;
    case MenuIndexAbout:
        canvas->setFont(FontPrimary);
        canvas->text(Vector(7, 16), VERSION_TAG);
        canvas->setFontCustom(FONT_SIZE_SMALL);
        canvas->text(Vector(7, 25), "Developed by\nJBlanked and Derek \nJamison. Graphics\nfrom Pr3!\n\nwww.github.com/jblanked");

        // draw a box around the selected option
        canvas->drawRect(Vector(80, 18), Vector(36, 30), ColorBlack);
        canvas->setFont(FontSecondary);
        canvas->text(Vector(86, 30), "Info");
        canvas->setFont(FontPrimary);
        canvas->text(Vector(86, 42), "More");
        break;
    default:
        break;
    }
}

void Player::drawTitleView(Draw *canvas)
{
    canvas->fillScreen(ColorWhite);

    // rain effect
    drawRainEffect(canvas);

    // draw lobby text
    if (currentTitleIndex == 0)
    {
        canvas->fillRect(Vector(36, 16), Vector(56, 16), ColorBlack);
        canvas->color(ColorWhite);
        canvas->text(Vector(54, 27), "Story");
        canvas->fillRect(Vector(36, 32), Vector(56, 16), ColorWhite);
        canvas->color(ColorBlack);
        canvas->text(Vector(54, 42), "PvE");
    }
    else if (currentTitleIndex == 1)
    {
        canvas->color(ColorWhite);
        canvas->fillRect(Vector(36, 16), Vector(56, 16), ColorWhite);
        canvas->color(ColorBlack);
        canvas->text(Vector(54, 27), "Story");
        canvas->fillRect(Vector(36, 32), Vector(56, 16), ColorBlack);
        canvas->color(ColorWhite);
        canvas->text(Vector(54, 42), "PvE");
        canvas->color(ColorBlack);
    }
}

void Player::drawUsername(Vector pos, Game *game)
{
    // Calculate screen position after applying camera offset
    float screen_x = pos.x - game->pos.x;
    float screen_y = pos.y - game->pos.y;

    // Check if the username would be visible on the 128x64 screen
    float text_width = strlen(name) * 4 + 1; // Approximate text width
    if (screen_x - text_width / 2 < 0 || screen_x + text_width / 2 > 128 ||
        screen_y - 10 < 0 || screen_y > 64)
    {
        return;
    }

    // draw box around the name
    game->draw->fillRect(Vector(screen_x - (strlen(name) * 2) - 1, screen_y - 14), Vector(strlen(name) * 4 + 1, 8), ColorWhite);

    // draw name over player's head
    game->draw->text(Vector(screen_x - (strlen(name) * 2), screen_y - 7), name, ColorBlack);
}

void Player::drawUserInfoView(Draw *canvas)
{
    static bool loadingStarted = false;
    switch (userInfoStatus)
    {
    case UserInfoWaiting:
        if (!loadingStarted)
        {
            if (!loading)
            {
                loading = std::make_unique<Loading>(canvas);
            }
            loadingStarted = true;
            if (loading)
            {
                loading->setText("Fetching...");
            }
        }
        if (!this->httpRequestIsFinished())
        {
            if (loading)
            {
                loading->animate();
            }
        }
        else
        {
            canvas->text(Vector(0, 10), "Loading user info...", ColorBlack);
            canvas->text(Vector(0, 20), "Please wait...", ColorBlack);
            canvas->text(Vector(0, 30), "It may take up to 15 seconds.", ColorBlack);
            char response[512];
            FlipWorldApp *app = static_cast<FlipWorldApp *>(flipWorldRun->appContext);
            if (app && app->loadChar("user_info", response, sizeof(response)))
            {
                userInfoStatus = UserInfoSuccess;
                // they're in! let's go
                char *game_stats = get_json_value("game_stats", response);
                if (!game_stats)
                {
                    FURI_LOG_E("Player", "Failed to parse game_stats");
                    userInfoStatus = UserInfoParseError;
                    if (loading)
                    {
                        loading->stop();
                    }
                    loadingStarted = false;
                    return;
                }
                canvas->fillScreen(ColorWhite);
                canvas->text(Vector(0, 10), "User info loaded!", ColorBlack);
                char *username = get_json_value("username", game_stats);
                char *level = get_json_value("level", game_stats);
                char *xp = get_json_value("xp", game_stats);
                char *health = get_json_value("health", game_stats);
                char *strength = get_json_value("strength", game_stats);
                char *max_health = get_json_value("max_health", game_stats);
                if (!username || !level || !xp || !health || !strength || !max_health)
                {
                    FURI_LOG_E("Player", "Failed to parse user info");
                    userInfoStatus = UserInfoParseError;
                    if (username)
                        ::free(username);
                    if (level)
                        ::free(level);
                    if (xp)
                        ::free(xp);
                    if (health)
                        ::free(health);
                    if (strength)
                        ::free(strength);
                    if (max_health)
                        ::free(max_health);
                    ::free(game_stats);
                    if (loading)
                    {
                        loading->stop();
                    }
                    loadingStarted = false;
                    return;
                }

                canvas->fillScreen(ColorWhite);
                canvas->text(Vector(0, 10), "User data found!", ColorBlack);

                // Update player info
                snprintf(player_name, sizeof(player_name), "%s", username);
                name = player_name;
                this->level = atoi(level);
                this->xp = atoi(xp);
                this->health = atoi(health);
                this->strength = atoi(strength);
                this->max_health = atoi(max_health);

                canvas->fillScreen(ColorWhite);
                canvas->text(Vector(0, 10), "Player info updated!", ColorBlack);

                // clean em up gang
                ::free(username);
                ::free(level);
                ::free(xp);
                ::free(health);
                ::free(strength);
                ::free(max_health);
                ::free(game_stats);

                canvas->fillScreen(ColorWhite);
                canvas->text(Vector(0, 10), "Memory freed!", ColorBlack);

                currentMainView = GameViewGame; // switch to game view

                if (loading)
                {
                    loading->stop();
                }
                loadingStarted = false;

                canvas->fillScreen(ColorWhite);
                canvas->text(Vector(0, 10), "User info loaded successfully!", ColorBlack);
                canvas->text(Vector(0, 20), "Please wait...", ColorBlack);
                canvas->text(Vector(0, 30), "Starting game...", ColorBlack);
                canvas->text(Vector(0, 40), "It may take up to 15 seconds.", ColorBlack);

                flipWorldRun->startGame();
                return;
            }
            else
            {
                userInfoStatus = UserInfoRequestError;
            }
        }
        break;
    case UserInfoSuccess:
        canvas->fillScreen(ColorWhite);
        canvas->setFont(FontPrimary);
        canvas->text(Vector(0, 10), "User info loaded successfully!", ColorBlack);
        canvas->text(Vector(0, 20), "Press OK to continue.", ColorBlack);
        break;
    case UserInfoCredentialsMissing:
        canvas->fillScreen(ColorWhite);
        canvas->setFont(FontPrimary);
        canvas->text(Vector(0, 10), "Missing credentials!", ColorBlack);
        canvas->text(Vector(0, 20), "Please update your username", ColorBlack);
        canvas->text(Vector(0, 30), "and password in the settings.", ColorBlack);
        break;
    case UserInfoRequestError:
        canvas->fillScreen(ColorWhite);
        canvas->setFont(FontPrimary);
        canvas->text(Vector(0, 10), "User info request failed!", ColorBlack);
        canvas->text(Vector(0, 20), "Check your network and", ColorBlack);
        canvas->text(Vector(0, 30), "try again later.", ColorBlack);
        break;
    case UserInfoParseError:
        canvas->fillScreen(ColorWhite);
        canvas->setFont(FontPrimary);
        canvas->text(Vector(0, 10), "Failed to parse user info!", ColorBlack);
        canvas->text(Vector(0, 20), "Try again...", ColorBlack);
        break;
    default:
        canvas->fillScreen(ColorWhite);
        canvas->setFont(FontPrimary);
        canvas->text(Vector(0, 10), "Loading user info...", ColorBlack);
        break;
    }
}

void Player::drawUserStats(Vector pos, Draw *canvas)
{
    // first draw a white rectangle to make the text more readable
    canvas->fillRect(Vector(pos.x - 1, pos.y - 7), Vector(34, 21), ColorWhite);

    char health_str[32];
    char xp_str[32];
    char level_str[32];

    snprintf(health_str, sizeof(health_str), "HP : %.0f", (double)health);
    snprintf(level_str, sizeof(level_str), "LVL: %.0f", (double)level);

    if (xp < 10000)
        snprintf(xp_str, sizeof(xp_str), "XP : %.0f", (double)xp);
    else
        snprintf(xp_str, sizeof(xp_str), "XP : %.0fK", (double)xp / 1000);

    // draw items
    canvas->setFontCustom(FONT_SIZE_SMALL);
    canvas->text(Vector(pos.x, pos.y), health_str, ColorBlack);
    canvas->text(Vector(pos.x, pos.y + 7), xp_str, ColorBlack);
    canvas->text(Vector(pos.x, pos.y + 14), level_str, ColorBlack);
}

bool Player::httpRequestIsFinished()
{
    if (!flipWorldRun)
    {
        return true; // Default to finished if no game context
    }

    // Get app context to check HTTP state
    FlipWorldApp *app = static_cast<FlipWorldApp *>(flipWorldRun->appContext);
    if (!app)
    {
        return true; // Default to finished if no app context
    }

    // Check if HTTP request is finished (state is IDLE)
    return app->getHttpState() == IDLE;
}

HTTPState Player::getHttpState()
{
    if (!flipWorldRun)
    {
        return INACTIVE;
    }

    // Get app context to check HTTP state
    FlipWorldApp *app = static_cast<FlipWorldApp *>(flipWorldRun->appContext);
    if (!app)
    {
        return INACTIVE;
    }

    return app->getHttpState();
}

IconGroupContext *Player::getIconGroupContext() const
{
    if (!flipWorldRun)
    {
        return nullptr;
    }
    return flipWorldRun->currentIconGroup;
}

void Player::iconGroupRender(Game *game)
{
    if (!flipWorldRun || !game || !game->draw)
    {
        return; // Ensure we have a valid game and draw context
    }
    auto iconGroupContext = getIconGroupContext();
    for (int i = 0; i < iconGroupContext->count; i++)
    {
        auto spec = &iconGroupContext->icons[i];
        int x_pos = spec->pos.x - game->pos.x - (spec->size.x / 2);
        int y_pos = spec->pos.y - game->pos.y - (spec->size.y / 2);
        if (x_pos + spec->size.x < 0 || x_pos > 128 ||
            y_pos + spec->size.y < 0 || y_pos > 64)
        {
            continue;
        }
        game->draw->image(Vector(x_pos, y_pos), spec->icon, spec->size);
    }
}

void Player::processInput()
{
    if (!flipWorldRun)
    {
        return;
    }

    InputKey currentInput = lastInput;

    if (currentInput == InputKeyMAX)
    {
        return;
    }

    switch (currentMainView)
    {
    case GameViewTitle:
        // Handle title view navigation
        switch (currentInput)
        {
        case InputKeyUp:
            currentTitleIndex = TitleIndexStory;
            flipWorldRun->shouldDebounce = true;
            break;
        case InputKeyDown:
            currentTitleIndex = TitleIndexPvE;
            flipWorldRun->shouldDebounce = true;
            break;
        case InputKeyOk:
            flipWorldRun->shouldDebounce = true;
            currentMainView = GameViewLogin; // Switch to login view
            userRequest(RequestTypeLogin);
            loginStatus = LoginWaiting;
            break;
        case InputKeyBack:
            flipWorldRun->endGame();
            flipWorldRun->shouldDebounce = true;
            break;
        default:
            break;
        }
        break;
    case GameViewSystemMenu:
        switch (currentInput)
        {
        case InputKeyBack:
            currentMainView = GameViewGame;
            flipWorldRun->shouldDebounce = true;
            break;
        case InputKeyUp:
            if (currentSystemMenuIndex > MenuIndexProfile)
            {
                currentSystemMenuIndex = static_cast<MenuIndex>(currentSystemMenuIndex - 1);
            }
            flipWorldRun->shouldDebounce = true;
            break;
        case InputKeyDown:
            if (currentSystemMenuIndex < MenuIndexAbout)
            {
                currentSystemMenuIndex = static_cast<MenuIndex>(currentSystemMenuIndex + 1);
            }
            flipWorldRun->shouldDebounce = true;
            break;
        default:
            break;
        }
        break;

    case GameViewLogin:
        switch (currentInput)
        {
        case InputKeyBack:
            currentMainView = GameViewTitle;
            flipWorldRun->shouldDebounce = true;
            break;
        case InputKeyOk:
            if (loginStatus == LoginSuccess)
            {
                currentMainView = GameViewUserInfo;
                userRequest(RequestTypeUserInfo);
                userInfoStatus = UserInfoWaiting;
                flipWorldRun->shouldDebounce = true;
            }
            break;
        default:
            break;
        }
        break;

    case GameViewRegistration:
        switch (currentInput)
        {
        case InputKeyBack:
            currentMainView = GameViewTitle;
            flipWorldRun->shouldDebounce = true;
            break;
        case InputKeyOk:
            if (registrationStatus == RegistrationSuccess)
            {
                currentMainView = GameViewUserInfo;
                userRequest(RequestTypeUserInfo);
                userInfoStatus = UserInfoWaiting;
                flipWorldRun->shouldDebounce = true;
            }
            break;
        default:
            break;
        }
        break;

    case GameViewUserInfo:
        switch (currentInput)
        {
        case InputKeyBack:
            currentMainView = GameViewTitle;
            flipWorldRun->shouldDebounce = true;
            break;
        default:
            break;
        }
        break;
    default:
        break;
    }
}

void Player::render(Draw *canvas, Game *game)
{
    iconGroupRender(game);
    drawUsername(position, game);
    drawUserStats(Vector(0, 50), canvas);
}

bool Player::setHttpState(HTTPState state)
{
    if (!flipWorldRun)
    {
        return false;
    }

    // Get app context to set HTTP state
    FlipWorldApp *app = static_cast<FlipWorldApp *>(flipWorldRun->appContext);
    if (!app)
    {
        return false;
    }

    return app->setHttpState(state);
}

void Player::update(Game *game)
{
    // Apply health regeneration
    elapsed_health_regen += 1.0 / 30; // 30 frames per second
    if (elapsed_health_regen >= 1 && health < max_health)
    {
        health += health_regen;
        elapsed_health_regen = 0;
        if (health > max_health)
        {
            health = max_health;
        }
    }

    // Increment the elapsed_attack_timer for the player
    elapsed_attack_timer += 1.0 / 30; // 30 frames per second

    // update plyer traits
    updateStats();

    // Check if all enemies are dead and switch to next level if needed
    checkForLevelCompletion(game);

    Vector oldPos = position;
    Vector newPos = oldPos;
    bool shouldSetPosition = false;

    // Move according to input
    if (game->input == InputKeyUp)
    {
        newPos.y -= 5;
        direction = ENTITY_UP;
        lastInput = InputKeyUp;
        shouldSetPosition = true;
    }
    else if (game->input == InputKeyDown)
    {
        newPos.y += 5;
        direction = ENTITY_DOWN;
        lastInput = InputKeyDown;
        shouldSetPosition = true;
    }
    else if (game->input == InputKeyLeft)
    {
        newPos.x -= 5;
        direction = ENTITY_LEFT;
        lastInput = InputKeyLeft;
        shouldSetPosition = true;
    }
    else if (game->input == InputKeyRight)
    {
        newPos.x += 5;
        direction = ENTITY_RIGHT;
        lastInput = InputKeyRight;
        shouldSetPosition = true;
    }
    else if (game->input == InputKeyOk)
    {
        lastInput = InputKeyOk;
    }

    // reset input
    game->input = InputKeyMAX;

    // check if new position is within the level boundaries
    if (newPos.x < 0 || newPos.x + size.x > game->size.x ||
        newPos.y < 0 || newPos.y + size.y > game->size.y)
    {
        // restore old position
        shouldSetPosition = false;
    }

    // Store the current camera position before updating
    game->old_pos = game->pos;

    // Update camera position to center the player
    // The viewport is 128x64, so we want the player to be at the center of this viewport
    float viewport_width = 128.0f;
    float viewport_height = 64.0f;

    float camera_x = position.x - (viewport_width / 2);
    float camera_y = position.y - (viewport_height / 2);

    // Clamp camera position to ensure we don't show areas outside the world
    // Camera position represents the top-left corner of what we see
    camera_x = constrain(camera_x, 0, game->size.x - viewport_width);
    camera_y = constrain(camera_y, 0, game->size.y - viewport_height);

    // Set the new camera position
    game->pos = Vector(camera_x, camera_y);

    // update player sprite based on direction
    if (direction == ENTITY_LEFT)
    {
        sprite = sprite_left;
    }
    else if (direction == ENTITY_RIGHT)
    {
        sprite = sprite_right;
    }

    // Only check for collisions if we're actually trying to move
    if (shouldSetPosition)
    {
        bool hasCollision = false;

        // Loop over all icon specifications in the current icon group.
        for (int i = 0; i < getIconGroupContext()->count; i++)
        {
            IconSpec *spec = &getIconGroupContext()->icons[i];

            // Calculate the difference between the NEW position and the icon's center.
            float dx = newPos.x - spec->pos.x;
            float dy = newPos.y - spec->pos.y;

            // approximate collision radius:
            float radius = (spec->size.x + spec->size.y) / 4.0f;

            // Collision: if player's distance to the icon center is less than the collision radius.
            if ((dx * dx + dy * dy) < (radius * radius))
            {
                hasCollision = true;
                break;
            }
        }

        // Only update position if there's no collision
        if (!hasCollision)
        {
            position_set(newPos);
        }
        // If there's a collision, we simply don't move (stay at current position)
    }
}

void Player::updateStats()
{
    // Determine the player's level based on XP
    level = 1;
    uint32_t xp_required = 100; // Base XP for level 2

    while (level < 100 && xp >= xp_required) // Maximum level supported
    {
        level++;
        xp_required = (uint32_t)(xp_required * 1.5); // 1.5 growth factor per level
    }

    // Update strength and max health based on the new level
    strength = 10 + (level * 1);           // 1 strength per level
    max_health = 100 + ((level - 1) * 10); // 10 health per level
}

void Player::userRequest(RequestType requestType)
{
    if (!flipWorldRun)
    {
        FURI_LOG_E("Player", "userRequest: FlipWorldRun instance is null");
        return;
    }

    // Get app context to access HTTP functionality
    FlipWorldApp *app = static_cast<FlipWorldApp *>(flipWorldRun->appContext);
    if (!app)
    {
        FURI_LOG_E("Player", "userRequest: App context is null");
        return;
    }

    // Allocate memory for credentials
    char *username = (char *)malloc(64);
    char *password = (char *)malloc(64);
    if (!username || !password)
    {
        FURI_LOG_E("Player", "userRequest: Failed to allocate memory for credentials");
        if (username)
            free(username);
        if (password)
            free(password);
        return;
    }

    // Load credentials from storage
    bool credentialsLoaded = true;
    if (!app->loadChar("user_name", username, 64))
    {
        FURI_LOG_E("Player", "Failed to load user_name");
        credentialsLoaded = false;
    }
    if (!app->loadChar("user_pass", password, 64))
    {
        FURI_LOG_E("Player", "Failed to load user_pass");
        credentialsLoaded = false;
    }

    if (!credentialsLoaded)
    {
        switch (requestType)
        {
        case RequestTypeLogin:
            loginStatus = LoginCredentialsMissing;
            break;
        case RequestTypeRegistration:
            registrationStatus = RegistrationCredentialsMissing;
            break;
        case RequestTypeUserInfo:
            userInfoStatus = UserInfoCredentialsMissing;
            break;
        }
        free(username);
        free(password);
        return;
    }

    // Create JSON payload for login/registration
    char *payload = (char *)malloc(256);
    if (!payload)
    {
        FURI_LOG_E("Player", "userRequest: Failed to allocate memory for payload");
        free(username);
        free(password);
        return;
    }
    snprintf(payload, 256, "{\"username\":\"%s\",\"password\":\"%s\"}", username, password);

    switch (requestType)
    {
    case RequestTypeLogin:
        if (!app->httpRequestAsync("login.txt",
                                   "https://www.jblanked.com/flipper/api/user/login/",
                                   POST, "{\"Content-Type\":\"application/json\"}", payload))
        {
            loginStatus = LoginRequestError;
        }
        break;
    case RequestTypeRegistration:
        if (!app->httpRequestAsync("register.txt",
                                   "https://www.jblanked.com/flipper/api/user/register/",
                                   POST, "{\"Content-Type\":\"application/json\"}", payload))
        {
            registrationStatus = RegistrationRequestError;
        }
        break;
    case RequestTypeUserInfo:
    {
        char *url = (char *)malloc(128);
        if (!url)
        {
            FURI_LOG_E("Player", "userRequest: Failed to allocate memory for url");
            userInfoStatus = UserInfoRequestError;
            free(username);
            free(password);
            free(payload);
            return;
        }
        snprintf(url, 128, "https://www.jblanked.com/flipper/api/user/game-stats/%s/", username);
        if (!app->httpRequestAsync("user_info.txt", url, GET, "{\"Content-Type\":\"application/json\"}"))
        {
            userInfoStatus = UserInfoRequestError;
        }
        free(url);
    }
    break;
    default:
        FURI_LOG_E("Player", "Unknown request type: %d", requestType);
        loginStatus = LoginRequestError;
        registrationStatus = RegistrationRequestError;
        userInfoStatus = UserInfoRequestError;
        free(username);
        free(password);
        free(payload);
        return;
    }

    free(username);
    free(password);
    free(payload);
}