#include "game/player.hpp"
#include "free_roam_icons.h"
#include <math.h>

Player::Player() : Entity("Player", ENTITY_PLAYER, Vector(6, 6), Vector(10, 10), NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, SPRITE_3D_HUMANOID)
{
    direction = Vector(1, 0);      // facing east initially (better for 3rd person view)
    plane = Vector(0, 0.66);       // camera plane perpendicular to direction
    is_player = true;              // Mark this entity as a player (so level doesn't delete it)
    end_position = Vector(6, 6);   // Initialize end position
    start_position = Vector(6, 6); // Initialize start position
    strncpy(player_name, "Player", sizeof(player_name) - 1);
    player_name[sizeof(player_name) - 1] = '\0'; // Ensure null termination
    name = player_name;                          // Point Entity's name to our writable buffer
}

bool Player::collisionMapCheck(Vector new_position)
{
    if (currentDynamicMap == nullptr)
        return false;

    uint8_t x = (uint8_t)new_position.x;
    uint8_t y = (uint8_t)new_position.y;

    // Bounds checking
    if (x >= currentDynamicMap->getWidth() || y >= currentDynamicMap->getHeight())
    {
        // Out of bounds, treat as collision
        return true;
    }

    TileType tile = currentDynamicMap->getTile(x, y);
    return (tile == TILE_WALL); // Only walls block movement
}

void Player::debounceInput(Game *game)
{
    static uint8_t debounceCounter = 0;
    if (shouldDebounce)
    {
        game->input = InputKeyMAX;
        debounceCounter++;
        if (debounceCounter < 4)
        {
            return;
        }
        debounceCounter = 0;
        shouldDebounce = false;
        inputHeld = false;
    }
}

