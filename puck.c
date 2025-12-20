/**
 * Puck - A Pac-Man style maze chase game for Flipper Zero
 * 
 * Control a pie-shaped character (Puck girl) through a maze, eating dots while
 * avoiding three ghosts. Power pills make ghosts temporarily eatable.
 */

#include <furi.h> // Flipper Universal Registry Implementation = Core OS functionality
#include <gui/gui.h> // GUI system
#include <input/input.h> // Input handling (buttons)
#include <stdlib.h> // Standard library functions
#include <string.h> // memcpy/memset
#include <gui/elements.h> // to access button drawing functions
#include "puck_icons.h" // Custom icon definitions, header file is auto-magically generated

// Maze configuration - defines the logical grid structure
#define MAZE_WIDTH 15   // Number of cells horizontally
#define MAZE_HEIGHT 7   // Number of cells vertically
#define CELL_PITCH 8    // Pixel pitch betweencorridor centers (1px wall + 7px corridor) 
#define OFFSET_X 0      // Horizontal offset for maze rendering
#define OFFSET_Y 0      // Vertical offset for maze rendering

#define PAC_MOVE_MS 60
#define GHOST_MOVE_MS 90

#define DOT_PITCH 8
#define DOT_CX    4
#define DOT_CY    4

// Ghost house / central box exclusion zone in *pixel coordinates*.
#define GHOST_BOX_X0 40
#define GHOST_BOX_Y0 24
#define GHOST_BOX_X1 88
#define GHOST_BOX_Y1 40


// Game states
typedef enum {
    GameStateRunning,   // Game is actively being played
    GameStateWin,       // Player collected all dots
    GameStateGameOver   // Player was caught by a ghost
} GameState;

// Movement directions
typedef enum {
    DirNone,    // Not moving
    DirUp,      // Moving upward
    DirDown,    // Moving downward
    DirLeft,    // Moving left
    DirRight    // Moving right
} Direction;

// Movement connectivity bitmask (Von Neumann neighborhood)
enum {
    MV_N = 1 << 0,
    MV_E = 1 << 1,
    MV_S = 1 << 2,
    MV_W = 1 << 3,
};

static inline uint8_t dir_to_mask(Direction d) {
    switch(d) {
        case DirUp: return MV_N;
        case DirRight: return MV_E;
        case DirDown: return MV_S;
        case DirLeft: return MV_W;
        default: return 0;
    }
}


// Base entity structure for all moving objects (Puck and ghosts)
typedef struct {
    float x;                // X position in grid coordinates (can be fractional for smooth movement)
    float y;                // Y position in grid coordinates
    Direction dir;          // Current direction of movement
    Direction next_dir;     // Queued direction (for smoother turning)
    uint8_t anim_frame;     // Current animation frame (0-2 for ghosts, 0-1 for Puck)
    uint32_t last_move;     // Tick timestamp of last movement (for timing)
} Entity;

// Ghost-specific data structure
typedef struct {
    Entity entity;          // Base entity data (position, direction, etc.)
    bool eatable;           // True when ghost can be eaten (after power pill)
    uint32_t eatable_timer; // Tick when ghost became eatable (for timeout)
} Ghost;

// Complete game state
typedef struct {
    Entity pacgirl;            // The player character (Puck aka pac-girl)
    Ghost ghosts[3];           // Three ghost enemies
    uint8_t conn[MAZE_HEIGHT][MAZE_WIDTH];      // Movement connectivity bitmask per cell (M_N/M_E/M_S/M_W)
    uint8_t pellets[MAZE_HEIGHT][MAZE_WIDTH];   // 0=none, 1=dot, 2=power pill
    uint16_t dots_remaining;   // Number of dots left to collect
    uint16_t score;            // Current score
    GameState state;           // Current game state (running/win/game over)
    uint32_t power_pill_timer; // Tick when last power pill was eaten
    bool power_mode;           // True when power pill effect is active
	volatile bool* running;    // allows input_callback() to exit main loop
} GameData;

