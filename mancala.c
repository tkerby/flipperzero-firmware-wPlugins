#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdint.h> // Standard integer types
#include <stdlib.h> // Standard library functions
#include <gui/gui.h> // GUI system
#include <gui/view_port.h>
#include <gui/canvas.h>
#include <gui/elements.h> // Button drawing functions
#include <input/input.h> // Input handling (buttons)
#include <mitzi_mancala_icons.h>

#define TAG "Mancala" // Tag for logging purposes

// =============================================================================
// GAME STATE DEFINITIONS
// =============================================================================

// Represents whose turn it is
typedef enum {
    TURN_USER = 0,
    TURN_AI = 1
} Turn;

// Main application state structure
typedef struct {
    ViewPort* view_port;    // ViewPort for rendering UI
    Gui* gui;               // GUI instance
    int pits_user[6];       // Lower row (user's pits, left to right)
    int pits_ai[6];         // Upper row (AI's pits, left to right)
    int store_user;         // Right store (user's collection)
    int store_ai;           // Left store (AI's collection)
    Turn turn;              // Current player's turn
    int cursor;             // Selected pit index for user (0..5, left->right)
    bool game_over;         // Game completion flag
    char status_text[32];   // Status message displayed at bottom
    bool redraw;            // Flag to trigger screen redraw
    bool should_exit;       // Flag to exit application
} MancalaState;

// Game constants
#define INIT_STONES 4 // Starting stones per pit

// Layout constants for rendering
// Y-positions for pit number display
static const int ai_pit_y = 26;
static const int user_pit_y = 45;
// X-positions and spacing for pits
static const int pit_start_x = 24;  // X position of first pit
static const int pit_spacing = 16;  // Distance between pits
// Number position for stores (=mancalas)
static const int store_ai_x = 3;
static const int store_ai_y = 36;
static const int store_user_x = 115;
static const int store_user_y = 36;
// Status message position
static const int status_x = 57;
static const int status_y = 2;

// =============================================================================
// FUNCTION DECLARATIONS
// =============================================================================
// Game logic functions --------------------------------------------------------
static void init_game(MancalaState* s);
static void end_game_if_needed(MancalaState* s);
static void collect_remaining_to_stores(MancalaState* s);
static void do_move_from(MancalaState* s, int pit_index, bool by_user);
static int ai_choose_move(MancalaState* s);
// Rendering functions ---------------------------------------------------------
static void render_callback(Canvas* canvas, void* ctx);
static void draw_board_background(Canvas* canvas, MancalaState* state);
static void draw_numbers(Canvas* canvas, MancalaState* state);
static void draw_cursor(Canvas* canvas, MancalaState* state);
// Input handling functions ----------------------------------------------------
static void input_handler(InputEvent* evt, void* ctx);
static void perform_user_action_ok(MancalaState* state);
static void perform_ai_turn(MancalaState* state);

// =============================================================================
// GAME LOGIC IMPLEMENTATION (Kalah rules)
// =============================================================================
static void init_game(MancalaState* s) {
    // Initialize all pits with equal number of starting stones
    for(int i = 0; i < 6; i++) {
        s->pits_user[i] = INIT_STONES;
        s->pits_ai[i] = INIT_STONES;
    }
    
    // Empty stores
    s->store_user = 0;
    s->store_ai = 0;
    
    // Game state: User goes first
    s->turn = TURN_USER;
    s->cursor = 2; // Default cursor position (middle pit)
    s->game_over = false;
    
    // Initial status message
    snprintf(s->status_text, sizeof(s->status_text), "You: 0 | AI: 0");
    s->redraw = true;
}

/**
 * Collect all remaining stones from both sides into respective stores
 * Called when game ends
 */
static void collect_remaining_to_stores(MancalaState* s) {
    // Collect user's remaining stones
    int sum = 0;
    for(int i = 0; i < 6; i++) {
        sum += s->pits_user[i];
        s->pits_user[i] = 0;
    }
    s->store_user += sum;
    
    // Collect AI's remaining stones
    sum = 0;
    for(int i = 0; i < 6; i++) {
        sum += s->pits_ai[i];
        s->pits_ai[i] = 0;
    }
    s->store_ai += sum;
}

/**
 * Check if game should end (one side has no stones)
 * If so, collect remaining stones and determine winner
 */
static void end_game_if_needed(MancalaState* s) {
    bool user_empty = true;
    bool ai_empty = true;
    
    // Check if either side has no stones left
    for(int i = 0; i < 6; i++) {
        if(s->pits_user[i] != 0) user_empty = false;
        if(s->pits_ai[i] != 0) ai_empty = false;
    }
    
    // If either side is empty, game ends
    if(user_empty || ai_empty) {
        collect_remaining_to_stores(s);
        s->game_over = true;
        
        // Determine winner
        if(s->store_user > s->store_ai) {
            strcpy(s->status_text, "YOU WIN");
        } else if(s->store_user < s->store_ai) {
            strcpy(s->status_text, "YOU LOSE");
        } else {
            strcpy(s->status_text, "DRAW");
        }
    }
}

