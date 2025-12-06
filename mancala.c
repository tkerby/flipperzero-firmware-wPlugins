#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include <stdint.h> // Standard integer types
#include <stdlib.h> // Standard library functions
#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <gui/gui.h> // GUI system
#include <gui/view_port.h>
#include <gui/canvas.h>
#include <gui/elements.h> // to access button drawing functions
#include <input/input.h> // Input handling (buttons)

extern const Icon I_board, I_icon_10x10;

// Game state
typedef enum {
    TURN_USER = 0,
    TURN_AI = 1
} Turn;

typedef struct {
    int pits_user[6];  // lower row (user)
    int pits_ai[6];    // upper row (ai)
    int store_user;    // right store (conventional)
    int store_ai;      // left store
    Turn turn;
    int cursor;        // selected index for user (0..5) left->right
    bool game_over;
    char status_text[32];
    bool redraw;
} MancalaState;

#define INIT_STONES 4

// Layout: position mapping for drawing numbers (approx)
// Y-positions only, X calculated based on pit index
static const int ai_pit_y = 5;
static const int user_pit_y = 22;
static const int pit_start_x = 18;  // X position of first pit
static const int pit_spacing = 15;  // Distance between pits

static const int store_ai_x = 2;
static const int store_ai_y = 8;
static const int store_user_x = 114;
static const int store_user_y = 8;

static MancalaState state;
static ViewPort* view_port = NULL;
static bool should_exit = false;

/* Utility functions */
static void init_game(MancalaState* s);
static void end_game_if_needed(MancalaState* s);
static void collect_remaining_to_stores(MancalaState* s);
static void do_move_from(MancalaState* s, int pit_index, bool by_user);
static int ai_choose_move(MancalaState* s);
static void render_callback(Canvas* canvas, void* ctx);
static void input_handler(InputEvent* evt, void* ctx);

/* ---------------------------------------------------------------------
   GAME LOGIC IMPLEMENTATION (Kalah)
   ------------------------------------------------------------------ */

static void init_game(MancalaState* s) {
    for(int i=0;i<6;i++){
        s->pits_user[i] = INIT_STONES;
        s->pits_ai[i] = INIT_STONES;
    }
    s->store_user = 0;
    s->store_ai = 0;
    s->turn = TURN_USER;
    s->cursor = 2; // default start cursor
    s->game_over = false;
    snprintf(s->status_text, sizeof(s->status_text), "You: 0 | AI: 0");
    s->redraw = true;
}

static void collect_remaining_to_stores(MancalaState* s){
    int sum=0;
    for(int i=0;i<6;i++){ sum += s->pits_user[i]; s->pits_user[i]=0; }
    s->store_user += sum;
    sum=0;
    for(int i=0;i<6;i++){ sum += s->pits_ai[i]; s->pits_ai[i]=0; }
    s->store_ai += sum;
}

static void end_game_if_needed(MancalaState* s){
    bool user_empty=true, ai_empty=true;
    for(int i=0;i<6;i++){
        if(s->pits_user[i] != 0) user_empty=false;
        if(s->pits_ai[i] != 0) ai_empty=false;
    }
    if(user_empty || ai_empty){
        collect_remaining_to_stores(s);
        s->game_over = true;
        if(s->store_user > s->store_ai) strcpy(s->status_text, "YOU WIN");
        else if(s->store_user < s->store_ai) strcpy(s->status_text, "YOU LOSE");
        else strcpy(s->status_text, "DRAW");
    }
}

/*
 * do_move_from:
 *   pit_index: index 0..5 relative to the player performing the move
 *   by_user: true if the moving side is the user, false if AI
 */
