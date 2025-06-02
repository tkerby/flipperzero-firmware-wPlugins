#include "battle.h"
#include "pokemon.h"
#include <stdlib.h>
#include <furi.h>
#include <string.h>

// Private battle structure
struct Battle {
    Pokemon* player_pokemon;
    Pokemon* enemy_pokemon;
    uint8_t menu_cursor; // 0-3 for move selection
    bool player_turn;
    bool waiting_for_input; // tracks if we're waiting for button press
    char message[64];
    uint32_t message_timer;
    bool battle_over;
};

// Change battle_create to accept Pokemon indices
Battle* battle_create_with_selection(int player_index, int enemy_index) {
    Battle* battle = malloc(sizeof(Battle));
    if(!battle) return NULL;

    // Get species list
    const PokemonSpecies* species_list = pokemon_get_species_list();

    // Create the selected Pokemon
    battle->player_pokemon = pokemon_create_from_species(&species_list[player_index], 50);
    battle->enemy_pokemon = pokemon_create_from_species(&species_list[enemy_index], 50);

    // Initialize battle state
    battle->menu_cursor = 0;
    battle->player_turn = true;
    battle->waiting_for_input = false;

    // Dynamic message with actual Pokemon names
    snprintf(battle->message, 64, "What will %s do?", battle->player_pokemon->name);
    battle->message_timer = 0;
    battle->battle_over = false;

    return battle;
}

void battle_free(Battle* battle) {
    if(battle) {
        pokemon_free(battle->player_pokemon);
        pokemon_free(battle->enemy_pokemon);
        free(battle);
    }
}

// Handle battle updates
static void battle_update(Battle* battle) {
    // Check if a message is being displayed
    if(battle->message_timer > 0) {
        // Message shows for 1.5 seconds
        if(furi_get_tick() - battle->message_timer > 1500) {
            battle->message_timer = 0;

            // After message clears, process next turn
            if(!battle->battle_over) {
                if(!battle->player_turn) {
                    // Enemy's turn
                    const char* enemy_moves[4] = {"Tackle", "Water Gun", "Tail Whip", "Bubble"};
                    int move_choice = rand() % 4;
                    uint8_t move_power = (move_choice == 1) ? 40 : 35;

                    uint16_t damage = pokemon_calculate_damage(
                        battle->enemy_pokemon, battle->player_pokemon, move_power);

                    if(damage > battle->player_pokemon->current_hp) {
                        battle->player_pokemon->current_hp = 0;
                        snprintf(
                            battle->message,
                            64,
                            "(battle->player_pokemon->name fainted! You lose!");
                        battle->battle_over = true;
                    } else {
                        battle->player_pokemon->current_hp -= damage;
                        snprintf(
                            battle->message,
                            64,
                            "(battle->enemy_pokemon->name used %s! -%d HP",
                            enemy_moves[move_choice],
                            damage);
                    }

                    battle->message_timer = furi_get_tick();
                    battle->player_turn = true;
                } else if(battle->enemy_pokemon->current_hp == 0) {
                    snprintf(
                        battle->message, 64, "(battle->enemy_pokemon->name fainted! You win!");
                    battle->battle_over = true;
                    battle->message_timer = furi_get_tick();
                } else {
                    // Back to player's turn
                    strcpy(battle->message, "What will (battle->player_pokemon->name do?");
                }
            }
        }
    }
}