/**
 * Perform a move from a selected pit
 * 
 * @param s Game state
 * @param pit_index Index (0..5) of the pit to move from
 * @param by_user true if user is moving, false if AI is moving
 * 
 * Implements standard Kalah rules:
 * - Sow stones counter-clockwise
 * - Skip opponent's store
 * - Extra turn if last stone lands in own store
 * - Capture if last stone lands in own empty pit
 */
static void do_move_from(MancalaState* s, int pit_index, bool by_user) {
    // Validate game state and pit selection
    if(s->game_over) return;
    if(pit_index < 0 || pit_index > 5) return;

    // Get stones from the selected pit
    int stones = by_user ? s->pits_user[pit_index] : s->pits_ai[pit_index];
    if(stones == 0) return;
    
    // Clear the pit
    if(by_user) {
        s->pits_user[pit_index] = 0;
    } else {
        s->pits_ai[pit_index] = 0;
    }

    /**
     * Board layout as slot indices for sowing:
     * 0: store_ai (left)
     * 1..6: ai pits[0..5] (left to right)
     * 7: store_user (right)
     * 8..13: user pits[0..5] (left to right)
     * Movement is counter-clockwise: 0->1->...->13->0
     */
    
    // Calculate starting slot index
    int slot_index;
    if(by_user) {
        // User pit: slots 8..13
        slot_index = 8 + pit_index;
    } else {
        // AI pit: slots 1..6
        slot_index = 1 + pit_index;
    }

    // Sow stones counter-clockwise
    int last_slot = -1;
    while(stones > 0) {
        // Move to next slot
        slot_index++;
        if(slot_index > 13) slot_index = 0;

        // Skip opponent's store (standard Kalah rule)
        if(by_user && slot_index == 0) {
            // User skips AI store
            slot_index++;
            if(slot_index > 13) slot_index = 0;
        }
        if(!by_user && slot_index == 7) {
            // AI skips user store
            slot_index++;
            if(slot_index > 13) slot_index = 0;
        }

        // Place one stone in current slot
        if(slot_index == 0) {
            s->store_ai += 1;
        } else if(slot_index >= 1 && slot_index <= 6) {
            s->pits_ai[slot_index - 1] += 1;
        } else if(slot_index == 7) {
            s->store_user += 1;
        } else { // 8..13
            s->pits_user[slot_index - 8] += 1;
        }
        
        stones--;
        last_slot = slot_index;
    }

    // Check for extra turn: last stone landed in own store
    bool extra_turn = false;
    if(by_user && last_slot == 7) extra_turn = true;
    if(!by_user && last_slot == 0) extra_turn = true;

    // Check for capture (only if no extra turn)
    // If last stone landed in an empty pit on own side, capture opposite pit
    if(!extra_turn) {
        if(by_user && last_slot >= 8 && last_slot <= 13) {
            // User's last stone in user pit
            int idx = last_slot - 8;
            if(s->pits_user[idx] == 1) { // Was empty before placing
                int opposite_idx = 5 - idx;
                int captured = s->pits_ai[opposite_idx];
                if(captured > 0) {
                    // Capture opposite pit and own stone
                    s->pits_ai[opposite_idx] = 0;
                    s->pits_user[idx] = 0;
                    s->store_user += (1 + captured);
                }
            }
        } else if(!by_user && last_slot >= 1 && last_slot <= 6) {
            // AI's last stone in AI pit
            int idx = last_slot - 1;
            if(s->pits_ai[idx] == 1) { // Was empty before placing
                int opposite_idx = 5 - idx;
                int captured = s->pits_user[opposite_idx];
                if(captured > 0) {
                    // Capture opposite pit and own stone
                    s->pits_user[opposite_idx] = 0;
                    s->pits_ai[idx] = 0;
                    s->store_ai += (1 + captured);
                }
            }
        }
    }

    // Set next turn based on extra turn rule
    if(extra_turn) {
        // Same player goes again
        s->turn = by_user ? TURN_USER : TURN_AI;
        if(by_user) {
            strcpy(s->status_text, "Extra turn!");
        } else {
            strcpy(s->status_text, "AI extra turn");
        }
    } else {
        // Switch to other player
        s->turn = by_user ? TURN_AI : TURN_USER;
        if(s->turn == TURN_USER) {
            snprintf(s->status_text, sizeof(s->status_text), 
                    "You: %d | AI: %d", s->store_user, s->store_ai);
        } else {
            strcpy(s->status_text, "AI thinking...");
        }
    }

    // Check if game has ended
    end_game_if_needed(s);
    s->redraw = true;
}

