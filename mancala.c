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

#include "mitzi_mancala_icons.h" // Custom icon definitions

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
typedef struct {
    int x, y;
} Pos;

static const Pos ai_pit_pos[6] = {
    {18, 5}, {33, 5}, {48, 5}, {63, 5}, {78, 5}, {93, 5}
};

static const Pos user_pit_pos[6] = {
    {18, 22}, {33, 22}, {48, 22}, {63, 22}, {78, 22}, {93, 22}
};

static const Pos store_ai_pos = {2, 8};     // left tall store
static const Pos store_user_pos = {114, 8}; // right tall store

static MancalaState state;
static ViewPort* view_port = NULL;
static FuriMessageQueue* event_queue = NULL;
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
    strcpy(s->status_text, "Your turn");
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
 *
 * Implements Kalah sowing, skipping opponent store, extra turn, and capture.
 */
static void do_move_from(MancalaState* s, int pit_index, bool by_user){
    if(s->game_over) return;

    int stones = 0;
    if(by_user){
        if(pit_index < 0 || pit_index > 5) return;
        stones = s->pits_user[pit_index];
        if(stones == 0) return;
        s->pits_user[pit_index] = 0;
    } else {
        if(pit_index < 0 || pit_index > 5) return;
        stones = s->pits_ai[pit_index];
        if(stones == 0) return;
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

        // Skip opponent store
        if(by_user && slot_index == 0) continue; // user skip ai store? Actually user places into own store (7), skip AI store (0) when sowing? Wait: standard Kalah you skip the opponent store.
        if(!by_user && slot_index == 7) continue;

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
        if(by_user) strcpy(s->status_text, "You get extra turn");
        else strcpy(s->status_text, "AI extra turn");
    } else {
        s->turn = by_user ? TURN_AI : TURN_USER;
        if(s->turn == TURN_USER) strcpy(s->status_text, "Your turn");
        else strcpy(s->status_text, "AI thinking");
    }

    end_game_if_needed(s);
    s->redraw = true;
}

/* ---------------------------------------------------------------------
   AI (simple heuristic)
   ------------------------------------------------------------------ */

/*
 * For each possible pit (0..5):
 *  - simulate move and see if it yields extra turn (best)
 *  - else if it yields capture (second priority)
 *  - else fall back to leftmost non-empty
 *
 * Uses a simplified in-place simulation on copies.
 */
static int ai_choose_move(MancalaState* s){
    // helper simulation
    for(int priority=0;priority<3;priority++){
        for(int i=0;i<6;i++){
            if(s->pits_ai[i] == 0) continue;

            // copy state
            MancalaState tmp = *s;
            // perform move as AI
            int stones = tmp.pits_ai[i];
            tmp.pits_ai[i] = 0;
            int slot_index = 1 + i; // AI pit was 1..6

            int last_slot = -1;
            while(stones > 0){
                slot_index++;
                if(slot_index > 13) slot_index = 0;
                if(/*AI skip opponent store*/ slot_index == 7) continue;

                if(slot_index == 0) tmp.store_ai++;
                else if(slot_index >=1 && slot_index <=6) tmp.pits_ai[slot_index - 1]++;
                else if(slot_index == 7) tmp.store_user++;
                else tmp.pits_user[slot_index - 8]++;
                stones--;
                last_slot = slot_index;
            }

            bool extra_turn = (last_slot == 0);
            bool capture = false;
            if(!extra_turn){
                if(last_slot >=1 && last_slot <=6){
                    int idx = last_slot - 1;
                    if(tmp.pits_ai[idx] == 1){
                        int opposite_idx = 5 - idx;
                        if(tmp.pits_user[opposite_idx] > 0) capture = true;
                    }
                }
            }

            if(priority == 0 && extra_turn) return i;
            if(priority == 1 && capture) return i;
            if(priority == 2) ; // leftmost fallback will be returned below
        }
    }

    // fallback: leftmost non-empty
    for(int i=0;i<6;i++) if(s->pits_ai[i] > 0) return i;
    return -1;
}

