#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include "p1x_flipper_and_watch_icons.h"

// Game constants
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define MAX_PACKETS 10  // Increased DDOS limit for a smoother start
#define HACK_WARN_TIME 2000  // 2 seconds warning (in ms)
#define HACK_TIME 5000       // 5 seconds to fully hack (in ms)
#define PATCH_TIME 2000      // 2 seconds to patch (in ms)
#define PACKET_ANIMATION_TIME 300 // Faster animation for accepted packets
#define PACKET_SPAWN_CHANCE 1  // Reduced packet spawn chance for slower gameplay
#define PACKET_SPAWN_INTERVAL 3000  // Increased interval to 300 seconds for much slower packet arrival

// Add upcoming packets queue to game state
#define MAX_UPCOMING_PACKETS 3

// Define game states - remove duplicates
#define GAME_STATE_TITLE 0
#define GAME_STATE_MENU 1
#define GAME_STATE_HELP 2
#define GAME_STATE_PLAY 3

// System positions (4 corners)
typedef struct {
    int x, y;
} Position;

static const Position SYSTEM_POSITIONS[4] = {
    {32, 21},  // Top-left (moved down by 5px)
    {96, 21},  // Top-right (moved down by 5px)
    {32, 53},  // Bottom-left (moved down by 5px)
    {96, 53}   // Bottom-right (moved down by 5px)
};

// Adjust player positions to be inside the computer boxes
static const Position PLAYER_POSITIONS[4] = {
    {32, 21},  // Inside top-left system
    {96, 21},  // Inside top-right system
    {32, 53},  // Inside bottom-left system
    {96, 53}   // Inside bottom-right system
};

// Server position (center)
static const Position SERVER_POSITION = {64, 37}; // Center server (moved down by 5px)

// Game state
typedef struct {
    int player_pos;          // 0-3 for the 4 positions
    bool hacking[4];         // Is system being hacked?
    uint32_t hack_start[4];  // When hacking began (ms)
    uint32_t warn_start[4];  // When warning began (ms)
    int packets[4];          // Packets waiting at each position
    bool patching;           // Currently patching?
    uint32_t patch_start;    // When patching began (ms)
    bool game_over;
    int game_over_reason;    // 1=hack, 2=DDOS
    bool packet_animation;   // Packet moving to server
    uint32_t anim_start;     // When animation began
    int anim_from;           // Which system accepted the packet
    int score;               // Player's score
    int upcoming_packets[4][MAX_UPCOMING_PACKETS]; // Queue of upcoming packets for each system
    uint32_t last_packet_spawn_time; // Last packet spawn time
    int current_state;       // Current game state
    int help_page;           // Help page number
    int menu_selection;      // Menu selection index
} GameState;

// Game over reasons
#define GAME_OVER_HACK 1
#define GAME_OVER_DDOS 2

// Reset game state for new game
static void reset_game_state(GameState* state) {
    state->player_pos = 0;
    state->patching = false;
    state->game_over = false;
    state->game_over_reason = 0;
    state->packet_animation = false;
    state->score = 0;
    state->last_packet_spawn_time = furi_get_tick();
    state->menu_selection = 0;
    state->help_page = 0;
    for(int i = 0; i < 4; i++) {
        state->hacking[i] = false;
        state->hack_start[i] = 0;
        state->warn_start[i] = 0;
        state->packets[i] = 0;
        for(int j = 0; j < MAX_UPCOMING_PACKETS; j++) {
            state->upcoming_packets[i][j] = rand() % 2;
        }
    }
}

// Initialize game state
static void game_init(GameState* state) {
    state->current_state = GAME_STATE_TITLE;
    reset_game_state(state);
}

// Draw title screen
static void draw_title_screen(Canvas* canvas) {
    canvas_clear(canvas);
    
    // Display title graphics - using correct icon names
    canvas_draw_icon(canvas, 9, 10, &I_title_network);
    canvas_draw_icon(canvas, 10, 39, &I_title_defender);
    canvas_draw_icon(canvas, 94, 30, &I_player_left);
    
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, 30, 59, "Press OK to continue");
}

