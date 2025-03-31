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
// Change from constants to initial values
#define INITIAL_WEAPON_LENGTH 8
#define INITIAL_WEAPON_DURATION 4  // frames that the weapon stays active
#define CASTLE_WIDTH 15    // Width of castle entrance
#define CASTLE_HEIGHT 30   // Height of castle entrance
#define PLAYER_SPEED 4     // Player moves 4 pixels per keypress

// Game states
typedef enum {
    GameStateTitle,
    GameStateGameplay,
    GameStateGameOver,
    GameStateWin,
    GameStateUpgrade,  // New state for upgrade selection
    GameStateBossFight, // New game state
} GameStateEnum;

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
    int32_t hitpoints; // New field for boss or tougher enemies
} Enemy;

typedef struct {
    GameStateEnum state;   // Current game state
    Player player;
    Enemy enemies[MAX_ENEMIES];
    int32_t score;
    uint8_t selected_upgrade; // Track which upgrade is currently selected
    bool wave_completed;      // Flag to track if a wave was just completed
    int32_t weapon_length;    // Variable for weapon length that can be upgraded
    int32_t weapon_duration;  // Variable for weapon duration that can be upgraded
    Enemy boss; // New boss enemy
} GameState;

static GameState game_state = {
    .state = GameStateTitle,
    .player = {
        .x = 10, 
        .y = 30, 
        .health = 100, 
        .direction = DIR_RIGHT,
        .weapon_active = false,
        .weapon_timer = 0
    },
    .score = 0,
    .selected_upgrade = 0,
    .wave_completed = false,
    .weapon_length = INITIAL_WEAPON_LENGTH,
    .weapon_duration = INITIAL_WEAPON_DURATION
};

// Calculate weapon end point based on player position and direction
static void get_weapon_end_point(int* end_x, int* end_y) {
    *end_x = game_state.player.x + 4; // Start from center of player
    *end_y = game_state.player.y + 4;
    
    switch(game_state.player.direction) {
        case DIR_UP:
            *end_y -= game_state.weapon_length;  // Use the variable instead of constant
            break;
        case DIR_RIGHT:
            *end_x += game_state.weapon_length;  // Use the variable instead of constant
            break;
        case DIR_DOWN:
            *end_y += game_state.weapon_length;  // Use the variable instead of constant
            break;
        case DIR_LEFT:
            *end_x -= game_state.weapon_length;  // Use the variable instead of constant
            break;
    }
}

// Draw the castle entrance on the right side of the screen
static void draw_castle_entrance(Canvas* canvas) {
    int castle_x = SCREEN_WIDTH - CASTLE_WIDTH;
    int castle_y = (SCREEN_HEIGHT - CASTLE_HEIGHT) / 2;
    
    // Draw castle walls
    canvas_draw_frame(canvas, castle_x, castle_y, CASTLE_WIDTH, CASTLE_HEIGHT);
    
    // Draw towers
    canvas_draw_box(canvas, castle_x - 2, castle_y - 2, 4, 4); // Top tower
    canvas_draw_box(canvas, castle_x - 2, castle_y + CASTLE_HEIGHT - 2, 4, 4); // Bottom tower
    
    // Draw entrance (doorway)
    canvas_draw_frame(canvas, 
                    castle_x + 3, 
                    castle_y + (CASTLE_HEIGHT/2) - 8, 
                    CASTLE_WIDTH - 6, 
                    16);
}

// Check if player has reached the castle entrance
static bool check_castle_reached() {
    int castle_x = SCREEN_WIDTH - CASTLE_WIDTH;
    int castle_y = (SCREEN_HEIGHT - CASTLE_HEIGHT) / 2;
    int castle_door_y = castle_y + (CASTLE_HEIGHT/2) - 8;
    
    // Player needs to be at the castle entrance (doorway)
    return (game_state.player.x >= castle_x - 2 && 
            game_state.player.y >= castle_door_y - 4 && 
            game_state.player.y <= castle_door_y + 20);
}