// Maze layout
static const uint8_t initial_conn[MAZE_HEIGHT][MAZE_WIDTH] = {
    {6, 10, 12,  6, 10, 10, 10, 10, 10, 10, 10, 12,  6, 10, 12},
    {5,  6, 11, 11, 10, 10, 12,  6, 10, 10, 10, 11, 11, 12,  5},
    {5,  7, 14, 10, 10, 14, 11, 15, 10, 14, 10, 10, 14, 13,  5},
    {7, 13,  5,  2, 10, 13,  2, 11,  8,  7, 10,  8,  5,  7, 13},
    {5,  5,  3, 10, 10, 15, 10, 10, 10, 15, 10, 10,  9,  5,  5},
    {5,  3, 14, 10, 10, 11, 12,  6, 10, 11, 10, 10, 14,  9,  5},
    {3, 10, 11, 10, 10, 10, 11, 11, 10, 10, 10, 10, 11, 10,  9},
};
/* Decimal | Binary | Directions allowed
 * --------+--------+--------------------
 *   0     | 0000   | none       (unreachable / wall)
 *   1     | 0001   | N
 *   2     | 0010   | E
 *   3     | 0011   | N, E       (bottom left corner)
 *   4     | 0100   | S
 *   5     | 0101   | N, S       (vertical corridor)
 *   6     | 0110   | E, S       (top left corner)
 *   7     | 0111   | N, E, S    
 *   8     | 1000   | W
 *   9     | 1001   | N, W       (bottom right corner)
 *  10     | 1010   | E, W       (horizontal corridor)
 *  11     | 1011   | N, E, W
 *  12     | 1100   | S, W       (top right corner)
 *  13     | 1101   | N, S, W
 *  14     | 1110   | E, S, W
 *  15     | 1111   | N, E, S, W (4-way intersection)
*/

// Collectible layout for the maze. NB: These are logical cells, not pixel!
// 0 = no pellet, normal dot, power pill
static const uint8_t initial_pellets[MAZE_HEIGHT][MAZE_WIDTH] = {
    {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 2},
};

static inline bool point_in_rect(int x, int y, int x0, int y0, int x1, int y1) {
    return (x >= x0) && (x <= x1) && (y >= y0) && (y <= y1);
}

// --- Initialize or reset the game to starting state -----
static void game_init(GameData* game) {
    // Initial maze layout 
    memcpy(game->conn, initial_conn, sizeof(initial_conn));
    memcpy(game->pellets, initial_pellets, sizeof(initial_pellets));
    
    // Count total dots and power pills for win condition
    game->dots_remaining = 0;
    for(int y = 0; y < MAZE_HEIGHT; y++) {
        for(int x = 0; x < MAZE_WIDTH; x++) {
            if(game->pellets[y][x] == 1 || game->pellets[y][x] == 2) {
                game->dots_remaining++;
            }
        }
    }
    
    // Initialize Puck (player) at starting position
    game->pacgirl.x = 7.0f;  // Center-ish horizontal position
    game->pacgirl.y = 6.0f;   // Near bottom of maze
    game->pacgirl.dir = DirNone;
    game->pacgirl.next_dir = DirNone;
    game->pacgirl.anim_frame = 0;
    game->pacgirl.last_move = 0;
    
    // Initialize the three ghosts at starting positions
    for(int i = 0; i < 3; i++) {
		// Start ghosts in the three spawn cells (7,4), (8,4), (9,4)
        game->ghosts[i].entity.x = 7.0f + i;  // 7, 8, 9
        game->ghosts[i].entity.y = 4.0f;
        game->ghosts[i].entity.dir = DirUp;       // sensible default for leaving
        game->ghosts[i].entity.next_dir = DirUp;
        game->ghosts[i].entity.anim_frame = 0;
        game->ghosts[i].entity.last_move = 0;
        game->ghosts[i].eatable = false;
        game->ghosts[i].eatable_timer = 0;
    }
    
    // Reset game state
    game->score = 0;
    game->state = GameStateRunning;
    game->power_mode = false;
    game->power_pill_timer = 0;
}

static void game_restart(GameData* game) {
    game_init(game);
    game->state = GameStateRunning;
}

static inline bool in_bounds(int x, int y) {
    return (x >= 0 && x < MAZE_WIDTH && y >= 0 && y < MAZE_HEIGHT);
}