static void do_move_from(MancalaState* s, int pit_index, bool by_user){
    if(s->game_over) return;
    
    // Validate pit index early
    if(pit_index < 0 || pit_index > 5) return;

    // Get stones from the selected pit
    int stones = by_user ? s->pits_user[pit_index] : s->pits_ai[pit_index];
    if(stones == 0) return;
    
    // Clear the pit
    if(by_user){
        s->pits_user[pit_index] = 0;
    } else {
        s->pits_ai[pit_index] = 0;
    }

    // We will move counter-clockwise starting from the next pit.
    // Order of slots as indices:
    // AI store (left) -> AI pits[0..5] (left->right) -> USER store (right) -> USER pits[0..5] -> back to AI store
    // We'll step through slots using a small state machine.
    // Represent slots as:
    // 0: store_ai
    // 1..6: ai pits 0..5
    // 7: store_user
    // 8..13: user pits 0..5
    // start position depends where we picked from; compute index of next slot.

    int slot_index = 0;
    if(by_user){
        // picked from user pit pit_index -> our slot was 8 + pit_index
        slot_index = 8 + pit_index;
    } else {
        // picked from ai pit pit_index -> slot was 1 + pit_index
        slot_index = 1 + pit_index;
    }

    int last_slot = -1;
    while(stones > 0){
        // advance
        slot_index++;
        if(slot_index > 13) slot_index = 0;

        // Skip opponent store (standard Kalah rules)
        if(by_user && slot_index == 0) continue; // user skips AI store
        if(!by_user && slot_index == 7) continue; // AI skips user store

        // place stone
        if(slot_index == 0){
            s->store_ai += 1;
        } else if(slot_index >=1 && slot_index <=6){
            s->pits_ai[slot_index - 1] += 1;
        } else if(slot_index == 7){
            s->store_user += 1;
        } else { // 8..13
            s->pits_user[slot_index - 8] += 1;
        }
        stones--;
        last_slot = slot_index;
    }

    // After sowing, check extra turn: if last stone landed in player's store -> same player's turn
    bool extra_turn = false;
    if(by_user && last_slot == 7) extra_turn = true;
    if(!by_user && last_slot == 0) extra_turn = true;

    // Check capture: if last stone landed in an empty pit on own side, capture that stone and opposite
    if(!extra_turn){
        if(by_user && last_slot >= 8 && last_slot <= 13){
            int idx = last_slot - 8;
            if(s->pits_user[idx] == 1){ // it was empty before placing
                int opposite_idx = 5 - idx;
                int captured = s->pits_ai[opposite_idx];
                if(captured > 0){
                    s->pits_ai[opposite_idx] = 0;
                    s->pits_user[idx] = 0;
                    s->store_user += (1 + captured);
                }
            }
        } else if(!by_user && last_slot >=1 && last_slot <=6){
            int idx = last_slot - 1;
            if(s->pits_ai[idx] == 1){
                int opposite_idx = 5 - idx;
                int captured = s->pits_user[opposite_idx];
                if(captured > 0){
                    s->pits_user[opposite_idx] = 0;
                    s->pits_ai[idx] = 0;
                    s->store_ai += (1 + captured);
                }
            }
        }
    }

    // Set next turn
    if(extra_turn){
        s->turn = by_user ? TURN_USER : TURN_AI;
        if(by_user) strcpy(s->status_text, "Extra turn!");
        else strcpy(s->status_text, "AI extra turn");
    } else {
        s->turn = by_user ? TURN_AI : TURN_USER;
        if(s->turn == TURN_USER) {
            snprintf(s->status_text, sizeof(s->status_text), 
                    "You: %d | AI: %d", s->store_user, s->store_ai);
        } else {
            strcpy(s->status_text, "AI thinking...");
        }
    }

    end_game_if_needed(s);
    s->redraw = true;
}

/* ---------------------------------------------------------------------
   AI (improved heuristic with scoring)
   ------------------------------------------------------------------ */
/*
 * For each possible pit (0..5):
 *  - simulate move and calculate score based on:
 *    - extra turns (highest priority)
 *    - captures (medium priority)
 *    - points gained (base value)
 *  - select move with best score
 */
static int ai_choose_move(MancalaState* s){
    int best_move = -1;
    int best_score = -1000;
    
    // Evaluate each possible move
    for(int i=0; i<6; i++){
        if(s->pits_ai[i] == 0) continue;

        // Copy state for simulation
        MancalaState tmp = *s;
        
        // Perform move as AI
        int stones = tmp.pits_ai[i];
        tmp.pits_ai[i] = 0;
        int slot_index = 1 + i; // AI pit was 1..6

        int last_slot = -1;
        while(stones > 0){
            slot_index++;
            if(slot_index > 13) slot_index = 0;
            if(slot_index == 7) continue; // AI skip opponent store

            if(slot_index == 0) tmp.store_ai++;
            else if(slot_index >=1 && slot_index <=6) tmp.pits_ai[slot_index - 1]++;
            else if(slot_index == 7) tmp.store_user++;
            else tmp.pits_user[slot_index - 8]++;
            stones--;
            last_slot = slot_index;
        }

        // Calculate score for this move
        int score = 0;
        
        // Check for extra turn (highest priority)
        bool extra_turn = (last_slot == 0);
        if(extra_turn) score += 100;
        
        // Check for capture (medium priority)
        if(!extra_turn){
            if(last_slot >=1 && last_slot <=6){
                int idx = last_slot - 1;
                if(tmp.pits_ai[idx] == 1){
                    int opposite_idx = 5 - idx;
                    if(tmp.pits_user[opposite_idx] > 0) {
                        score += 50 + tmp.pits_user[opposite_idx]; // Value capture by amount
                    }
                }
            }
        }
        
        // Favor moves that increase our score
        score += (tmp.store_ai - s->store_ai) * 10;
        
        // Update best move if this is better
        if(score > best_score){
            best_score = score;
            best_move = i;
        }
    }

    // Fallback: leftmost non-empty pit (should not be reached with scoring system)
    if(best_move < 0){
        for(int i=0; i<6; i++){
            if(s->pits_ai[i] > 0) return i;
        }
    }
    
    return best_move;
}

