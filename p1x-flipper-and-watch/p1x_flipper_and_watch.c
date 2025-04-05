#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>

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
} GameState;

// Game over reasons
#define GAME_OVER_HACK 1
#define GAME_OVER_DDOS 2

// Initialize game state
static void game_init(GameState* state) {
    state->player_pos = 0;
    state->patching = false;
    state->game_over = false;
    state->game_over_reason = 0;
    state->packet_animation = false;
    state->score = 0;
    state->last_packet_spawn_time = furi_get_tick();
    for(int i = 0; i < 4; i++) {
        state->hacking[i] = false;
        state->hack_start[i] = 0;
        state->warn_start[i] = 0;
        state->packets[i] = 0;
        for(int j = 0; j < MAX_UPCOMING_PACKETS; j++) {
            state->upcoming_packets[i][j] = rand() % 2; // Randomly initialize as 0 (normal) or 1 (hacking attempt)
        }
    }
}

// Draw callback for GUI
static void draw_callback(Canvas* canvas, void* ctx) {
    GameState* state = (GameState*)ctx;

    canvas_clear(canvas);

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
        canvas_draw_str(canvas, 15, 45, score_text);

        canvas_draw_str(canvas, 15, 55, "Press BACK to restart");


        return;
    }

    // Draw server in center
    canvas_draw_rframe(canvas, SERVER_POSITION.x - 8, SERVER_POSITION.y - 8, 16, 16, 2);
    canvas_draw_str(canvas, SERVER_POSITION.x - 6, SERVER_POSITION.y + 2, "SRV");

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

        // Draw system (box)
        canvas_draw_frame(canvas, x - 6, y - 6, 12, 12);

        // Hacking warning or active hack
        uint32_t now = furi_get_tick();
        if(state->warn_start[i] && !state->hacking[i]) {
            if((now - state->warn_start[i]) / 500 % 2 == 0) { // Flash every 0.5s
                canvas_draw_str(canvas, x - 4, y - 8, "!");
            }
        } else if(state->hacking[i]) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, x - 4, y - 8, "!!!");
            canvas_set_font(canvas, FontPrimary);
        }

        // Draw packets (dots)
        for(int p = 0; p < state->packets[i] && p < 3; p++) {
            canvas_draw_disc(canvas, (i % 2 == 0) ? x + 10 + p*3 : x - 10 - p*3, y, 1);
        }

        // Adjust upcoming packets display to left and right sides
        for(int j = 0; j < MAX_UPCOMING_PACKETS; j++) {
            int packet_type = state->upcoming_packets[i][j];
            int packet_x = (i % 2 == 0) ? x - 10 - j * 5 : x + 10 + j * 5; // Left for even-indexed systems, right for odd-indexed systems

            if(packet_type == 0) {
                canvas_draw_disc(canvas, packet_x, y, 1); // Normal packet
            } else {
                canvas_draw_circle(canvas, packet_x, y, 2); // Hacking attempt (larger circle)
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
        canvas_draw_disc(canvas, px, py, 2);
    }

    // Draw player
    int px = PLAYER_POSITIONS[state->player_pos].x;
    int py = PLAYER_POSITIONS[state->player_pos].y;
    if(state->patching) {
        canvas_draw_circle(canvas, px, py, 4); // Circle when patching
        canvas_draw_line(canvas, px - 3, py - 3, px + 3, py + 3); // "+" sign when patching
        canvas_draw_line(canvas, px - 3, py + 3, px + 3, py - 3);
    } else {
        canvas_draw_disc(canvas, px, py, 3); // Player dot
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

// Update upcoming packets logic to show real-time network for all computers
static void update_upcoming_packets(GameState* state) {
    for(int i = 0; i < 4; i++) {
        // Shift all packets one step closer to the computer
        for(int j = 0; j < MAX_UPCOMING_PACKETS - 1; j++) {
            state->upcoming_packets[i][j] = state->upcoming_packets[i][j + 1];
        }
        // Add a new random packet at the end of the queue
        state->upcoming_packets[i][MAX_UPCOMING_PACKETS - 1] = rand() % 2;

        // If the first packet in the queue reaches the computer, add it to the system
        if(state->upcoming_packets[i][0] == 1 && state->packets[i] < 3) {
            state->packets[i]++;
            state->upcoming_packets[i][0] = 0; // Clear the slot
        }
    }
}

// Game logic update
static void update_game(GameState* state) {
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

    // Randomly spawn warnings or packets - adjusted probabilities
    if(rand() % 100 < 1) { // Reduced from 5% to 1% chance for hack warnings
        int target = rand() % 4;
        if(!state->hacking[target] && !state->warn_start[target]) {
            state->warn_start[target] = now;
            furi_hal_vibro_on(true); // Short vibrate for warning
            furi_delay_ms(100);
            furi_hal_vibro_on(false);
        }
    }
    
    // Increase packet spawning slightly to compensate for fewer hacks
    // Ensure PACKET_SPAWN_INTERVAL is properly enforced
    if(now - state->last_packet_spawn_time >= PACKET_SPAWN_INTERVAL) {
        int target = rand() % 4;
        if(state->packets[target] < 3) {
            state->packets[target]++;
            // Shift upcoming packets queue
            for(int j = 0; j < MAX_UPCOMING_PACKETS - 1; j++) {
                state->upcoming_packets[target][j] = state->upcoming_packets[target][j + 1];
            }
            // Add new packet type to the end of the queue
            state->upcoming_packets[target][MAX_UPCOMING_PACKETS - 1] = rand() % 2;
        }
        state->last_packet_spawn_time = now; // Update last spawn time
    } else {
        // Prevent packet spawning if interval has not passed
        return;
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

    // Call update_upcoming_packets in update_game
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
                if(game_state->game_over) {
                    if(event.key == InputKeyBack) {
                        // Restart game instead of exiting
                        game_init(game_state);
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
                        running = false; // Exit game
                        break;
                    default:
                        break;
                    }
                }
            }
        }

        update_game(game_state);
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