// =============================================================================
// AI IMPLEMENTATION (Heuristic-based move selection)
// =============================================================================
/**
 * 1. Extra turns (highest priority) - score +100
 * 2. Captures (medium priority) - score +50 plus captured stones
 * 3. Points gained - score +10 per stone added to store
 * 
 * @param s Current game state
 * @return Index (0..5) of best pit to move from, or -1 if no valid moves
 */
static int ai_choose_move(MancalaState* s) {
    int best_move = -1;
    int best_score = -1000;
    
    // Evaluate each possible move
    for(int i = 0; i < 6; i++) {
        if(s->pits_ai[i] == 0) continue; // Skip empty pits

        // Simulate move on temporary state copy
        MancalaState tmp = *s;
        
        // Perform simulated move
        int stones = tmp.pits_ai[i];
        tmp.pits_ai[i] = 0;
        int slot_index = 1 + i; // AI pit i maps to slot (1 + i)

        int last_slot = -1;
        while(stones > 0) {
            slot_index++;
            if(slot_index > 13) slot_index = 0;
            
            // Skip user store
            if(slot_index == 7) {
                slot_index++;
                if(slot_index > 13) slot_index = 0;
            }

            // Place stone
            if(slot_index == 0) {
                tmp.store_ai++;
            } else if(slot_index >= 1 && slot_index <= 6) {
                tmp.pits_ai[slot_index - 1]++;
            } else if(slot_index == 7) {
                tmp.store_user++;
            } else {
                tmp.pits_user[slot_index - 8]++;
            }
            
            stones--;
            last_slot = slot_index;
        }

        // Calculate score for this move
        int score = 0;
        
        // Check for extra turn (highest priority)
        bool extra_turn = (last_slot == 0);
        if(extra_turn) score += 100;
        
        // Check for capture (medium priority)
        if(!extra_turn) {
            if(last_slot >= 1 && last_slot <= 6) {
                int idx = last_slot - 1;
                if(tmp.pits_ai[idx] == 1) { // Was empty
                    int opposite_idx = 5 - idx;
                    if(tmp.pits_user[opposite_idx] > 0) {
                        // Value capture by amount captured
                        score += 50 + tmp.pits_user[opposite_idx];
                    }
                }
            }
        }
        
        // Favor moves that increase our score (base priority)
        score += (tmp.store_ai - s->store_ai) * 10;
        
        // Update best move if this is better
        if(score > best_score) {
            best_score = score;
            best_move = i;
        }
    }

    // Fallback: leftmost non-empty pit (should rarely be reached)
    if(best_move < 0) {
        for(int i = 0; i < 6; i++) {
            if(s->pits_ai[i] > 0) return i;
        }
    }
    
    return best_move;
}

// =============================================================================
// RENDERING FUNCTIONS
// =============================================================================
static void draw_board_background(Canvas* canvas, MancalaState* state) {
	canvas_draw_icon(canvas, 1, 1, &I_board); 
    canvas_draw_icon(canvas, 1, 1, &I_icon_10x10); // App icon comes on top of the board ^^
	canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 13, 1, AlignLeft, AlignTop, "Mancala");
    canvas_set_font(canvas, FontSecondary);
    
    // Draw navigation button hints (only when available)
    if(state->cursor < 5) elements_button_right(canvas, "");
    if(state->cursor > 0) elements_button_left(canvas, "");
    elements_button_center(canvas, "Pick pit"); // Action button hint
}

static void draw_numbers(Canvas* canvas, MancalaState* state) { 
    char buf[8];
    
    // Draw AI pits (top row)
    for(int i = 0; i < 6; i++) {
        snprintf(buf, sizeof(buf), "%d", state->pits_ai[i]);
        size_t len = strlen(buf);
        int x = pit_start_x + (i * pit_spacing);
        int tx = x - (len * 3); // Center text
        if(tx < 0) tx = 0; // Prevent negative coordinates
        canvas_draw_str(canvas, tx, ai_pit_y, buf);
    }
    
    // Draw user pits (bottom row)
    for(int i = 0; i < 6; i++) {
        snprintf(buf, sizeof(buf), "%d", state->pits_user[i]);
        size_t len = strlen(buf);
        int x = pit_start_x + (i * pit_spacing);
        int tx = x - (len * 3); // Center text
        if(tx < 0) tx = 0; // Prevent negative coordinates
        canvas_draw_str(canvas, tx, user_pit_y, buf);
    }
    
    // Draw store values
    snprintf(buf, sizeof(buf), "%d", state->store_ai);
    canvas_draw_str(canvas, store_ai_x, store_ai_y, buf);
    
    snprintf(buf, sizeof(buf), "%d", state->store_user);
    canvas_draw_str(canvas, store_user_x, store_user_y, buf);
}