// Draw menu screen with selection indicator
static void draw_menu_screen(Canvas* canvas, int selection) {
    canvas_clear(canvas);
    
    // Add title without frame
    canvas_draw_icon(canvas, 9, 5, &I_title_network);
    canvas_draw_icon(canvas, 10, 20, &I_title_defender);
    
    // Draw menu options with selection indicator
    canvas_set_font(canvas, FontSecondary);
    
    // Selection indicator
    canvas_draw_str(canvas, 10, 40 + (selection * 10), ">");
    
    canvas_draw_str(canvas, 20, 40, "Start Game");
    canvas_draw_str(canvas, 20, 50, "Help");
    canvas_draw_str(canvas, 20, 60, "Quit");
}

// Draw help screen with optimized layout
static void draw_help_screen(Canvas* canvas, int page) {
    canvas_clear(canvas);
    
    canvas_set_font(canvas, FontSecondary);
    
    if(page == 0) {
        // Page 1: Icons explanation - more compact layout
        canvas_draw_str(canvas, 5, 10, "ICONS:");
        
        // Packet icon
        canvas_draw_icon(canvas, 10, 14, &I_packet);
        canvas_draw_str(canvas, 20, 21, "- Data packet");
        
        // Computer icon
        canvas_draw_icon(canvas, 10, 25, &I_pc);
        canvas_draw_str(canvas, 20, 32, "- Computer system");
        
        // Warning icon
        canvas_draw_str(canvas, 10, 43, "!");
        canvas_draw_str(canvas, 20, 43, "- Hack warning");
        
        // Active hack icon
        canvas_draw_str(canvas, 10, 54, "!!!");
        canvas_draw_str(canvas, 20, 54, "- Active hack attack");
    } else {
        // Page 2: Gameplay instructions - more compact layout
        canvas_draw_str(canvas, 5, 10, "HOW TO PLAY:");
        canvas_draw_str(canvas, 5, 20, "- Move with D-pad");
        canvas_draw_str(canvas, 5, 30, "- Press OK to accept packet");
        canvas_draw_str(canvas, 5, 40, "- Hold OK for 3s to patch");
        canvas_draw_str(canvas, 5, 50, "- Keep packets below limit");
        canvas_draw_str(canvas, 5, 60, "- Max 10 total packets");
    }
    
    // Small page indicator at bottom right
    canvas_draw_str(canvas, SCREEN_WIDTH - 10, SCREEN_HEIGHT - 4, page == 0 ? "1/2" : "2/2");
}

