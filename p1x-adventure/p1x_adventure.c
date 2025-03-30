/**
 * @file p1x_adventure.c
 * @brief Simple adventure game with hero and enemies.
 */
#include <furi.h>
#include <furi_hal.h>

#include <gui/gui.h>
#include <input/input.h>

#define MAX_ENEMIES 4
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64

typedef struct {
    int32_t x, y;
    int32_t health;
} Player;

typedef struct {
    int32_t x, y;
    bool active;
} Enemy;

typedef struct {
    Player player;
    Enemy enemies[MAX_ENEMIES];
    int32_t score;
} GameState;

static GameState game_state = {
    .player = {.x = 10, .y = 30, .health = 100},
    .score = 0,
};

// Screen is 128x64 px
static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    canvas_clear(canvas);
    
    // Draw player (8x8 square)
    canvas_draw_frame(
        canvas,
        game_state.player.x,
        game_state.player.y,
        8,
        8);
    
    // Draw enemies (8px circles)
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game_state.enemies[i].active) {
            canvas_draw_circle(
                canvas,
                game_state.enemies[i].x + 4,
                game_state.enemies[i].y + 4,
                4);
        }
    }
    
    // Draw health and score
    canvas_set_font(canvas, FontSecondary);
    char health_str[12];
    snprintf(health_str, sizeof(health_str), "HP: %ld", game_state.player.health);
    canvas_draw_str(canvas, 5, 10, health_str);
    
    char score_str[12];
    snprintf(score_str, sizeof(score_str), "Score: %ld", game_state.score);
    canvas_draw_str(canvas, 70, 10, score_str);
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);

    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

static void init_enemies() {
    for(int i = 0; i < MAX_ENEMIES; i++) {
        game_state.enemies[i].x = 30 + (i * 20) % 100;
        game_state.enemies[i].y = 20 + (i * 15) % 30;
        game_state.enemies[i].active = true;
    }
}

static void check_collisions() {
    const int collision_threshold = 10;
    
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game_state.enemies[i].active) {
            int dx = abs(game_state.player.x - game_state.enemies[i].x);
            int dy = abs(game_state.player.y - game_state.enemies[i].y);
            
            if(dx < collision_threshold && dy < collision_threshold) {
                game_state.player.health -= 10;
                game_state.enemies[i].active = false;
                game_state.score += 5;
            }
        }
    }
}

static void move_enemies() {
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game_state.enemies[i].active) {
            // Simple movement toward player
            if(game_state.enemies[i].x < game_state.player.x) {
                game_state.enemies[i].x++;
            } else if(game_state.enemies[i].x > game_state.player.x) {
                game_state.enemies[i].x--;
            }
            
            if(game_state.enemies[i].y < game_state.player.y) {
                game_state.enemies[i].y++;
            } else if(game_state.enemies[i].y > game_state.player.y) {
                game_state.enemies[i].y--;
            }
        }
    }
}

static bool all_enemies_defeated() {
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game_state.enemies[i].active) return false;
    }
    return true;
}

int32_t p1x_adventure_main(void* p) {
    UNUSED(p);
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));

    // Configure view port
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, app_draw_callback, NULL);
    view_port_input_callback_set(view_port, app_input_callback, event_queue);

    // Register view port in GUI
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Initialize game (spawn enemies)
    init_enemies();
    
    InputEvent event;
    uint32_t last_tick = furi_get_tick();

    bool running = true;
    while(running && game_state.player.health > 0) {
        if(furi_message_queue_get(event_queue, &event, 10) == FuriStatusOk) {
            if((event.type == InputTypePress) || (event.type == InputTypeRepeat)) {
                switch(event.key) {
                case InputKeyLeft:
                    game_state.player.x -= 2;
                    if(game_state.player.x < 0) game_state.player.x = 0;
                    break;
                case InputKeyRight:
                    game_state.player.x += 2;
                    if(game_state.player.x > SCREEN_WIDTH - 16) 
                        game_state.player.x = SCREEN_WIDTH - 16;
                    break;
                case InputKeyUp:
                    game_state.player.y -= 2;
                    if(game_state.player.y < 15) game_state.player.y = 15;
                    break;
                case InputKeyDown:
                    game_state.player.y += 2;
                    if(game_state.player.y > SCREEN_HEIGHT - 16) 
                        game_state.player.y = SCREEN_HEIGHT - 16;
                    break;
                case InputKeyBack:
                    running = false;
                    break;
                default:
                    break;
                }
            }
        }
        
        // Update game state every 100ms
        uint32_t current_tick = furi_get_tick();
        if(current_tick - last_tick > 100) {
            move_enemies();
            check_collisions();
            
            // Check if level is complete
            if(all_enemies_defeated()) {
                init_enemies();
                game_state.score += 20; // Bonus for completing a wave
            }
            
            last_tick = current_tick;
        }
        
        view_port_update(view_port);
    }
    
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);

    furi_record_close(RECORD_GUI);

    return 0;
}