void Player::handleMenu(Draw *draw, Game *game)
{
    if (!draw || !game)
    {
        return;
    }

    // too lazy for now, but we should use the Draw methods instead of canvas directly
    // I'll fix this before porting to Picoware
    auto canvas = draw->display;
    if (!canvas)
    {
        return;
    }

    if (currentMenuIndex != MenuIndexSettings)
    {
        switch (game->input)
        {
        case InputKeyUp:
            if (currentMenuIndex > MenuIndexProfile)
            {
                currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
            }
            shouldDebounce = true;
            break;
        case InputKeyDown:
            if (currentMenuIndex < MenuIndexAbout)
            {
                currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex + 1);
            }
            shouldDebounce = true;
            break;
        default:
            break;
        };
    }
    else
    {
        switch (currentSettingsIndex)
        {
        case MenuSettingsMain:
            // back to title, up to profile, down to settings, left to sound
            switch (game->input)
            {
            case InputKeyUp:
                if (currentMenuIndex > MenuIndexProfile)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex - 1);
                }
                shouldDebounce = true;
                break;
            case InputKeyDown:
                if (currentMenuIndex < MenuIndexAbout)
                {
                    currentMenuIndex = static_cast<MenuIndex>(currentMenuIndex + 1);
                }
                shouldDebounce = true;
                break;
            case InputKeyLeft:
                currentSettingsIndex = MenuSettingsSound; // Switch to sound settings
                shouldDebounce = true;
                break;
            default:
                break;
            };
            break;
        case MenuSettingsSound:
            // sound on/off (using OK button), down to vibration, right to MainSettingsMain
            switch (game->input)
            {
            case InputKeyOk:
            {
                // Toggle sound on/off
                soundToggle = soundToggle == ToggleOn ? ToggleOff : ToggleOn;
                shouldDebounce = true;
                // let's just make the game check if state has changed and save it
            }
            break;
            case InputKeyRight:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                shouldDebounce = true;
                break;
            case InputKeyDown:
                currentSettingsIndex = MenuSettingsVibration; // Switch to vibration settings
                shouldDebounce = true;
                break;
            default:
                break;
            };
            break;
        case MenuSettingsVibration:
            // vibration on/off (using OK button), up to sound, right to MainSettingsMain, down to leave game
            switch (game->input)
            {
            case InputKeyOk:
            {
                // Toggle vibration on/off
                vibrationToggle = vibrationToggle == ToggleOn ? ToggleOff : ToggleOn;
                shouldDebounce = true;
                // let's just make the game check if state has changed and save it
            }
            break;
            case InputKeyRight:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                shouldDebounce = true;
                break;
            case InputKeyUp:
                currentSettingsIndex = MenuSettingsSound; // Switch to sound settings
                shouldDebounce = true;
                break;
            case InputKeyDown:
                currentSettingsIndex = MenuSettingsLeave; // Switch to leave game settings
                shouldDebounce = true;
                break;
            default:
                break;
            };
            break;
        case MenuSettingsLeave:
            // leave game (using OK button), up to vibration, right to MainSettingsMain
            switch (game->input)
            {
            case InputKeyOk:
                // Leave game
                leaveGame = ToggleOn;
                shouldDebounce = true;
                break;
            case InputKeyRight:
                currentSettingsIndex = MenuSettingsMain; // Switch back to main settings
                shouldDebounce = true;
                break;
            case InputKeyUp:
                currentSettingsIndex = MenuSettingsVibration; // Switch to vibration settings
                shouldDebounce = true;
                break;
            default:
                break;
            };
            break;
        default:
            break;
        };
    }

    if (game->input == InputKeyOk)
    {
        switch (currentSettingsIndex)
        {
        case MenuSettingsSound:
            // Toggle sound on/off
            soundToggle = soundToggle == ToggleOn ? ToggleOff : ToggleOn;
            shouldDebounce = true;
            // let's just make the game check if state has changed and save it
            break;
        case MenuSettingsVibration:
            // Toggle vibration on/off
            vibrationToggle = vibrationToggle == ToggleOn ? ToggleOff : ToggleOn;
            shouldDebounce = true;
            // let's just make the game check if state has changed and save it
            break;
        case MenuSettingsLeave:
            leaveGame = ToggleOn;
            shouldDebounce = true;
            break;
        default:
            break;
        }
    }

    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_icon(canvas, 0, 0, &I_icon_menu_128x64px);

    switch (currentMenuIndex)
    {
    case MenuIndexProfile:
    {
        // draw info
        char health[32];
        char xp[32];
        char level[32];
        char strength[32];

        snprintf(level, sizeof(level), "Level   : %d", (int)this->level);
        snprintf(health, sizeof(health), "Health  : %d", (int)this->health);
        snprintf(xp, sizeof(xp), "XP      : %d", (int)this->xp);
        snprintf(strength, sizeof(strength), "Strength: %d", (int)this->strength);
        canvas_set_font(canvas, FontPrimary);
        if (this->name == nullptr || strlen(this->name) == 0)
        {
            canvas_draw_str(canvas, 6, 16, "Unknown");
        }
        else
        {
            canvas_draw_str(canvas, 6, 16, this->name);
        }
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        canvas_draw_str(canvas, 6, 30, level);
        canvas_draw_str(canvas, 6, 37, health);
        canvas_draw_str(canvas, 6, 44, xp);
        canvas_draw_str(canvas, 6, 51, strength);

        // draw a box around the selected option
        canvas_draw_frame(canvas, 76, 6, 46, 46);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 80, 18, "Profile");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 80, 32, "Settings");
        canvas_draw_str(canvas, 80, 46, "About");
    }
    break;
    case MenuIndexSettings: // sound on/off, vibration on/off, and leave game
    {
        char soundStatus[16];
        char vibrationStatus[16];
        snprintf(soundStatus, sizeof(soundStatus), "Sound: %s", toggleToString(soundToggle));
        snprintf(vibrationStatus, sizeof(vibrationStatus), "Vibrate: %s", toggleToString(vibrationToggle));
        // draw settings info
        switch (currentSettingsIndex)
        {
        case MenuSettingsMain:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 6, 16, "Settings");
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 6, 30, soundStatus);
            canvas_draw_str(canvas, 6, 40, vibrationStatus);
            canvas_draw_str(canvas, 6, 50, "Leave Game");
            break;
        case MenuSettingsSound:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 6, 16, "Settings");
            canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
            canvas_draw_str(canvas, 6, 30, soundStatus);
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 6, 40, vibrationStatus);
            canvas_draw_str(canvas, 6, 50, "Leave Game");
            break;
        case MenuSettingsVibration:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 6, 16, "Settings");
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 6, 30, soundStatus);
            canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
            canvas_draw_str(canvas, 6, 40, vibrationStatus);
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 6, 50, "Leave Game");
            break;
        case MenuSettingsLeave:
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, 6, 16, "Settings");
            canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
            canvas_draw_str(canvas, 6, 30, soundStatus);
            canvas_draw_str(canvas, 6, 40, vibrationStatus);
            canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
            canvas_draw_str(canvas, 6, 50, "Leave Game");
            break;
        default:
            break;
        };
        // draw a box around the selected option
        canvas_draw_frame(canvas, 76, 6, 46, 46);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 80, 18, "Profile");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 79, 32, "Settings");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 80, 46, "About");
    }
    break;
    case MenuIndexAbout:
    {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 6, 16, "Free Roam");
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        canvas_draw_str_multi(canvas, 6, 25, "Creator: JBlanked");
        canvas_draw_str(canvas, 6, 59, "www.github.com/jblanked");

        // draw a box around the selected option
        canvas_draw_frame(canvas, 76, 6, 46, 46);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 80, 18, "Profile");
        canvas_draw_str(canvas, 80, 32, "Settings");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 80, 46, "About");
    }
    break;
    default:
        canvas_clear(canvas);
        canvas_draw_str(canvas, 0, 10, "Unknown Menu");
        break;
    };
}