void battle_draw(Battle* battle, Canvas* canvas) {
    canvas_clear(canvas);
    canvas_set_font(canvas, FontSecondary);

    // Always update battle state
    battle_update(battle);

    // Draw enemy Pokemon info (top of screen)
    canvas_draw_str(canvas, 2, 8, battle->enemy_pokemon->name);
    char level_str[16];
    snprintf(level_str, 16, "Lv%d", battle->enemy_pokemon->level);
    canvas_draw_str(canvas, 80, 8, level_str);

    // Enemy HP bar
    canvas_draw_frame(canvas, 2, 10, 52, 6);
    uint8_t enemy_hp_fill =
        (battle->enemy_pokemon->current_hp * 50) / battle->enemy_pokemon->max_hp;
    if(enemy_hp_fill > 0) {
        canvas_draw_box(canvas, 3, 11, enemy_hp_fill, 4);
    }

    // Draw player Pokemon info (bottom right)
    canvas_draw_str(canvas, 74, 40, battle->player_pokemon->name);
    snprintf(level_str, 16, "Lv%d", battle->player_pokemon->level);
    canvas_draw_str(canvas, 74, 48, level_str);

    // Player HP bar
    canvas_draw_frame(canvas, 74, 50, 52, 6);
    uint8_t player_hp_fill =
        (battle->player_pokemon->current_hp * 50) / battle->player_pokemon->max_hp;
    if(player_hp_fill > 0) {
        canvas_draw_box(canvas, 75, 51, player_hp_fill, 4);
    }

    // HP text
    char hp_text[32];
    snprintf(
        hp_text, 32, "%d/%d", battle->player_pokemon->current_hp, battle->player_pokemon->max_hp);
    canvas_draw_str(canvas, 74, 62, hp_text);

    // Draw battle menu or message
    if(battle->player_turn && battle->message_timer == 0 && !battle->battle_over) {
        // Draw move menu
        canvas_draw_frame(canvas, 0, 20, 70, 44);

        const char* moves[4] = {"Tackle", "Ember", "Growl", "Scratch"};
        for(int i = 0; i < 4; i++) {
            int x = 2 + (i % 2) * 35;
            int y = 30 + (i / 2) * 15;

            if(i == battle->menu_cursor) {
                canvas_draw_box(canvas, x - 1, y - 8, 33, 10);
                canvas_set_color(canvas, ColorWhite);
            }

            canvas_draw_str(canvas, x, y, moves[i]);
            canvas_set_color(canvas, ColorBlack);
        }
    } else {
        // Draw message
        canvas_draw_str(canvas, 2, 35, battle->message);

        // If battle is over, show prompt
        if(battle->battle_over && battle->message_timer == 0) {
            canvas_draw_str(canvas, 2, 50, "Press BACK to exit");
        }
    }
}

void battle_handle_input(Battle* battle, InputKey key) {
    // If battle is over, only handle back button
    if(battle->battle_over) {
        return;
    }

    // Only handle input during player's turn and no message showing
    if(!battle->player_turn || battle->message_timer > 0) return;

    switch(key) {
    case InputKeyUp:
        if(battle->menu_cursor >= 2) battle->menu_cursor -= 2;
        break;
    case InputKeyDown:
        if(battle->menu_cursor < 2) battle->menu_cursor += 2;
        break;
    case InputKeyLeft:
        if(battle->menu_cursor % 2 == 1) battle->menu_cursor--;
        break;
    case InputKeyRight:
        if(battle->menu_cursor % 2 == 0) battle->menu_cursor++;
        break;
    case InputKeyOk:
        // Execute player move
        // Declare arrays at case level so they're accessible throughout
        const char* move_names[4] = {"Tackle", "Ember", "Growl", "Scratch"};
        uint8_t move_powers[4] = {35, 40, 0, 40}; // Growl has 0 power

        uint8_t move_power = move_powers[battle->menu_cursor];

        if(move_power > 0) {
            uint16_t damage = pokemon_calculate_damage(
                battle->player_pokemon, battle->enemy_pokemon, move_power);

            if(damage > battle->enemy_pokemon->current_hp) {
                battle->enemy_pokemon->current_hp = 0;
            } else {
                battle->enemy_pokemon->current_hp -= damage;
            }

            snprintf(
                battle->message,
                64,
                "(battle->player_pokemon->name used %s! -%d HP",
                move_names[battle->menu_cursor],
                damage);
        } else {
            snprintf(battle->message, 64, "(battle->player_pokemon->name used Growl!");
        }

        battle->message_timer = furi_get_tick();
        battle->player_turn = false;
        break;
    default:
        break;
    }
}

bool battle_is_over(Battle* battle) {
    return battle->battle_over;
}