// =============================================================================
// RENDERING
// =============================================================================
static void draw_board_background(Canvas* canvas){
    // canvas_draw_icon(canvas, 1, -1, &I_icon_10x10); // App icon
	// canvas_draw_icon(canvas, 1, 1, &I_board); 
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 13, 1, AlignLeft, AlignTop, "Mancala"); // App title 
	elements_button_right(canvas, ">");
	elements_button_left(canvas, "<");
	elements_button_center(canvas, "Pick pit");
}

static void draw_numbers(Canvas* canvas){
    char buf[8];
    
    // AI pits
    for(int i=0; i<6; i++){
        snprintf(buf, sizeof(buf), "%d", state.pits_ai[i]);
        size_t len = strlen(buf);
        int x = pit_start_x + (i * pit_spacing);
        int tx = x - (len * 3);
        if(tx < 0) tx = 0; // Prevent negative coordinates
        canvas_draw_str(canvas, tx, ai_pit_y, buf);
    }
    
    // User pits
    for(int i=0; i<6; i++){
        snprintf(buf, sizeof(buf), "%d", state.pits_user[i]);
        size_t len = strlen(buf);
        int x = pit_start_x + (i * pit_spacing);
        int tx = x - (len * 3);
        if(tx < 0) tx = 0; // Prevent negative coordinates
        canvas_draw_str(canvas, tx, user_pit_y, buf);
    }
    
    // stores
    snprintf(buf, sizeof(buf), "%d", state.store_ai);
    canvas_draw_str(canvas, store_ai_x, store_ai_y, buf);
    snprintf(buf, sizeof(buf), "%d", state.store_user);
    canvas_draw_str(canvas, store_user_x, store_user_y, buf);
}

static void draw_cursor(Canvas* canvas){
    // highlight selected user pit
    int i = state.cursor;
    int x = pit_start_x + (i * pit_spacing) - 7;
    int y = user_pit_y - 7;
    // draw a rectangle highlight (invert effect)
    canvas_draw_frame(canvas, x, y, 14, 14);
}

static void draw_status(Canvas* canvas){
    // status text at top center
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, AlignTop, state.status_text);
}

static void render_callback(Canvas* canvas, void* ctx){
	(void)ctx;  
    canvas_clear(canvas);

    draw_board_background(canvas);
    draw_numbers(canvas);
    draw_cursor(canvas);
    draw_status(canvas);

    if(state.game_over){
        // overlay message big-ish
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, AlignTop, state.status_text);
        canvas_draw_str_aligned(canvas, 64, 24, AlignCenter, AlignTop, "Press OK to restart");
    }
}

/* ---------------------------------------------------------------------
   INPUT / MAIN LOOP
   ------------------------------------------------------------------ */

static void perform_user_action_ok(){
    // If game is over, restart game
    if(state.game_over) {
        init_game(&state);
        return;
    }
    
    // only allow picking from user's pits when it's user's turn
    if(state.turn != TURN_USER) return;
    
    int idx = state.cursor;
    if(state.pits_user[idx] == 0){
        // nothing there
        strcpy(state.status_text, "Empty pit!");
        state.redraw = true;
        return;
    }
    do_move_from(&state, idx, true);
    state.redraw = true;
}

static void perform_ai_turn(){
    if(state.game_over) return;
    
    int pick = ai_choose_move(&state);
    if(pick < 0){
        // no move possible
        end_game_if_needed(&state);
        return;
    }
    do_move_from(&state, pick, false);
}

/* Input callback from view_port */
static void input_handler(InputEvent* evt, void* ctx){
	(void)ctx;  
    if(evt->type == InputTypeLong && evt->key == InputKeyBack){
        // long back -> exit
        should_exit = true;
        return;
    }

    if(evt->type == InputTypeShort){
        if(evt->key == InputKeyRight){
            // move cursor right (wrap)
            state.cursor = (state.cursor + 1) % 6;
            state.redraw = true;
        } else if(evt->key == InputKeyLeft){
            // move cursor left (wrap)
            state.cursor = (state.cursor + 5) % 6;
            state.redraw = true;
        } else if(evt->key == InputKeyOk){
            perform_user_action_ok();
        }
    }
}

// =============================================================================
// MAIN APPLICATION
// =============================================================================
int32_t mancala_main(void* p){
    UNUSED(p);

    init_game(&state);

    // create view port
    view_port = view_port_alloc();
    if(!view_port) return -1; // Check allocation
    
    view_port_draw_callback_set(view_port, render_callback, NULL);
    view_port_input_callback_set(view_port, input_handler, NULL);

    // attach to GUI
    Gui* gui = furi_record_open("gui");
    if(!gui) {
        view_port_free(view_port);
        return -1;
    }
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Main loop that polls and lets AI act when it's their turn.
    while(!should_exit){
        // If it's AI's turn and not game over -> perform AI action with delay
        if(state.turn == TURN_AI && !state.game_over){
            // small delay to make AI moves observable
            furi_delay_ms(500);
            perform_ai_turn();
        }

        // request redraw if needed
        if(state.redraw){
            view_port_update(view_port);
            state.redraw = false;
        }
        
        furi_delay_ms(50); // Small delay to prevent busy-waiting
    }

    // Cleanup
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close("gui");
    
    return 0;
}