// Convert a fractional grid coordinate into a SAFE cell index.
// We round to nearest cell (x+0.5), then clamp to [0..MAZE_WIDTH-1] / [0..MAZE_HEIGHT-1].
static inline int cell_x_from_float(float x) {
    int cx = (int)(x + 0.5f);
    if(cx < 0) cx = 0;
    if(cx >= MAZE_WIDTH) cx = MAZE_WIDTH - 1;
    return cx;
}

static inline int cell_y_from_float(float y) {
    int cy = (int)(y + 0.5f);
    if(cy < 0) cy = 0;
    if(cy >= MAZE_HEIGHT) cy = MAZE_HEIGHT - 1;
    return cy;
}

/**
 * Check whether an entity can move out of cell (x,y) in direction dir,
 * based on the precomputed connectivity bitmask.
 */
static bool can_move_from(GameData* game, int x, int y, Direction dir) {
    if(!in_bounds(x, y)) return false;
    uint8_t m = dir_to_mask(dir);
    if(m == 0) return false;
    return (game->conn[y][x] & m) != 0;
 }

/**
 * Calculate next position based on current position and direction
 * Handles screen wrapping for tunnel effect
 * @param x Current X position
 * @param y Current Y position
 * @param dir Direction of movement
 * @param next_x Output: next X position
 * @param next_y Output: next Y position
 */
static void get_next_position(float x, float y, Direction dir, float* next_x, float* next_y) {
    *next_x = x;
    *next_y = y;
    
    // Apply movement delta based on direction
    // Movement speed per update (fractional cells)
    switch(dir) {
        case DirUp:
            *next_y = y - 0.25f;
            break;
        case DirDown:
            *next_y = y + 0.25f;
            break;
        case DirLeft:
            *next_x = x - 0.25f;
            break;
        case DirRight:
            *next_x = x + 0.25f;
            break;
        default:
            break;
    }
    
    // Handle screen wrap-around (tunnel effect)
    if(*next_x < 0) *next_x = MAZE_WIDTH - 1;
    if(*next_x >= MAZE_WIDTH) *next_x = 0;
}

/**
 * Update Puck (player) position and handle dot collection
 * @param game Game state
 * @param tick Current game tick for timing
 */
static void update_pacgirl(GameData* game, uint32_t tick) {
    Entity* pacgirl = &game->pacgirl;
    
    // Try to change direction if a new direction was queued
    // This allows buffering input for smoother corner turning
    if(pacgirl->next_dir != DirNone) {
		int cx = cell_x_from_float(pacgirl->x);
        int cy = cell_y_from_float(pacgirl->y);
        if(can_move_from(game, cx, cy, pacgirl->next_dir)) {
            pacgirl->dir = pacgirl->next_dir;
            pacgirl->next_dir = DirNone;
        }
    }
    
    // Move on a real-time cadence (ms)
    if(tick - pacgirl->last_move > PAC_MOVE_MS) {
        int cx = cell_x_from_float(pacgirl->x);
        int cy = cell_y_from_float(pacgirl->y);
        if(can_move_from(game, cx, cy, pacgirl->dir)) {
            float next_x, next_y;
            get_next_position(pacgirl->x, pacgirl->y, pacgirl->dir, &next_x, &next_y);

            pacgirl->x = next_x;
            pacgirl->y = next_y;
            
            // Handle horizontal screen wrap-around
            if(pacgirl->x < 0) pacgirl->x = MAZE_WIDTH - 1;
            if(pacgirl->x >= MAZE_WIDTH) pacgirl->x = 0;
            
            // Check for collectibles at current grid position
            int grid_x = cell_x_from_float(pacgirl->x);
            int grid_y = cell_y_from_float(pacgirl->y);

            // Collect regular dot
            if(game->pellets[grid_y][grid_x] == 1) {
                game->pellets[grid_y][grid_x] = 0;  // Remove dot
                game->dots_remaining--;
                game->score += 10;
            } 
            // Collect power pill
            else if(game->pellets[grid_y][grid_x] == 2) {
                game->pellets[grid_y][grid_x] = 0;  // Remove power pill
                game->dots_remaining--;
                game->score += 50;
                game->power_mode = true;
                game->power_pill_timer = tick;
                
                // Make all ghosts eatable
                for(int i = 0; i < 3; i++) {
                    game->ghosts[i].eatable = true;
                    game->ghosts[i].eatable_timer = tick;
                }
            }
            
            // Alternate animation frame (closed/open mouth)
            pacgirl->anim_frame = (pacgirl->anim_frame + 1) % 2;
        }
        
        pacgirl->last_move = tick;
    }
    
    // Check win condition - all dots collected
    if(game->dots_remaining == 0) {
        game->state = GameStateWin;
    }
}

