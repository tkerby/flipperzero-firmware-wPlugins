#include <dolphin/dolphin.h>

#include "bomber_loop.h"
#include "helpers.h"
#include "subghz.h"
#include "levels.h"

#define BOMB_HOT_TIME     furi_ms_to_ticks(2000)
#define BOMB_PLANTED_TIME furi_ms_to_ticks(2100)
#define BOMB_EXPLODE_TIME furi_ms_to_ticks(2500)
#define BOMB_RESET_TIME   furi_ms_to_ticks(2600)
#define MAX_X             16
#define MAX_Y             8

// Quit the app
static void bomber_app_quit(BomberAppState* state) {
    FURI_LOG_I(TAG, "Quitting");
    bomber_app_set_mode(state, BomberAppMode_Quit);
}

// Put the game in error state
static void bomber_app_error(BomberAppState* state) {
    FURI_LOG_E(TAG, "Error occurred");
    bomber_app_set_mode(state, BomberAppMode_Error);
}

static void bomber_app_wait(BomberAppState* state) {
    FURI_LOG_E(TAG, "Waiting for P1 to select level");
    state->rxMode = RxMode_LevelData;
    bomber_app_set_mode(state, BomberAppMode_Waiting);
}

// Start playing
static void bomber_app_start(BomberAppState* state) {
    FURI_LOG_I(TAG, "Start playing");

    dolphin_deed(DolphinDeedPluginGameStart);

    // Figure out player starting positions from level data
    state->fox = bomber_app_get_block(state->level, BlockType_Fox);
    state->level[ix(state->fox.x, state->fox.y)] = (uint8_t)BlockType_Empty;
    state->wolf = bomber_app_get_block(state->level, BlockType_Wolf);
    state->level[ix(state->wolf.x, state->wolf.y)] = (uint8_t)BlockType_Empty;

    bomber_app_set_mode(state, BomberAppMode_Playing);
}

static void bomber_app_setup_level(BomberAppState* state) {
    state->level = all_levels[state->selectedLevel];
    uint8_t wall_count = count_walls(state->level);
    uint8_t powerup_bomb_count = (uint8_t)round((POWERUP_EXTRABOMB_RATIO * wall_count));
    uint8_t powerup_power_count = (uint8_t)round((POWERUP_BOMBPOWER_RATIO * wall_count));
    FURI_LOG_D(
        TAG,
        "Walls: %d, Extra Bombs: %d, Bomb Power: %d",
        wall_count,
        powerup_bomb_count,
        powerup_power_count);

    uint8_t* bomb_powerups = malloc(sizeof(uint8_t) * powerup_bomb_count);
    uint8_t* power_powerups = malloc(sizeof(uint8_t) * powerup_power_count);

    get_random_powerup_locations(state->level, powerup_bomb_count, bomb_powerups);
    get_random_powerup_locations(state->level, powerup_power_count, power_powerups);

    for(uint8_t i = 0; i < powerup_bomb_count; i++) {
        state->level[bomb_powerups[i]] = BlockType_PuExtraBomb_Hidden;
    }
    for(uint8_t i = 0; i < powerup_power_count; i++) {
        state->level[power_powerups[i]] = BlockType_PuBombStrength_Hidden;
    }

    free(bomb_powerups);
    free(power_powerups);

    // Tx level data
    subghz_tx_level_data(state, state->level);

    bomber_app_start(state);
}

static void bomber_app_select_level(BomberAppState* state) {
    FURI_LOG_I(TAG, "Select Level");
    bomber_app_set_mode(state, BomberAppMode_LevelSelect);
}

// Check if a particular coordingate is occupied by a players active bomb
static bool is_occupied_by_bomb(Player* player, uint8_t x, uint8_t y) {
    for(int i = 0; i < MAX_BOMBS; i++) {
        Bomb bomb = player->bombs[i];
        if(bomb.state != BombState_None && bomb.x == x && bomb.y == y) {
            return true;
        }
    }

    return false;
}

