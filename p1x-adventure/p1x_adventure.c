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
#define WEAPON_LENGTH 12
#define WEAPON_DURATION 10  // frames that the weapon stays active

// Direction constants
#define DIR_UP 0
#define DIR_RIGHT 1
#define DIR_DOWN 2
#define DIR_LEFT 3

typedef struct {
    int32_t x, y;
    int32_t health;
    uint8_t direction;     // Last movement direction
    bool weapon_active;    // Is weapon currently active
    uint8_t weapon_timer;  // How long the weapon remains active
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
    .player = {
        .x = 10, 
        .y = 30, 
        .health = 100, 
        .direction = DIR_RIGHT,
        .weapon_active = false,
        .weapon_timer = 0
    },
    .score = 0,
};

// Calculate weapon end point based on player position and direction
static void get_weapon_end_point(int* end_x, int* end_y) {
    *end_x = game_state.player.x + 4; // Start from center of player
    *end_y = game_state.player.y + 4;
    
    switch(game_state.player.direction) {
        case DIR_UP:
            *end_y -= WEAPON_LENGTH;
            break;
        case DIR_RIGHT:
            *end_x += WEAPON_LENGTH;
            break;
        case DIR_DOWN:
            *end_y += WEAPON_LENGTH;
            break;
        case DIR_LEFT:
            *end_x -= WEAPON_LENGTH;
            break;
    }
}

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
    
    // Draw weapon if active 
    if(game_state.player.weapon_active) {
        int weapon_end_x, weapon_end_y;
        get_weapon_end_point(&weapon_end_x, &weapon_end_y);
        
        canvas_draw_line(
            canvas,
            game_state.player.x + 4,  // Center of player
            game_state.player.y + 4,
            weapon_end_x,
            weapon_end_y);
    }
    
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

// Check if a point is within a circle
static bool point_in_circle(int px, int py, int cx, int cy, int radius) {
    int dx = px - cx;
    int dy = py - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
}

// Check if a line segment intersects a circle
static bool line_circle_collision(
    int x1, int y1, int x2, int y2,   // Line segment
    int cx, int cy, int radius        // Circle
) {
    // Vector from point 1 to point 2
    int dx = x2 - x1;
    int dy = y2 - y1;
    
    // Length of line squared
    int length_squared = dx * dx + dy * dy;
    
    if (length_squared == 0) {
        // Line is a point - check if point is in circle
        return point_in_circle(x1, y1, cx, cy, radius);
    }
    
    // Calculate projection of circle center onto line
    float t = ((cx - x1) * dx + (cy - y1) * dy) / (float)length_squared;
    
    // Clamp t to [0,1] for line segment
    if (t < 0) t = 0;
    if (t > 1) t = 1;
    
    // Find nearest point on line to circle
    int nearest_x = x1 + t * dx;
    int nearest_y = y1 + t * dy;
    
    // Check if this point is within the circle
    return point_in_circle(nearest_x, nearest_y, cx, cy, radius);
}

// Check for weapon collisions with enemies
static void check_weapon_collisions() {
    if (!game_state.player.weapon_active) return;
    
    int weapon_end_x, weapon_end_y;
    get_weapon_end_point(&weapon_end_x, &weapon_end_y);
    
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (game_state.enemies[i].active) {
            // Check if weapon collides with this enemy
            if (line_circle_collision(
                game_state.player.x + 4, game_state.player.y + 4,  // Weapon start (player center)
                weapon_end_x, weapon_end_y,                        // Weapon end
                game_state.enemies[i].x + 4, game_state.enemies[i].y + 4, 4  // Enemy circle
            )) {
                // Kill enemy
                game_state.enemies[i].active = false;
                game_state.score += 10; // More points for weapon kill
            }
        }
    }
}

static void check_collisions() {
    const int collision_threshold = 8;
    
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game_state.enemies[i].active) {
            int dx = abs(game_state.player.x - game_state.enemies[i].x);
            int dy = abs(game_state.player.y - game_state.enemies[i].y);
            
            if(dx < collision_threshold && dy < collision_threshold) {
                game_state.player.health -= 10;
                // Don't kill enemy on body collision, player must use weapon
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
            if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                switch(event.key) {
                case InputKeyLeft:
                    game_state.player.x -= 2;
                    game_state.player.direction = DIR_LEFT;
                    if(game_state.player.x < 0) game_state.player.x = 0;
                    break;
                case InputKeyRight:
                    game_state.player.x += 2;
                    game_state.player.direction = DIR_RIGHT;
                    if(game_state.player.x > SCREEN_WIDTH - 8) 
                        game_state.player.x = SCREEN_WIDTH - 8;
                    break;
                case InputKeyUp:
                    game_state.player.y -= 2;
                    game_state.player.direction = DIR_UP;
                    if(game_state.player.y < 15) game_state.player.y = 15;
                    break;
                case InputKeyDown:
                    game_state.player.y += 2;
                    game_state.player.direction = DIR_DOWN;
                    if(game_state.player.y > SCREEN_HEIGHT - 8) 
                        game_state.player.y = SCREEN_HEIGHT - 8;
                    break;
                case InputKeyOk:
                    // Activate weapon on OK button press
                    game_state.player.weapon_active = true;
                    game_state.player.weapon_timer = WEAPON_DURATION;
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
            // Update weapon timer
            if(game_state.player.weapon_active) {
                if(--game_state.player.weapon_timer <= 0) {
                    game_state.player.weapon_active = false;
                }
            }
            
            move_enemies();
            check_collisions();
            check_weapon_collisions();
            
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