void Player::render(Draw *canvas, Game *game)
{
    if (!canvas || !game || !game->current_level)
    {
        return;
    }

    static uint8_t _state = GameStatePlaying;

    this->switchLevels(game);

    if (gameState == GameStatePlaying)
    {
        if (_state != GameStatePlaying)
        {
            // make entities active again
            for (int i = 0; i < game->current_level->getEntityCount(); i++)
            {
                Entity *entity = game->current_level->getEntity(i);
                if (entity && !entity->is_active && !entity->is_player)
                {
                    entity->is_active = true; // activate all entities
                }
            }
            this->is_visible = true; // show player entity in game
            _state = GameStatePlaying;
        }
        if (currentDynamicMap != nullptr)
        {
            float camera_height = 1.6f;

            // Check if the game is using 3rd person perspective
            if (game->getPerspective() == CAMERA_THIRD_PERSON)
            {
                // Calculate 3rd person camera position for map rendering
                // Normalize direction vector to ensure consistent behavior
                float dir_length = sqrtf(direction.x * direction.x + direction.y * direction.y);
                Vector normalized_dir = Vector(direction.x / dir_length, direction.y / dir_length);

                Vector camera_pos = Vector(
                    position.x - 1.5f, // Fixed offset instead of direction-based
                    position.y - 1.5f);

                // DEBUG: Ensure 3D sprite position is updated using Entity methods
                if (has3DSprite())
                {
                    // Use Entity's methods instead of direct Sprite3D access
                    update3DSpritePosition();

                    // Force sprite to face the camera for testing
                    float camera_to_player_angle = atan2f(
                        position.y - camera_pos.y,
                        position.x - camera_pos.x);
                    set3DSpriteRotation(camera_to_player_angle);
                }
                // Note: If sprite is lost, we can't recreate it from here (private method)

                // Render map from 3rd person camera position
                currentDynamicMap->render(camera_height, canvas, camera_pos, normalized_dir, plane);
            }
            else
            {
                // Default 1st person rendering
                currentDynamicMap->render(camera_height, canvas, position, direction, plane);
            }
        }
    }
    else if (gameState == GameStateMenu)
    {
        if (_state != GameStateMenu)
        {
            // make entities inactive
            for (int i = 0; i < game->current_level->getEntityCount(); i++)
            {
                Entity *entity = game->current_level->getEntity(i);
                if (entity && entity->is_active && !entity->is_player)
                {
                    entity->is_active = false; // deactivate all entities
                }
            }
            this->is_visible = false; // hide player entity in menu
            _state = GameStateMenu;
        }
        handleMenu(canvas, game);
    }
}

