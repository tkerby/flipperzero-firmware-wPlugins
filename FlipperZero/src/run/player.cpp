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
            bool gameStarted = flipWorldRun->startGame(currentTitleIndex);
            if (gameStarted && flipWorldRun->getEngine())
            {
                flipWorldRun->getEngine()->runAsync(true); // Run the game engine immediately
            }
        }
        break;
    default:
        canvas->fillScreen(ColorWhite);
        canvas->text(Vector(0, 10), "Unknown View", ColorBlack);
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
    float text_width = strlen(name) * 5 + 4; // Approximate text width
    if (screen_x - text_width / 2 < 0 || screen_x + text_width / 2 > 128 ||
        screen_y - 10 < 0 || screen_y > 64)
    {
        return;
    }

    // draw box around the name
    game->draw->fillRect(Vector(screen_x - (strlen(name) * 2), screen_y - 10), Vector(strlen(name) * 5 + 4, 10), ColorWhite);

    // draw name over player's head
    game->draw->text(Vector(screen_x - (strlen(name) * 2), screen_y - 10), name, ColorBlack);
}

void Player::drawUserStats(Vector pos, Draw *canvas)
{
    // first draw a white rectangle to make the text more readable
    canvas->fillRect(Vector(pos.x - 2, pos.y - 5), Vector(48, 32), ColorWhite);

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
    canvas->text(Vector(pos.x, pos.y), health_str, ColorBlack);
    canvas->text(Vector(pos.x, pos.y + 9), xp_str, ColorBlack);
    canvas->text(Vector(pos.x, pos.y + 18), level_str, ColorBlack);
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

void Player::processInput()
{
    if (!flipWorldRun)
    {
        return;
    }

    InputKey currentInput = lastInput;

    if (currentInput == InputKeyMAX)
    {
        return; // No input to process
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
            flipWorldRun->startGame(currentTitleIndex);
            currentMainView = GameViewGame; // Switch to game view after starting the game
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

    // case GameViewLogin:
    //     switch (currentInput)
    //     {
    //     case InputKeyBack:
    //         currentMainView = GameViewWelcome;
    //         flipWorldRun->shouldDebounce = true;
    //         break;
    //     case InputKeyOk:
    //         if (loginStatus == LoginSuccess)
    //         {
    //             currentMainView = GameViewTitle;
    //             flipWorldRun->shouldDebounce = true;
    //         }
    //         break;
    //     default:
    //         break;
    //     }
    //     break;

    // case GameViewRegistration:
    //     switch (currentInput)
    //     {
    //     case InputKeyBack:
    //         currentMainView = GameViewWelcome;
    //         flipWorldRun->shouldDebounce = true;
    //         break;
    //     case InputKeyOk:
    //         if (registrationStatus == RegistrationSuccess)
    //         {
    //             currentMainView = GameViewTitle;
    //             flipWorldRun->shouldDebounce = true;
    //         }
    //         break;
    //     default:
    //         break;
    //     }
    //     break;

    // case GameViewUserInfo:
    //     switch (currentInput)
    //     {
    //     case InputKeyBack:
    //         currentMainView = GameViewTitle;
    //         flipWorldRun->shouldDebounce = true;
    //         break;
    //     default:
    //         break;
    //     }
    //     break;
    default:
        break;
    }
}

void Player::render(Draw *canvas, Game *game)
{
    drawUsername(position, game);
    drawUserStats(Vector(5, 210), canvas);
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

    Vector oldPos = position;
    Vector newPos = oldPos;

    // Move according to input
    if (game->input == InputKeyUp)
    {
        newPos.y -= 5;
        direction = ENTITY_UP;
        lastInput = InputKeyUp;
    }
    else if (game->input == InputKeyDown)
    {
        newPos.y += 5;
        direction = ENTITY_DOWN;
        lastInput = InputKeyDown;
    }
    else if (game->input == InputKeyLeft)
    {
        newPos.x -= 5;
        direction = ENTITY_LEFT;
        lastInput = InputKeyLeft;
    }
    else if (game->input == InputKeyRight)
    {
        newPos.x += 5;
        direction = ENTITY_RIGHT;
        lastInput = InputKeyRight;
    }
    else if (game->input == InputKeyOk)
    {
        lastInput = InputKeyOk;
    }

    // reset input
    game->input = InputKeyMAX;

    // Tentatively set new position
    position_set(newPos);

    // check if new position is within the level boundaries
    if (newPos.x < 0 || newPos.x + size.x > game->size.x ||
        newPos.y < 0 || newPos.y + size.y > game->size.y)
    {
        // restore old position
        position_set(oldPos);
    }

    // Store the current camera position before updating
    game->old_pos = game->pos;

    // Update camera position to center the player
    // The viewport is 128x64, so we want the player at the center of this viewport
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