// Draw callback for GUI
static void draw_callback(Canvas* canvas, void* ctx) {
    GameState* state = (GameState*)ctx;

    if(state->current_state == GAME_STATE_TITLE) {
        draw_title_screen(canvas);
        return;
    } else if(state->current_state == GAME_STATE_MENU) {
        draw_menu_screen(canvas, state->menu_selection);
        return;
    } else if(state->current_state == GAME_STATE_HELP) {
        draw_help_screen(canvas, state->help_page);
        return;
    }

    // Draw game frame
    canvas_draw_icon(canvas, 0, 0, &I_frame);
    
    if(state->game_over) {
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 30, 20, "GAME OVER");
        
        canvas_set_font(canvas, FontSecondary);
        if(state->game_over_reason == GAME_OVER_HACK) {
            canvas_draw_str(canvas, 15, 35, "System hacked!");
        } else if(state->game_over_reason == GAME_OVER_DDOS) {
            canvas_draw_str(canvas, 15, 35, "DDOS attack!");
        }

        char score_text[32];
        snprintf(score_text, sizeof(score_text), "Score: %d", state->score);
        canvas_draw_str(canvas, 15, 50, score_text);

        canvas_draw_str(canvas, 15, 60, "Press BACK to restart");
        return;
    }

    // Draw server in center instead of a rectangle
    canvas_draw_icon(canvas, SERVER_POSITION.x - 8, SERVER_POSITION.y - 8, &I_pc);

    // Packet count near server
    int total_packets = 0;
    for(int i = 0; i < 4; i++) total_packets += state->packets[i];
    char buffer[16];
    snprintf(buffer, sizeof(buffer), "%d/%d", total_packets, MAX_PACKETS);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str(canvas, SERVER_POSITION.x - canvas_string_width(canvas, buffer)/2, 
                   SERVER_POSITION.y + 12, buffer);

    // Draw systems and states
    for(int i = 0; i < 4; i++) {
        int x = SYSTEM_POSITIONS[i].x;
        int y = SYSTEM_POSITIONS[i].y;

        // Draw system (computer icon) - pc.png is the server, but we'll use it for computers too
        canvas_draw_icon(canvas, x - 8, y - 8, &I_pc);

        // Hacking warning or active hack
        uint32_t now = furi_get_tick();
        if(state->warn_start[i] && !state->hacking[i]) {
            if((now - state->warn_start[i]) / 500 % 2 == 0) { // Flash every 0.5s
                canvas_draw_str(canvas, x - 4, y - 12, "!");
            }
        } else if(state->hacking[i]) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, x - 4, y - 12, "!!!");
            canvas_set_font(canvas, FontPrimary);
        }

        // Draw packets at the system
        for(int p = 0; p < state->packets[i] && p < 3; p++) {
            int packet_x = (i % 2 == 0) ? x + 10 + p*5 : x - 14 - p*5;
            canvas_draw_icon(canvas, packet_x, y - 3, &I_packet);
        }
    }
    
    // Draw upcoming packets with icons instead of circles
    for(int i = 0; i < 4; i++) {
        int x = SYSTEM_POSITIONS[i].x;
        int y = SYSTEM_POSITIONS[i].y;
        
        // Determine direction based on system position (left or right)
        bool is_right_side = (i % 2 == 1); // Systems 1 and 3 are on the right
        
        for(int j = 0; j < MAX_UPCOMING_PACKETS; j++) {
            // Calculate packet position with more spacing
            int packet_x = is_right_side ? 
                (x + 12 + j * 8) :  // Right side systems, packets on right
                (x - 18 - j * 8);   // Left side systems, packets on left
            
            if(state->upcoming_packets[i][j] == 1) {
                // Use packet icon instead of filled circle
                canvas_draw_icon(canvas, packet_x, y - 3, &I_packet);
            } else {
                // Empty circle for empty slot - keep this as circle for visibility
                canvas_draw_circle(canvas, packet_x + 3, y, 2);
            }
        }
    }

    // Draw packet animation if active
    if(state->packet_animation) {
        uint32_t now = furi_get_tick();
        float progress = (float)(now - state->anim_start) / PACKET_ANIMATION_TIME;
        if(progress > 1.0f) progress = 1.0f;
        
        // Source position
        int sx = SYSTEM_POSITIONS[state->anim_from].x;
        int sy = SYSTEM_POSITIONS[state->anim_from].y;
        
        // Calculate interpolated position
        int px = sx + (SERVER_POSITION.x - sx) * progress;
        int py = sy + (SERVER_POSITION.y - sy) * progress;
        
        // Draw packet moving toward server
        canvas_draw_icon(canvas, px - 3, py - 3, &I_packet);
    }

    // Draw player using appropriate sprite based on position
    int px = PLAYER_POSITIONS[state->player_pos].x;
    int py = PLAYER_POSITIONS[state->player_pos].y;
    
    if(state->patching) {
        // When patching, show a different animation
        if((furi_get_tick() / 200) % 2) {
            canvas_draw_icon(canvas, px - 8, py - 8, (state->player_pos % 2 == 0) ? &I_player_left : &I_player_right);
        } else {
            // Alternate frame for patching animation
            canvas_draw_circle(canvas, px, py, 4);
        }
    } else {
        // Normal player display - use left or right facing sprite based on position
        canvas_draw_icon(canvas, px - 8, py - 8, (state->player_pos % 2 == 0) ? &I_player_left : &I_player_right);
    }

    // Move score display to center-top
    canvas_set_font(canvas, FontSecondary);
    char score_text[16];
    snprintf(score_text, sizeof(score_text), "Score: %d", state->score);
    canvas_draw_str(canvas, SCREEN_WIDTH / 2 - canvas_string_width(canvas, score_text) / 2, 10, score_text);
}

// Input handling
static void input_callback(InputEvent* input, void* ctx) {
    FuriMessageQueue* queue = (FuriMessageQueue*)ctx;
    furi_message_queue_put(queue, input, FuriWaitForever);
}