// Draw title screen
static void draw_title_screen(Canvas* canvas) {
    canvas_clear(canvas);
    
    // Draw game title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 15, AlignCenter, AlignCenter, "P1X Adventure");
    
    // Draw story
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "Rescue the princess");
    canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "from the castle!");
    
    // Draw instructions
    canvas_draw_str_aligned(canvas, 64, 52, AlignCenter, AlignCenter, "Press OK to start");
}

// Draw game over screen
static void draw_game_over_screen(Canvas* canvas) {
    canvas_clear(canvas);
    
    // Draw game over message
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "GAME OVER");
    
    // Draw final score
    char score_str[20];
    snprintf(score_str, sizeof(score_str), "Score: %ld", game_state.score);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, score_str);
    
    // Draw restart instructions or cooldown message
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "Press Back to restart");
}

// Draw win screen
static void draw_win_screen(Canvas* canvas) {
    canvas_clear(canvas);
    
    // Draw win message
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 20, AlignCenter, AlignCenter, "YOU WIN!");
    
    // Draw final score
    char score_str[20];
    snprintf(score_str, sizeof(score_str), "Score: %ld", game_state.score);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 64, 35, AlignCenter, AlignCenter, score_str);
    
    // Draw restart instructions
    canvas_draw_str_aligned(canvas, 64, 50, AlignCenter, AlignCenter, "Press Back to play again");
}

// Draw upgrade screen
static void draw_upgrade_screen(Canvas* canvas) {
    canvas_clear(canvas);
    
    // Draw title
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, 64, 12, AlignCenter, AlignCenter, "LEVEL UP!");
    
    // Draw upgrade options
    canvas_set_font(canvas, FontSecondary);
    
    // Option 1: Weapon Duration
    if(game_state.selected_upgrade == 0) {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "> Weapon Upgrade <");
    } else {
        canvas_draw_str_aligned(canvas, 64, 30, AlignCenter, AlignCenter, "  Weapon Upgrade   ");
    }
    
    // Option 2: Weapon Length
    if(game_state.selected_upgrade == 1) {
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "> Health Restore <");
    } else {
        canvas_draw_str_aligned(canvas, 64, 40, AlignCenter, AlignCenter, "  Health Restore  ");
    }
    
    // Instructions
    canvas_draw_str_aligned(canvas, 64, 55, AlignCenter, AlignCenter, "Select your upgrade!");
}

// Draw gameplay screen
static void draw_gameplay_screen(Canvas* canvas) {
    // Draw castle entrance first (background)
    draw_castle_entrance(canvas);
    
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
        
        // Clip weapon endpoint to screen boundaries to prevent overdraw
        if(weapon_end_x < 0) weapon_end_x = 0;
        if(weapon_end_x >= SCREEN_WIDTH) weapon_end_x = SCREEN_WIDTH - 1;
        if(weapon_end_y < 0) weapon_end_y = 0;
        if(weapon_end_y >= SCREEN_HEIGHT) weapon_end_y = SCREEN_HEIGHT - 1;
        
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

// Draw boss fight screen
static void draw_boss_fight_screen(Canvas* canvas) {
    // Draw castle entrance first (background)
    draw_castle_entrance(canvas);
    
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
        
        // Clip weapon endpoint to screen boundaries to prevent overdraw
        if(weapon_end_x < 0) weapon_end_x = 0;
        if(weapon_end_x >= SCREEN_WIDTH) weapon_end_x = SCREEN_WIDTH - 1;
        if(weapon_end_y < 0) weapon_end_y = 0;
        if(weapon_end_y >= SCREEN_HEIGHT) weapon_end_y = SCREEN_HEIGHT - 1;
        
        canvas_draw_line(
            canvas,
            game_state.player.x + 4,  // Center of player
            game_state.player.y + 4,
            weapon_end_x,
            weapon_end_y);
    }
    
    // Draw boss (16px circle)
    if(game_state.boss.active) {
        canvas_draw_circle(canvas, game_state.boss.x + 8, game_state.boss.y + 8, 6);
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

// Screen is 128x64 px
static void app_draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);

    canvas_clear(canvas);
    
    // Draw appropriate screen based on current game state
    switch(game_state.state) {
        case GameStateTitle:
            draw_title_screen(canvas);
            break;
        case GameStateGameplay:
            draw_gameplay_screen(canvas);
            break;
        case GameStateGameOver:
            draw_game_over_screen(canvas);
            break;
        case GameStateWin:
            draw_win_screen(canvas);
            break;
        case GameStateUpgrade:
            draw_upgrade_screen(canvas);
            break;
        case GameStateBossFight:
            draw_boss_fight_screen(canvas);
            break;
    }
}