// =============================================================================
// RENDERING
// =============================================================================
static void draw_board_background(Canvas* canvas){
#if BOARD_BITMAP_AVAILABLE
    // Draw pre-rendered bitmap occupying full screen 128x32
    // canvas_draw_bitmap requires a bitmap structure. We'll draw raw bytes using canvas_draw_bitmap or canvas_draw_bitmap_raw if available.
    // Many SDKs provide canvas_draw_bitmap(canvas, 0, 0, width, height, data)
    // We'll try a conservative approach. If compile-time fails, replace with fallback rectangle drawing.
    canvas_draw_bitmap(canvas, 0, 0, 128, 32, board_bitmap);
#else
    // draw schematic: stores and pits outlines
    // left store
    canvas_draw_rect(canvas, 2, 2, 12, 28);
    // right store
    canvas_draw_rect(canvas, 114, 2, 12, 28);
    // top (ai) pits
    for(int i=0;i<6;i++){
        int x = ai_pit_pos[i].x - 6;
        int y = ai_pit_pos[i].y - 6;
        canvas_draw_rect(canvas, x, y, 12, 12);
    }
    // bottom (user) pits
    for(int i=0;i<6;i++){
        int x = user_pit_pos[i].x - 6;
        int y = user_pit_pos[i].y - 6;
        canvas_draw_rect(canvas, x, y, 12, 12);
    }
#endif
}

static void draw_numbers(Canvas* canvas){
    char buf[8];
    // AI pits
    for(int i=0;i<6;i++){
        snprintf(buf, sizeof(buf), "%d", state.pits_ai[i]);
        // center text
        int tx = ai_pit_pos[i].x - (strlen(buf)*3);
        canvas_draw_str(canvas, tx, ai_pit_pos[i].y, buf);
    }
    // User pits
    for(int i=0;i<6;i++){
        snprintf(buf, sizeof(buf), "%d", state.pits_user[i]);
        int tx = user_pit_pos[i].x - (strlen(buf)*3);
        canvas_draw_str(canvas, tx, user_pit_pos[i].y, buf);
    }
    // stores
    snprintf(buf, sizeof(buf), "%d", state.store_ai);
    canvas_draw_str(canvas, store_ai_pos.x, store_ai_pos.y, buf);
    snprintf(buf, sizeof(buf), "%d", state.store_user);
    canvas_draw_str(canvas, store_user_pos.x, store_user_pos.y, buf);
}

static void draw_cursor(Canvas* canvas){
    // highlight selected user pit
    int i = state.cursor;
    int x = user_pit_pos[i].x - 7;
    int y = user_pit_pos[i].y - 7;
    // draw a rectangle highlight (invert effect)
    canvas_draw_rect(canvas, x, y, 14, 14);
}

static void draw_status(Canvas* canvas){
    // small status text at top-right or center
    canvas_draw_str_aligned(canvas, 64, 0, AlignCenter, state.status_text);
}

static void render_callback(Canvas* canvas, void* ctx){
    // clear
    canvas_clear(canvas);

    draw_board_background(canvas);
    draw_numbers(canvas);
    draw_cursor(canvas);
    draw_status(canvas);

    if(state.game_over){
        // overlay message big-ish
        canvas_draw_str_aligned(canvas, 64, 14, AlignCenter, state.status_text);
    }
}

/* ---------------------------------------------------------------------
   INPUT / MAIN LOOP
   ------------------------------------------------------------------ */

static void perform_user_action_ok(){
    if(state.game_over) return;
    // only allow picking from user's pits when it's user's turn
    if(state.turn != TURN_USER) return;
    int idx = state.cursor;
    if(state.pits_user[idx] == 0){
        // nothing there
        strcpy(state.status_text, "Empty pit");
        state.redraw = true;
        return;
    }
    do_move_from(&state, idx, true);
    state.redraw = true;
}

static void perform_ai_turn(){
    if(state.game_over) return;
    // simple delay for visible effect
    // In Flipper, sleeping in UI thread isn't good; we set status and let loop iterate
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
            state.cursor = (state.cursor + 5) % 6;
            state.redraw = true;
        } else if(evt->key == InputKeyOk || evt->key == InputKeyEnter){
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

    // create a queue to marshal events if needed
    event_queue = furi_message_queue_alloc(8, sizeof(InputEvent*));

    // create view port
    view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, render_callback, NULL);
    view_port_input_callback_set(view_port, input_handler, NULL);

    // attach to GUI
    Gui* gui = furi_record_open("gui");
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Main loop that polls and lets AI act when her turn.
    while(!should_exit){
        // If it's AI's turn and not game over -> perform AI action and short delay
        if(state.turn == TURN_AI && !state.game_over){
            // small delay to make AI moves observable
            furi_delay_ms(200);
            perform_ai_turn();
        }

        // request redraw if needed
        if(state.redraw){
            gui_request_full_redraw(gui);
            state.redraw = false;
        }
    }

    // Cleanup
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close("gui");
    if(event_queue) furi_message_queue_free(event_queue);
    return 0;
}