/**
 * Update ghost AI and movement
 * Ghosts chase Puck when normal, flee when eatable
 * @param game Game state
 * @param ghost The ghost to update
 * @param tick Current game tick for timing
 */
static void update_ghost(GameData* game, Ghost* ghost, uint32_t tick) {
    Entity* entity = &ghost->entity;

    // Check if eatable timer has expired (5 seconds = 5000ms)
    if(ghost->eatable && (tick - ghost->eatable_timer > 5000)) {
        ghost->eatable = false;
    }

    // Check if power mode has expired
    if(game->power_mode && (tick - game->power_pill_timer > 5000)) {
        game->power_mode = false;
    }

    // Move ghost on a real-time cadence (ms)
    if(tick - entity->last_move > GHOST_MOVE_MS) {
        Direction possible_dirs[4] = {DirUp, DirDown, DirLeft, DirRight};
        Direction best_dir = entity->dir;
        float best_dist = ghost->eatable ? -1.0f : 1e9f;

		int cx = cell_x_from_float(entity->x);
        int cy = cell_y_from_float(entity->y);
        
		// Release rule: while on spawn row y==4, try to leave the box.
        // If Up is possible, take it. Otherwise, drift horizontally toward the center (x==8).
        if(cy == 4) {
            Direction d = DirNone;
            if(can_move_from(game, cx, cy, DirUp)) {
                d = DirUp;
            } else if(cx < 8 && can_move_from(game, cx, cy, DirRight)) {
                d = DirRight;
            } else if(cx > 8 && can_move_from(game, cx, cy, DirLeft)) {
                d = DirLeft;
            }

            if(d != DirNone) {
                float nx, ny;
                get_next_position(entity->x, entity->y, d, &nx, &ny);
                entity->x = nx;
                entity->y = ny;
                entity->dir = d;
                entity->anim_frame = (entity->anim_frame + 1) & 1;
                entity->last_move = tick;
                return;
            }
        }

		

        // Pick best direction based on squared distance
        for(int i = 0; i < 4; i++) {
            Direction d = possible_dirs[i];

            if(!can_move_from(game, cx, cy, d)) continue;

            float nx, ny;
            get_next_position(entity->x, entity->y, d, &nx, &ny);

            float dx = nx - game->pacgirl.x;
            float dy = ny - game->pacgirl.y;
            float dist = dx * dx + dy * dy;

            if(ghost->eatable) {
                // Flee: maximize distance
                if(dist > best_dist) {
                    best_dist = dist;
                    best_dir = d;
                }
            } else {
                // Chase: minimize distance
                if(dist < best_dist) {
                    best_dist = dist;
                    best_dir = d;
                }
            }
        }

        // Move once in the chosen direction (still validate)
        if(can_move_from(game, cx, cy, best_dir)) {
            float nx, ny;
            get_next_position(entity->x, entity->y, best_dir, &nx, &ny);
            entity->x = nx;
            entity->y = ny;
            entity->dir = best_dir;

            // Animation frame toggle (0/1)
            entity->anim_frame = (entity->anim_frame + 1) & 1;
        }

        entity->last_move = tick;
    }

    // Collision with Puck
    float dx = entity->x - game->pacgirl.x;
    float dy = entity->y - game->pacgirl.y;
    float dist = dx * dx + dy * dy;

    if(dist < 0.5f) {
        if(ghost->eatable) {
            // Respawn ghost back to the house-ish area (stay within 15x7)
            entity->x = 7.0f;
            entity->y = 3.0f;
            ghost->eatable = false;
            game->score += 200;
        } else {
            game->state = GameStateGameOver;
        }
    }
}
    