// Fixed update_upcoming_packets to ensure indicators are synced with arriving packets
static void update_upcoming_packets(GameState* state) {
    uint32_t now = furi_get_tick();
    
    // Only update packets at fixed intervals
    if(now - state->last_packet_spawn_time < PACKET_SPAWN_INTERVAL) {
        return;
    }
    
    state->last_packet_spawn_time = now;
    
    // Process all computers simultaneously - conveyor belt style
    
    // 1. Check if first packets arrive at computers
    for(int i = 0; i < 4; i++) {
        // If the first slot has a packet and computer can accept it
        if(state->upcoming_packets[i][0] == 1) {
            if(state->packets[i] < 3) {
                state->packets[i]++;
            }
            // Always mark as processed whether it was accepted or not
            state->upcoming_packets[i][0] = 0;
        }
    }
    
    // 2. Shift everything one step closer (conveyor belt movement)
    for(int i = 0; i < 4; i++) {
        for(int j = 0; j < MAX_UPCOMING_PACKETS - 1; j++) {
            state->upcoming_packets[i][j] = state->upcoming_packets[i][j + 1];
        }
    }
    
    // 3. Generate new packets in the last position - synchronized for all computers
    // Determine if this tick will generate packets (20% chance)
    bool generate_packet = (rand() % 5 == 0);
    
    // Apply to all computers
    for(int i = 0; i < 4; i++) {
        state->upcoming_packets[i][MAX_UPCOMING_PACKETS - 1] = generate_packet ? 1 : 0;
    }
}

// Update game logic - remove redundant packet handling code
static void update_game(GameState* state) {
    // Only run game logic if we're in play state
    if(state->current_state != GAME_STATE_PLAY) return;
    
    if(state->game_over) return;

    uint32_t now = furi_get_tick();

    // Update packet animation
    if(state->packet_animation && now - state->anim_start >= PACKET_ANIMATION_TIME) {
        state->packet_animation = false;
    }

    // Check for hacks and warnings
    for(int i = 0; i < 4; i++) {
        if(state->warn_start[i] && !state->hacking[i] && now - state->warn_start[i] >= HACK_WARN_TIME) {
            state->hacking[i] = true;
            state->hack_start[i] = now;
            state->warn_start[i] = 0;
            furi_hal_vibro_on(true); // Vibrate when hack starts
            furi_delay_ms(200);
            furi_hal_vibro_on(false);
        }
        if(state->hacking[i] && now - state->hack_start[i] >= HACK_TIME) {
            state->game_over = true; // Hack completed
            state->game_over_reason = GAME_OVER_HACK;
            furi_hal_vibro_on(true); // Long vibration for game over
            furi_delay_ms(500);
            furi_hal_vibro_on(false);
            return;
        }
    }

    // Patching logic
    if(state->patching && now - state->patch_start >= PATCH_TIME) {
        int pos = state->player_pos;
        if(state->hacking[pos]) {
            state->hacking[pos] = false;
            state->hack_start[pos] = 0;
            state->score += 30; // Add 30 points for preventing hacking
            furi_hal_vibro_on(true); // Short vibrate for success
            furi_delay_ms(100);
            furi_hal_vibro_on(false);
        }
        state->patching = false;
    }

    // Randomly spawn warnings or packets - limit to 2 active hack attempts
    if(rand() % 100 < 1) { // 1% chance for hack warnings
        // Count active warnings/hacking
        int active_hacks = 0;
        for(int i = 0; i < 4; i++) {
            if(state->hacking[i] || state->warn_start[i]) {
                active_hacks++;
            }
        }
        
        // Only spawn new warning if we have fewer than 2 active hack attempts
        if(active_hacks < 2) {
            int target = rand() % 4;
            if(!state->hacking[target] && !state->warn_start[target]) {
                state->warn_start[target] = now;
                furi_hal_vibro_on(true); // Short vibrate for warning
                furi_delay_ms(100);
                furi_hal_vibro_on(false);
            }
        }
    } else {
        // Existing packet spawn logic continues here
        // ...existing code...
    }

    // Check DDOS - only active packets count towards the DDOS limit
    int total_packets = 0;
    for(int i = 0; i < 4; i++) total_packets += state->packets[i];
    if(total_packets >= MAX_PACKETS) {
        state->game_over = true;
        state->game_over_reason = GAME_OVER_DDOS;
        furi_hal_vibro_on(true); // Long vibrate for game over
        furi_delay_ms(500);
        furi_hal_vibro_on(false);
    }

    // Update upcoming packets
    update_upcoming_packets(state);
}