// Handle direction keys to move the player around
// state: Pointer to the application state
// input: Represents the input event
// returns: true if the viewport should be updated, else false
static bool handle_game_direction(BomberAppState* state, InputEvent input) {
    Player* player = get_player(state);

    Point newPoint = {player->x, player->y};

    switch(input.key) {
    case InputKeyUp:
        if(player->y == 0) return false;
        newPoint.y -= 1;
        break;
    case InputKeyDown:
        if(player->y >= 7) return false;
        newPoint.y += 1;
        break;
    case InputKeyLeft:
        if(player->x == 0) return false;
        newPoint.x -= 1;
        break;
    case InputKeyRight:
        if(player->x >= 15) return false;
        newPoint.x += 1;
        break;
    default:
        return false;
    }

    // Only allow move to new position if the block at that position is not occupied
    BlockType block = (BlockType)(state->level)[ix(newPoint.x, newPoint.y)];
    if(block == BlockType_Brick || block == BlockType_PuBombStrength_Hidden ||
       block == BlockType_PuExtraBomb_Hidden) {
        return false;
    }

    if(is_occupied_by_bomb(&state->fox, newPoint.x, newPoint.y) ||
       is_occupied_by_bomb(&state->wolf, newPoint.x, newPoint.y)) {
        return false;
    }

    if(block == BlockType_PuBombStrength) {
        player->bomb_power++;
        state->level[ix(newPoint.x, newPoint.y)] = BlockType_Empty;
    } else if(block == BlockType_PuExtraBomb) {
        player->bomb_count++;
        if(player->bomb_count == MAX_BOMBS) {
            player->bomb_count = MAX_BOMBS;
        }
        state->level[ix(newPoint.x, newPoint.y)] = BlockType_Empty;
    }

    player->x = newPoint.x;
    player->y = newPoint.y;

    tx_new_position(state, player);

    return true;
}

// Handles input while on player select screen - just switch between Fox/Wolf
// state: Pointer to the application state
// input: Represents the input event
// returns: true if the viewport should be updated, else false
static bool handle_player_select_input(BomberAppState* state, InputEvent input) {
    if(input.type == InputTypeShort) {
        switch(input.key) {
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyLeft:
        case InputKeyRight:
            state->isPlayerTwo = !state->isPlayerTwo;
            return true;
        case InputKeyOk:
            if(!state->isPlayerTwo) {
                bomber_app_select_level(state);
            } else {
                bomber_app_wait(state);
            }
            return true;
        default:
            return false;
        }
    }
    return false;
}

static bool handle_levelselect_input(BomberAppState* state, InputEvent input) {
    if(input.type == InputTypeShort) {
        switch(input.key) {
        case InputKeyOk:
            bomber_app_setup_level(state);
            return true;
        case InputKeyUp:
            state->selectedLevel -= 2;
            break;
        case InputKeyDown:
            state->selectedLevel += 2;
            break;
        case InputKeyLeft:
            state->selectedLevel -= 1;
            break;
        case InputKeyRight:
            state->selectedLevel += 1;
            break;
        default:
            return false;
        }
    }

    uint8_t levelCount = sizeof(all_levels) / sizeof(int);
    if(state->selectedLevel >= levelCount) {
        state->selectedLevel = levelCount - 1;
    }

    return true;
}

// Handle input while playing the game
// state: Pointer to the application state
// input: Represents the input event
// returns: true if the viewport should be updated, else false
static bool handle_game_input(BomberAppState* state, InputEvent input) {
    Player* player = get_player(state);

    if(input.type == InputTypeShort) {
        switch(input.key) {
        case InputKeyOk:
            FURI_LOG_I(TAG, "Drop Bomb");

            for(int i = 0; i < player->bomb_count; i++) {
                if(player->bombs[i].state == BombState_None) {
                    Bomb bomb;
                    bomb.x = player->x;
                    bomb.y = player->y;
                    bomb.state = BombState_Planted;
                    bomb.planted = furi_get_tick();

                    player->bombs[i] = bomb;

                    tx_bomb_placement(state, bomb.x, bomb.y);
                    break;
                }
            }

            return true;
        case InputKeyUp:
        case InputKeyDown:
        case InputKeyLeft:
        case InputKeyRight:
            return handle_game_direction(state, input);
        default:
            break;
        }
    }

    return false;
}

// Main entry point for handling input
// state: Pointer to the application state
// input: Represents the input event
// returns: true if the viewport should be updated, else false
static bool bomber_app_handle_input(BomberAppState* state, InputEvent input) {
    if(input.key == InputKeyBack) {
        bomber_app_quit(state);
        return false; // don't try to update the UI while quitting
    }

    switch(state->mode) {
    case BomberAppMode_Playing:
        return handle_game_input(state, input);
    case BomberAppMode_PlayerSelect:
        return handle_player_select_input(state, input);
    case BomberAppMode_LevelSelect:
        return handle_levelselect_input(state, input);
    default:
        break;
    }

    return false;
}