static void draw_simple_modal(Canvas* canvas, const char* text) {
    const int box_w = 78;
    const int box_h = 20;
    const int box_x = (128 - box_w) / 2;
    const int box_y = 20;

    // White filled rectangle
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_box(canvas, box_x, box_y, box_w, box_h);

    // Black border
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_frame(canvas, box_x, box_y, box_w, box_h);

    // Text inside
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, box_x + 8, box_y + 14, text);
}

	

/**
 * Draw callback - renders the entire game screen
 * Called by the GUI system whenever the screen needs to update
 * @param canvas Drawing canvas
 * @param ctx Context pointer (GameData*)
 */
static void draw_callback(Canvas* canvas, void* ctx) {
    GameData* game = (GameData*)ctx;
	
    // Clear screen and set up drawing context
    canvas_clear(canvas);
    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    
    // Draw the background maze image (128x64 pixels)
    canvas_draw_icon(canvas, OFFSET_X, OFFSET_Y, &I_maze);
    
    // Draw dots and power pills on top of the maze background
    // Uses text rendering: "." for dots, "*" for power pills
    for(int y = 0; y < MAZE_HEIGHT; y++) {
        for(int x = 0; x < MAZE_WIDTH; x++) {
			if(game->pellets[y][x] == 0) continue;
            // Map cell (x,y) to pixel center based on the bitmap maze pitch (8 px)
            int cx = OFFSET_X + 1 + x * DOT_PITCH + DOT_CX;
            int cy = OFFSET_Y + 1 + y * DOT_PITCH + DOT_CY;
            // Skip the ghost house area so dots don't appear inside the box.
            if(point_in_rect(cx, cy, OFFSET_X + GHOST_BOX_X0, OFFSET_Y + GHOST_BOX_Y0,
                                   OFFSET_X + GHOST_BOX_X1, OFFSET_Y + GHOST_BOX_Y1)) {
                continue;
            }

            if(game->pellets[y][x] == 1) {
                // Regular dot: small filled circle
                canvas_draw_disc(canvas, cx, cy, 1);
            } else {
                // Power pill: slightly larger filled circle
                canvas_draw_disc(canvas, cx, cy, 2);
            }

        }
    }
    
    // Draw ghosts
    for(int i = 0; i < 3; i++) {
        Ghost* ghost = &game->ghosts[i];
        // Convert grid coordinates to pixel coordinates, centered on 7x7 icon
        int px = OFFSET_X + (int)(1 + ghost->entity.x * CELL_PITCH + 4) - 3;
        int py = OFFSET_Y + (int)(1 + ghost->entity.y * CELL_PITCH + 4) - 3;
        
        // Select icon based on state (eatable vs normal) and animation frame
        const Icon* ghost_icon = NULL;
        if(ghost->eatable) {
            // Blue/eatable ghost animation          
            switch(ghost->entity.anim_frame & 1) {
                case 0: ghost_icon = &I_prey_1; break;
                default: ghost_icon = &I_prey_2; break;
            }
        } else {
            // Normal ghost animation
            switch(ghost->entity.anim_frame & 1) {
                case 0: ghost_icon = &I_monster_1; break;
                default: ghost_icon = &I_monster_2; break;
            }
        }
        canvas_draw_icon(canvas, px, py, ghost_icon);
    }
    
    // Draw Puck: Convert grid coordinates to pixel coordinates, centered on 7x7 icon
    int px = OFFSET_X + (int)(1 + game->pacgirl.x * CELL_PITCH + 4) - 3;
    int py = OFFSET_Y + (int)(1 + game->pacgirl.y * CELL_PITCH + 4) - 3;
    
    // Select icon based on direction and animation frame (open/closed mouth)
    const Icon* puck_icon;
    if(game->pacgirl.dir == DirRight) {
        puck_icon = game->pacgirl.anim_frame ? &I_puck_r_o_7x7 : &I_puck_r_c_7x7;
    } else if(game->pacgirl.dir == DirLeft) {
        puck_icon = game->pacgirl.anim_frame ? &I_puck_l_o_7x7 : &I_puck_l_c_7x7;
    } else if(game->pacgirl.dir == DirUp) {
        puck_icon = game->pacgirl.anim_frame ? &I_puck_u_o_7x7 : &I_puck_u_c_7x7;
    } else if(game->pacgirl.dir == DirDown) {
        puck_icon = game->pacgirl.anim_frame ? &I_puck_d_o_7x7 : &I_puck_d_c_7x7;
    } else {
        // Default when not moving (facing right, closed mouth)
        puck_icon = &I_puck_r_c_7x7;
    }
    canvas_draw_icon(canvas, px, py, puck_icon);
    
    // Draw score at top of screen
    char score_str[32];
    snprintf(score_str, sizeof(score_str), "Score: %d", game->score);
    canvas_draw_str(canvas, 1, 64, score_str);
    
    // Draw game over / win messages
    if(game->state == GameStateWin) {
        draw_simple_modal(canvas, "YOU WIN!");
		elements_button_center(canvas, "OK");
    } else if(game->state == GameStateGameOver) {
        draw_simple_modal(canvas, "GAME OVER");
		elements_button_center(canvas, "OK");
    }
	
}