static void app_input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);

    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

// Reset game to initial state
static void reset_game() {
    game_state.player.x = 10;
    game_state.player.y = 30;
    game_state.player.health = 100;
    game_state.player.direction = DIR_RIGHT;
    game_state.player.weapon_active = false;
    game_state.player.weapon_timer = 0;
    game_state.score = 0;
    game_state.selected_upgrade = 0;
    game_state.wave_completed = false;
    game_state.weapon_length = INITIAL_WEAPON_LENGTH;    // Reset weapon length
    game_state.weapon_duration = INITIAL_WEAPON_DURATION; // Reset weapon duration
}

static void init_enemies() {
    for(int i = 0; i < MAX_ENEMIES; i++) {
        game_state.enemies[i].x = 30 + (i * 20) % 100;
        game_state.enemies[i].y = 20 + (i * 15) % 30;
        game_state.enemies[i].active = true;
        game_state.enemies[i].hitpoints = 2; // new
    }
}

static void init_boss_fight() {
    game_state.player.x = 20;
    game_state.player.y = 30;
    game_state.boss.x = 90;
    game_state.boss.y = 30;
    game_state.boss.active = true;
    game_state.boss.hitpoints = 25; // was 3
    game_state.state = GameStateBossFight;
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
                
                // Short vibration for hit
                furi_hal_vibro_on(true);
                furi_delay_ms(20);
                furi_hal_vibro_on(false);
            }
        }
    }
}

static void check_boss_weapon_collision() {
    if(!game_state.player.weapon_active || !game_state.boss.active) return;
    int weapon_end_x, weapon_end_y;
    get_weapon_end_point(&weapon_end_x, &weapon_end_y);
    if(line_circle_collision(
        game_state.player.x + 4, game_state.player.y + 4,
        weapon_end_x, weapon_end_y,
        game_state.boss.x + 4, game_state.boss.y + 4, 8
    )) {
        game_state.boss.hitpoints--;
        if(game_state.boss.hitpoints <= 0) {
            game_state.boss.active = false;
            game_state.score += 50; // Bonus for defeating boss
            game_state.state = GameStateWin; // Or another transition
        }
    }
}