// Application main loop
// state: Pointer to the application state
void bomber_main_loop(BomberAppState* state) {
    FURI_LOG_I(TAG, "Begin application main loop");
    view_port_update(state->view_port);

    BomberEvent event;

    state->running = true;

    while(state->running) {
        switch(state->rxMode) {
        case RxMode_Command:
            subghz_check_incoming(state);
            break;
        case RxMode_LevelData:
            subghz_check_incoming_leveldata(state);
            break;
        }

        switch(furi_message_queue_get(state->queue, &event, LOOP_MESSAGE_TIMEOUT_ms)) {
        case FuriStatusOk:
            bool updated = false;
            switch(event.type) {
            case BomberEventType_Input:
                FURI_LOG_T(TAG, "Input Event from queue");
                updated = bomber_app_handle_input(state, event.input);
                break;
            case BomberEventType_Tick:
                FURI_LOG_T(TAG, "Tick Event from queue");
                updated = bomber_game_tick(state);
                break;
            case BomberEventType_SubGhz:
                FURI_LOG_I(TAG, "SubGhz Event from queue");
                bomber_game_post_rx(state, event.subGhzIncomingSize);
                updated = true;
                break;
            case BomberEventType_HaveLevelData:
                FURI_LOG_I(TAG, "Level data event from queue");
                state->level = state->levelData;
                state->rxMode = RxMode_Command;
                bomber_app_start(state);
                updated = true;
                break;
            default:
                FURI_LOG_E(TAG, "Unknown event received from queue.");
                break;
            }

            if(updated) {
                view_port_update(state->view_port);
            }
            break;

        // error cases
        case FuriStatusErrorTimeout:
            FURI_LOG_D(TAG, "furi_message_queue_get timed out");
            break;
        default:
            FURI_LOG_E(TAG, "furi_message_queue_get was neither ok nor timeout");
            bomber_app_error(state);
        }

        if(state->mode == BomberAppMode_Quit || state->mode == BomberAppMode_Error) {
            state->running = false;
        }
    }
}

static bool handle_explosion(BomberAppState* state, uint8_t x, uint8_t y, bool ownBombs) {
    // Out of bounds.
    // No need to check negatives as uint8_t is unsigned and will underflow, resulting in a value way over MAX_X and MAX_Y.
    if(x >= MAX_X || y >= MAX_Y) {
        return false;
    }

    Player* player = get_player(state);

    if(player->x == x && player->y == y) {
        tx_death(state);
        state->isDead = true;
        state->suicide = ownBombs;
        bomber_app_set_mode(state, BomberAppMode_GameOver);
    }

    switch(state->level[ix(x, y)]) {
    case BlockType_Brick:
        state->level[ix(x, y)] = BlockType_Empty;
        return true;
    case BlockType_PuBombStrength_Hidden:
        state->level[ix(x, y)] = BlockType_PuBombStrength;
        return true;
    case BlockType_PuExtraBomb_Hidden:
        state->level[ix(x, y)] = BlockType_PuExtraBomb;
        return true;
    default:
        return false;
    }
}

static bool update_bombs(Player* player, BomberAppState* state, bool ownBombs) {
    bool changed = false;

    for(uint8_t i = 0; i < MAX_BOMBS; i++) {
        Bomb* bomb = &player->bombs[i];
        if(bomb->state != BombState_None) {
            uint32_t time = furi_get_tick() - bomb->planted;

            if(time > BOMB_RESET_TIME) {
                bomb->planted = 0;
                bomb->state = BombState_None;
                continue;
            }

            if(time > BOMB_EXPLODE_TIME) {
                bomb->state = BombState_Explode;

                for(uint8_t j = 0; j < player->bomb_power + 1; j++) {
                    changed &= handle_explosion(state, bomb->x - j, bomb->y, ownBombs);
                    changed &= handle_explosion(state, bomb->x + j, bomb->y, ownBombs);
                    changed &= handle_explosion(state, bomb->x, bomb->y + j, ownBombs);
                    changed &= handle_explosion(state, bomb->x, bomb->y - j, ownBombs);
                }

                continue;
            }

            if(time > BOMB_PLANTED_TIME) {
                bomb->state = BombState_Planted;
            } else if(time > BOMB_HOT_TIME) {
                bomb->state = BombState_Hot;
            }
        }
    }

    return changed;
}

bool bomber_game_tick(BomberAppState* state) {
    bool changed = false;
    changed &= update_bombs(&state->fox, state, !state->isPlayerTwo);
    changed &= update_bombs(&state->wolf, state, state->isPlayerTwo);

    return changed;
}