/**
 * Input callback - handles button presses
 * Called by the GUI system when user presses buttons
 * @param input_event The input event (button press, release, etc.)
 * @param ctx Context pointer (message queue)
 */
static void input_callback(InputEvent* input_event, void* ctx) {
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;
    furi_message_queue_put(event_queue, input_event, FuriWaitForever);
}

/**
 * Main app entry point
 * Sets up the game, handles the main loop, and cleans up
 * @param p Parameter (unused)
 * @return Exit code
 */
int32_t puckgirl_main(void* p) {
    UNUSED(p);
    
    // Allocate game data structure
    GameData* game = malloc(sizeof(GameData));
    game_init(game);
    
    // Create event queue for input handling
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    
    // Set up view port for rendering
    ViewPort* view_port = view_port_alloc();
    view_port_draw_callback_set(view_port, draw_callback, game);
    view_port_input_callback_set(view_port, input_callback, event_queue);
    
    // Register view port in GUI system
    Gui* gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);
    
    // Main game loop
    InputEvent event;
    bool running = true;
	game->running = &running;
    uint32_t tick = furi_get_tick();  
    
    while(running) {
		tick = furi_get_tick();
        // Check for input events with 10ms timeout
        if(furi_message_queue_get(event_queue, &event, 10) == FuriStatusOk) {
			// Back always exits, regardless of game state
			if(event.key == InputKeyBack &&
			   (event.type == InputTypePress ||
				event.type == InputTypeRepeat ||
				event.type == InputTypeLong)) {
				running = false;
				break;
			}
			// OK restarts game from GAME OVER
			if((game->state == GameStateGameOver || game->state == GameStateWin) &&
			   event.key == InputKeyOk &&
			   event.type == InputTypePress) {
				game_restart(game);
				continue;
			}
					
            if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                if(event.key == InputKeyBack) {
                    // Back button exits the game
                    running = false;
                } else if(game->state == GameStateRunning) {
                    // Game is active - handle directional input
                    switch(event.key) {
                        case InputKeyUp:
                            game->pacgirl.next_dir = DirUp;
                            break;
                        case InputKeyDown:
                            game->pacgirl.next_dir = DirDown;
                            break;
                        case InputKeyLeft:
                            game->pacgirl.next_dir = DirLeft;
                            break;
                        case InputKeyRight:
                            game->pacgirl.next_dir = DirRight;
                            break;
                        case InputKeyOk:
                            // OK button during gameplay restarts
                            game_init(game);
                            break;
                        default:
                            break;
                    }
                } else {
                    // Game is over - OK button restarts
                    if(event.key == InputKeyOk) {
                        game_init(game);
                    }
                }
            }
        }
        
        // Update game state if playing
        if(game->state == GameStateRunning) {
            update_pacgirl(game, tick);
            for(int i = 0; i < 3; i++) {
                update_ghost(game, &game->ghosts[i], tick);
            }
        }
        view_port_update(view_port);  // Request screen redraw
    }
    
    // Clean up resources
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    furi_record_close(RECORD_GUI);
    view_port_free(view_port);
    furi_message_queue_free(event_queue);
    free(game);
    
    return 0;
}