static void check_boss_player_collision() {
    const int collision_threshold = 8;
    int dx = abs(game_state.player.x - game_state.boss.x);
    int dy = abs(game_state.player.y - game_state.boss.y);
    if(dx < collision_threshold && dy < collision_threshold) {
        // Boss deals more damage, for example 10
        game_state.player.health -= 10;
        // Optional vibration effect
        furi_hal_vibro_on(true);
        furi_delay_ms(100);
        furi_hal_vibro_on(false);
        if(game_state.player.health <= 0) {
            game_state.state = GameStateGameOver;
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
                
                // Medium vibration when player gets hit
                furi_hal_vibro_on(true);
                furi_delay_ms(100);
                furi_hal_vibro_on(false);
                
                // Don't kill enemy on body collision, player must use weapon
                game_state.enemies[i].hitpoints--;
                if(game_state.enemies[i].hitpoints <= 0) {
                    game_state.enemies[i].active = false;
                }
            }
        }
    }
    
    // Check if player died
    if (game_state.player.health <= 0) {
        game_state.state = GameStateGameOver;
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

static void move_boss() {
    static uint8_t slow_counter = 0;
    slow_counter++;
    if(slow_counter % 2 != 0) return;
    if(!game_state.boss.active) return;
    // Simple chase logic
    if(game_state.boss.x < game_state.player.x) game_state.boss.x++;
    else if(game_state.boss.x > game_state.player.x) game_state.boss.x--;
    if(game_state.boss.y < game_state.player.y) game_state.boss.y++;
    else if(game_state.boss.y > game_state.player.y) game_state.boss.y--;
}

static bool all_enemies_defeated() {
    for(int i = 0; i < MAX_ENEMIES; i++) {
        if(game_state.enemies[i].active) return false;
    }
    return true;
}

// Handle the title screen
static void handle_title_screen(InputEvent* event) {
    if(event->type == InputTypePress && event->key == InputKeyOk) {
        reset_game();
        init_enemies();
        game_state.state = GameStateGameplay;
    }
}

// Handle the game over screen
static void handle_game_over_screen(InputEvent* event) {
    // Only handle Back button press
    if(event->type == InputTypePress && event->key == InputKeyBack) {
        game_state.state = GameStateTitle;
    }
}

// Handle the win screen
static void handle_win_screen(InputEvent* event) {
    if(event->type == InputTypePress && event->key == InputKeyBack) {
        game_state.state = GameStateTitle;
    }
}

// Handle the upgrade screen
static void handle_upgrade_screen(InputEvent* event) {
    if(event->type == InputTypePress) {
        switch(event->key) {
            case InputKeyUp:
            case InputKeyDown:
                // Toggle between 0 and 1
                game_state.selected_upgrade = 1 - game_state.selected_upgrade;
                break;
            case InputKeyOk:
                // Apply the selected upgrade
                if(game_state.selected_upgrade == 0) {
                    // Increase weapon duration and length
                    game_state.weapon_duration += 1;
                    game_state.weapon_length += 2;
                } else {
                    // Increase player health
                    game_state.player.health += 25;
                    if(game_state.player.health > 100) {
                        game_state.player.health = 100; // Cap health at 100
                    }
                }
                
                // Return to gameplay with new enemies
                init_enemies();
                game_state.state = GameStateGameplay;
                game_state.wave_completed = false;
                
                // Vibrate to confirm selection
                furi_hal_vibro_on(true);
                furi_delay_ms(50);
                furi_hal_vibro_on(false);
                break;
            default:
                break;
        }
    }
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
    
    InputEvent event;
    uint32_t last_tick = furi_get_tick();

    bool running = true;
    while(running) {
        // Handle input based on current game state
        if(furi_message_queue_get(event_queue, &event, 10) == FuriStatusOk) {
            if(event.type == InputTypePress) {
                // Global back button handling
                if(event.key == InputKeyBack) {
                    // Always allow exit with back button
                    if(game_state.state == GameStateTitle) {
                        running = false;
                    } else {
                        // From gameplay or game over, go back to title
                        game_state.state = GameStateTitle;
                    }
                } 
                // State-specific input handling
                else {
                    switch(game_state.state) {
                        case GameStateTitle:
                            handle_title_screen(&event);
                            break;
                        case GameStateGameOver:
                            handle_game_over_screen(&event);
                            break;
                        case GameStateWin:
                            handle_win_screen(&event);
                            break;
                        case GameStateUpgrade:
                            handle_upgrade_screen(&event);
                            break;
                        case GameStateGameplay:
                            // Handle gameplay inputs
                            if((event.type == InputTypePress) || (event.type == InputTypeRepeat)) {
                                switch(event.key) {
                                case InputKeyLeft:
                                    game_state.player.x -= PLAYER_SPEED;
                                    game_state.player.direction = DIR_LEFT;
                                    if(game_state.player.x < 0) game_state.player.x = 0;
                                    break;
                                case InputKeyRight:
                                    game_state.player.x += PLAYER_SPEED;
                                    game_state.player.direction = DIR_RIGHT;
                                    if(game_state.player.x > SCREEN_WIDTH - 8) 
                                        game_state.player.x = SCREEN_WIDTH - 8;
                                    break;
                                case InputKeyUp:
                                    game_state.player.y -= PLAYER_SPEED;
                                    game_state.player.direction = DIR_UP;
                                    if(game_state.player.y < 15) game_state.player.y = 15;
                                    break;
                                case InputKeyDown:
                                    game_state.player.y += PLAYER_SPEED;
                                    game_state.player.direction = DIR_DOWN;
                                    if(game_state.player.y > SCREEN_HEIGHT - 8) 
                                        game_state.player.y = SCREEN_HEIGHT - 8;
                                    break;
                                case InputKeyOk:
                                    // Activate weapon on OK button press
                                    game_state.player.weapon_active = true;
                                    game_state.player.weapon_timer = game_state.weapon_duration;  // Use the variable instead of constant
                                    break;
                                default:
                                    break;
                                }
                            }
                            break;
                        case GameStateBossFight:
                            if((event.type == InputTypePress) || (event.type == InputTypeRepeat)) {
                                switch(event.key) {
                                    case InputKeyLeft:
                                        game_state.player.x -= PLAYER_SPEED;
                                        game_state.player.direction = DIR_LEFT;
                                        if(game_state.player.x < 0) game_state.player.x = 0;
                                        break;
                                    case InputKeyRight:
                                        game_state.player.x += PLAYER_SPEED;
                                        game_state.player.direction = DIR_RIGHT;
                                        if(game_state.player.x > SCREEN_WIDTH - 8) 
                                            game_state.player.x = SCREEN_WIDTH - 8;
                                        break;
                                    case InputKeyUp:
                                        game_state.player.y -= PLAYER_SPEED;
                                        game_state.player.direction = DIR_UP;
                                        if(game_state.player.y < 15) game_state.player.y = 15;
                                        break;
                                    case InputKeyDown:
                                        game_state.player.y += PLAYER_SPEED;
                                        game_state.player.direction = DIR_DOWN;
                                        if(game_state.player.y > SCREEN_HEIGHT - 8) 
                                            game_state.player.y = SCREEN_HEIGHT - 8;
                                        break;
                                    case InputKeyOk:
                                        // Activate weapon on OK button press
                                        game_state.player.weapon_active = true;
                                        game_state.player.weapon_timer = game_state.weapon_duration;
                                        break;
                                    case InputKeyBack:
                                        // Handle back logic if any
                                        break;
                                    default:
                                        break;
                                }
                            }
                            break;
                    }
                }
            }
        }
        
        // Update game logic only during gameplay
        uint32_t current_tick = furi_get_tick();
        
        if(game_state.state == GameStateGameplay && current_tick - last_tick > 100) {
            // Update weapon timer
            if(game_state.player.weapon_active) {
                if(--game_state.player.weapon_timer <= 0) {
                    game_state.player.weapon_active = false;
                }
            }
            
            move_enemies();
            check_collisions();
            check_weapon_collisions();
            
            // Check if player reached the castle
            if(check_castle_reached()) {
                init_boss_fight();
            }
            
            // Check if level is complete
            if(all_enemies_defeated() && !game_state.wave_completed) {
                game_state.wave_completed = true;
                game_state.score += 20; // Bonus for completing a wave
                game_state.state = GameStateUpgrade; // Go to upgrade screen
            }
            
            last_tick = current_tick;
        } else if(game_state.state == GameStateBossFight) {
            move_boss();
            check_boss_weapon_collision();
            check_boss_player_collision(); // New call
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
