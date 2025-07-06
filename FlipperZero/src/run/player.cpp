#include "run/player.hpp"
#include "flip_world_icons.h"
#include "run/run.hpp"
#include "run/general.hpp"
#include "app.hpp"
#include "jsmn/jsmn.h"
#include <math.h>

Player::Player() : Entity("Player", ENTITY_PLAYER, Vector(384, 192), Vector(15, 11), NULL, player_left_axe_15x11px)
{
    is_player = true;                                        // Mark this entity as a player (so level doesn't delete it)
    end_position = Vector(384, 192);                         // Initialize end position
    start_position = Vector(384, 192);                       // Initialize start position
    strncpy(player_name, "Player", sizeof(player_name) - 1); // Copy default player name
    player_name[sizeof(player_name) - 1] = '\0';             // Ensure null termination
    name = player_name;                                      // Point Entity's name to our writable buffer
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
    default:
        canvas->fillScreen(ColorWhite);
        canvas->text(Vector(0, 10), "Unknown View", ColorBlack);
        break;
    }
}

void Player::drawTitleView(Draw *canvas)
{
    UNUSED(canvas);
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

void Player::render(Draw *canvas, Game *game)
{
    UNUSED(game);
    UNUSED(canvas);
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
    UNUSED(game);
}