static void draw_cursor(Canvas* canvas, MancalaState* state) {
    int i = state->cursor;
    int x = pit_start_x + (i * pit_spacing) - 7;
    int y = user_pit_y - 11;
    // Draw frame around selected pit
    canvas_draw_frame(canvas, x, y, 14, 14);
}

static void render_callback(Canvas* canvas, void* ctx) {
    MancalaState* state = (MancalaState*)ctx;
    canvas_clear(canvas);
    draw_board_background(canvas, state);
    draw_numbers(canvas, state);
    draw_cursor(canvas, state);
    // Draw status message
    canvas_draw_str_aligned(canvas, status_x, status_y, AlignLeft, AlignTop, state->status_text);

    // Draw game over overlay if applicable
    if(state->game_over) {
        canvas_draw_str_aligned(canvas, status_x, status_y, AlignLeft, AlignTop, state->status_text);
        elements_button_center(canvas, "Play again");
    }
}

// =============================================================================
// INPUT HANDLING FUNCTIONS
// =============================================================================

/**
 * Handle OK button press
 * - During game: pick stones from selected pit
 * - After game: restart game
 */
static void perform_user_action_ok(MancalaState* state) {
    // Restart game if it's over
    if(state->game_over) {
        init_game(state);
        return;
    }
    
    // Only allow moves on user's turn
    if(state->turn != TURN_USER) return;
    
    // Check if selected pit has stones
    int idx = state->cursor;
    if(state->pits_user[idx] == 0) {
        strcpy(state->status_text, "Empty pit!");
        state->redraw = true;
        return;
    }
    
    // Perform the move
    do_move_from(state, idx, true);
    state->redraw = true;
}

static void perform_ai_turn(MancalaState* state) {
	FURI_LOG_I(TAG, "perform_ai_turn() called.");
    if(state->game_over) return;
    
    // Choose move
    int pick = ai_choose_move(state);
    if(pick < 0) {
        // No valid moves - end game
        end_game_if_needed(state);
        return;
    }
    
    // Perform AI move
    do_move_from(state, pick, false);
}

static void input_handler(InputEvent* evt, void* ctx) {
    MancalaState* state = (MancalaState*)ctx;
    
    // Long back press exits application
    if(evt->type == InputTypeLong && evt->key == InputKeyBack) {
        state->should_exit = true;
        return;
    }

    // Handle short button presses
    if(evt->type == InputTypeShort) {
        if(evt->key == InputKeyRight) {
            // Move cursor right (no wrap)
            if(state->cursor < 5) {
                state->cursor++;
                state->redraw = true;
            }
        } else if(evt->key == InputKeyLeft) {
            // Move cursor left (no wrap)
            if(state->cursor > 0) {
                state->cursor--;
                state->redraw = true;
            }
        } else if(evt->key == InputKeyOk) {
            // Select pit / restart
            perform_user_action_ok(state);
        }
    }
}

// =============================================================================
// MAIN APPLICATION ENTRY POINT
// =============================================================================
int32_t mancala_main(void* p) {
    UNUSED(p);
    
    // Allocate and initialize game state
    MancalaState* state = malloc(sizeof(MancalaState));
    state->should_exit = false;
    init_game(state);
    
    // Create and configure view port
    state->view_port = view_port_alloc();
    view_port_draw_callback_set(state->view_port, render_callback, state);
    view_port_input_callback_set(state->view_port, input_handler, state);
    
    // Register with GUI system
    state->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(state->gui, state->view_port, GuiLayerFullscreen);
    
    FURI_LOG_I(TAG, "Mancala game initialized and ready.");

    // Main game loop ----------------------------------------------------------
    while(!state->should_exit) {
        // Execute AI turn if it's AI's turn and game is ongoing
        if(state->turn == TURN_AI && !state->game_over) {
            // Small delay makes AI moves visible to user
            furi_delay_ms(500);
            perform_ai_turn(state);
        }

        // Update display if redraw flag is set
        if(state->redraw) {
            view_port_update(state->view_port);
            state->redraw = false;
        }
        
        // Small delay to prevent busy-waiting
        furi_delay_ms(50);
    }

    // Cleanup: Free all allocated resources
    FURI_LOG_I(TAG, "Exiting Mancala game.");
    view_port_enabled_set(state->view_port, false);
    gui_remove_view_port(state->gui, state->view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(state->view_port);
    free(state);
    
    return 0;
}