void Player::switchLevels(Game *game)
{
    if (currentDynamicMap == nullptr || strcmp(currentDynamicMap->getName(), game->current_level->name) != 0)
    {
        currentDynamicMap.reset(); // reset so we can delete the old map if it exists

        if (strcmp(game->current_level->name, "Tutorial") == 0)
        {
            currentDynamicMap = mapsTutorial();
        }
        else if (strcmp(game->current_level->name, "First") == 0)
        {
            currentDynamicMap = mapsFirst();
        }
        else if (strcmp(game->current_level->name, "Second") == 0)
        {
            currentDynamicMap = mapsSecond();
        }

        if (currentDynamicMap != nullptr)
        {
            // Only set position if we haven't been positioned yet (first load)
            // OR if we're switching to a different level
            if (!hasBeenPositioned)
            {
                Vector spawn_pos = this->start_position; // Use start position from the player entity
                position = spawn_pos;
                hasBeenPositioned = true;

                // update 3D sprite position immediately after setting player position
                if (has3DSprite())
                {
                    update3DSpritePosition();

                    // Also ensure the sprite rotation and scale are set correctly
                    set3DSpriteRotation(0.0f); // Face forward initially
                    set3DSpriteScale(1.0f);    // Normal scale
                }
            }
        }
    }
}

void Player::update(Game *game)
{
    debounceInput(game);

    if (game->input == InputKeyBack)
    {
        gameState = gameState == GameStateMenu ? GameStatePlaying : GameStateMenu;
        shouldDebounce = true;
    }

    if (gameState == GameStateMenu)
    {
        return; // Don't update player position in menu
    }

    const float rotSpeed = 0.2f; // Rotation speed in radians

    switch (game->input)
    {
    case InputKeyUp:
    {
        // Calculate new position
        Vector new_pos = Vector(
            position.x + direction.x * rotSpeed,
            position.y + direction.y * rotSpeed);

        // Check collision with dynamic map
        if (currentDynamicMap == nullptr || !collisionMapCheck(new_pos))
        {
            // Move forward in the direction the player is facing
            this->position_set(new_pos);

            // Update 3D sprite position and ensure rotation is correct
            if (has3DSprite())
            {
                update3DSpritePosition();
                // Ensure sprite rotation matches current direction for proper visibility
                float rotation_angle = atan2f(direction.y, direction.x);
                set3DSpriteRotation(rotation_angle);
            }
        }
        game->input = InputKeyMAX;
    }
    break;
    case InputKeyDown:
    {
        // Calculate new position
        Vector new_pos = Vector(
            position.x - direction.x * rotSpeed,
            position.y - direction.y * rotSpeed);

        // Check collision with dynamic map
        if (currentDynamicMap == nullptr || !collisionMapCheck(new_pos))
        {
            // Move backward (opposite to the direction)
            this->position_set(new_pos);

            // Update 3D sprite position and ensure rotation is correct
            if (has3DSprite())
            {
                update3DSpritePosition();
                // Ensure sprite rotation matches current direction for proper visibility
                float rotation_angle = atan2f(direction.y, direction.x);
                set3DSpriteRotation(rotation_angle);
            }
        }
        game->input = InputKeyMAX;
    }
    break;
    case InputKeyLeft:
    {
        float old_dir_x = direction.x;
        float old_plane_x = plane.x;

        direction.x = direction.x * cos(-rotSpeed) - direction.y * sin(-rotSpeed);
        direction.y = old_dir_x * sin(-rotSpeed) + direction.y * cos(-rotSpeed);
        plane.x = plane.x * cos(-rotSpeed) - plane.y * sin(-rotSpeed);
        plane.y = old_plane_x * sin(-rotSpeed) + plane.y * cos(-rotSpeed);

        // Update sprite rotation to face the new direction
        if (has3DSprite())
        {
            float rotation_angle = atan2f(direction.y, direction.x);
            set3DSpriteRotation(rotation_angle);
        }

        game->input = InputKeyMAX;
    }
    break;
    case InputKeyRight:
    {
        float old_dir_x = direction.x;
        float old_plane_x = plane.x;

        direction.x = direction.x * cos(rotSpeed) - direction.y * sin(rotSpeed);
        direction.y = old_dir_x * sin(rotSpeed) + direction.y * cos(rotSpeed);
        plane.x = plane.x * cos(rotSpeed) - plane.y * sin(rotSpeed);
        plane.y = old_plane_x * sin(rotSpeed) + plane.y * cos(rotSpeed);

        // Update sprite rotation to face the new direction
        if (has3DSprite())
        {
            float rotation_angle = atan2f(direction.y, direction.x);
            set3DSpriteRotation(rotation_angle);
        }

        game->input = InputKeyMAX;
    }
    break;
    default:
        break;
    }
}