// Main app entry
int32_t system_defender_app(void* p) {
    UNUSED(p);

    // Setup event queue
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Setup game state
    GameState* game_state = malloc(sizeof(GameState));
    game_init(game_state);

    // Setup GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, game_state);
    view_port_input_callback_set(view_port, input_callback, event_queue);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Main loop
    InputEvent event;
    bool running = true;
    
    while(running) {
        if(furi_message_queue_get(event_queue, &event, 100) == FuriStatusOk) {
            if(event.type == InputTypePress) {
                // Handle input based on current state
                if(game_state->current_state == GAME_STATE_TITLE) {
                    // Title screen inputs
                    if(event.key == InputKeyOk) {
                        game_state->current_state = GAME_STATE_MENU; // Go to menu instead of play
                    } else if(event.key == InputKeyBack) {
                        running = false; // Quit from title screen
                    }
                } else if(game_state->current_state == GAME_STATE_MENU) {
                    // Menu screen inputs
                    if(event.key == InputKeyUp && game_state->menu_selection > 0) {
                        game_state->menu_selection--;
                    } else if(event.key == InputKeyDown && game_state->menu_selection < 2) {
                        game_state->menu_selection++;
                    } else if(event.key == InputKeyOk) {
                        if(game_state->menu_selection == 0) {
                            game_state->current_state = GAME_STATE_PLAY;
                            reset_game_state(game_state); // Reset game when starting
                        } else if(game_state->menu_selection == 1) {
                            game_state->current_state = GAME_STATE_HELP;
                            game_state->help_page = 0; // Start at first help page
                        } else if(game_state->menu_selection == 2) {
                            running = false; // Quit
                        }
                    } else if(event.key == InputKeyBack) {
                        game_state->current_state = GAME_STATE_TITLE;
                    }
                } else if(game_state->current_state == GAME_STATE_HELP) {
                    // Help screen inputs
                    if(event.key == InputKeyOk) {
                        if(game_state->help_page == 0) {
                            game_state->help_page = 1; // Go to next page
                        } else {
                            game_state->current_state = GAME_STATE_MENU; // Return to menu
                        }
                    } else if(event.key == InputKeyBack) {
                        game_state->current_state = GAME_STATE_MENU;
                    }
                } else if(game_state->current_state == GAME_STATE_PLAY) {
                    // Game screen inputs
                    if(game_state->game_over) {
                        if(event.key == InputKeyBack) {
                            game_state->current_state = GAME_STATE_TITLE;
                            reset_game_state(game_state);
                        }
                    } else {
                        switch(event.key) {
                        case InputKeyUp:
                            game_state->player_pos = (game_state->player_pos == 0 || game_state->player_pos == 1) ? game_state->player_pos : game_state->player_pos - 2;
                            break;
                        case InputKeyDown:
                            game_state->player_pos = (game_state->player_pos == 2 || game_state->player_pos == 3) ? game_state->player_pos : game_state->player_pos + 2;
                            break;
                        case InputKeyLeft:
                            game_state->player_pos = (game_state->player_pos == 0 || game_state->player_pos == 2) ? game_state->player_pos : game_state->player_pos - 1;
                            break;
                        case InputKeyRight:
                            game_state->player_pos = (game_state->player_pos == 1 || game_state->player_pos == 3) ? game_state->player_pos : game_state->player_pos + 1;
                            break;
                        case InputKeyOk:
                            if(game_state->hacking[game_state->player_pos] && !game_state->patching) {
                                game_state->patching = true;
                                game_state->patch_start = furi_get_tick();
                            } else if(game_state->packets[game_state->player_pos] > 0) {
                                game_state->packets[game_state->player_pos]--; // Correctly remove packet
                                game_state->score += 10; // Increment score by 10 for each packet delivered

                                // Start packet animation
                                game_state->packet_animation = true;
                                game_state->anim_start = furi_get_tick();
                                game_state->anim_from = game_state->player_pos;
                                
                                furi_hal_vibro_on(true); // Vibrate for packet accept
                                furi_delay_ms(100);
                                furi_hal_vibro_on(false);
                            }
                            break;
                        case InputKeyBack:
                            game_state->current_state = GAME_STATE_TITLE; // Return to title
                            break;
                        default:
                            break;
                        }
                    }
                }
            }
        }

        // Only update game logic if we're in play mode
        if(game_state->current_state == GAME_STATE_PLAY) {
            update_game(game_state);
        }
        
        view_port_update(view_port);
    }

    // Cleanup
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    free(game_state);

    return 0;
}