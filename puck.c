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
#include <gui/elements.h> // to access button drawing functions
#include "puck_icons.h" // Custom icon definitions, header file is auto-magically generated

// Maze configuration - defines the logical grid structure
#define MAZE_WIDTH 29   // Number of cells horizontally
#define MAZE_HEIGHT 9   // Number of cells vertically
#define CELL_SIZE 4     // Pixels per cell (to be adjusted based on actual maze layout)
#define OFFSET_X 0      // Horizontal offset for maze rendering
#define OFFSET_Y 0      // Vertical offset for maze rendering

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
    Entity pacman;              // The player character (Puck)
    Ghost ghosts[3];            // Three ghost enemies
    uint8_t maze[MAZE_HEIGHT][MAZE_WIDTH];  // Maze layout: 1=wall, 0=empty, 2=dot, 3=power pill
    uint16_t dots_remaining;    // Number of dots left to collect
    uint16_t score;             // Current score
    GameState state;            // Current game state (running/win/game over)
    uint32_t power_pill_timer;  // Tick when last power pill was eaten
    bool power_mode;            // True when power pill effect is active
} GameData;

// Maze layout (1 = wall, 0 = path, 2 = dot, 3 = power pill)
// This defines the collision/gameplay grid, while the visual maze comes from maze.png
static const uint8_t initial_maze[MAZE_HEIGHT][MAZE_WIDTH] = {
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,1,2,1,1,1,1,1,1,2,1,2,1,1,1,1,1,1,2,1,1,1,2,1,2,1},
    {1,3,1,1,1,2,1,1,1,1,1,1,2,1,2,1,1,1,1,1,1,2,1,1,1,2,1,3,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,2,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,2,1,2,1,1,1,2,1,2,1},
    {1,2,2,2,2,2,1,2,2,2,2,2,2,1,2,2,2,2,2,2,1,2,2,2,2,2,2,2,1},
    {1,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,1},
    {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}
};

// --- Initialize or reset the game to starting state -----
static void game_init(GameData* game) {
    // Copy the initial maze layout into game state
    memcpy(game->maze, initial_maze, sizeof(initial_maze));
    
    // Count total dots and power pills for win condition
    game->dots_remaining = 0;
    for(int y = 0; y < MAZE_HEIGHT; y++) {
        for(int x = 0; x < MAZE_WIDTH; x++) {
            if(game->maze[y][x] == 2 || game->maze[y][x] == 3) {
                game->dots_remaining++;
            }
        }
    }
    
    // Initialize Puck (player) at starting position
    game->pacman.x = 14.0f;  // Center-ish horizontal position
    game->pacman.y = 7.0f;   // Near bottom of maze
    game->pacman.dir = DirNone;
    game->pacman.next_dir = DirNone;
    game->pacman.anim_frame = 0;
    game->pacman.last_move = 0;
    
    // Initialize the three ghosts at starting positions
    for(int i = 0; i < 3; i++) {
        game->ghosts[i].entity.x = 12.0f + i * 2;  // Space them horizontally
        game->ghosts[i].entity.y = 4.0f;           // Middle area of maze
        game->ghosts[i].entity.dir = DirRight;
        game->ghosts[i].entity.next_dir = DirRight;
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

/**
 * Check if a grid position is valid (not a wall, within bounds)
 * @param game Game state
 * @param x Grid X coordinate
 * @param y Grid Y coordinate
 * @return true if position is walkable, false if wall or out of bounds
 */
static bool is_valid_position(GameData* game, int x, int y) {
    if(x < 0 || x >= MAZE_WIDTH || y < 0 || y >= MAZE_HEIGHT) {
        return false;
    }
    return game->maze[y][x] != 1;  // 1 = wall
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
    // 0.15f is the movement speed per update
    switch(dir) {
        case DirUp:
            *next_y = y - 0.15f;
            break;
        case DirDown:
            *next_y = y + 0.15f;
            break;
        case DirLeft:
            *next_x = x - 0.15f;
            break;
        case DirRight:
            *next_x = x + 0.15f;
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
static void update_pacman(GameData* game, uint32_t tick) {
    Entity* pacman = &game->pacman;
    
    // Try to change direction if a new direction was queued
    // This allows buffering input for smoother corner turning
    if(pacman->next_dir != DirNone) {
        float next_x, next_y;
        get_next_position(pacman->x, pacman->y, pacman->next_dir, &next_x, &next_y);
        
        // Only change direction if the new direction is valid (not into a wall)
        if(is_valid_position(game, (int)(next_x + 0.5f), (int)(next_y + 0.5f))) {
            pacman->dir = pacman->next_dir;
            pacman->next_dir = DirNone;
        }
    }
    
    // Move every 50 ticks (controls Puck's speed)
    if(tick - pacman->last_move > 50) {
        float next_x, next_y;
        get_next_position(pacman->x, pacman->y, pacman->dir, &next_x, &next_y);
        
        // Only move if next position is valid
        if(is_valid_position(game, (int)(next_x + 0.5f), (int)(next_y + 0.5f))) {
            pacman->x = next_x;
            pacman->y = next_y;
            
            // Handle horizontal screen wrap-around
            if(pacman->x < 0) pacman->x = MAZE_WIDTH - 1;
            if(pacman->x >= MAZE_WIDTH) pacman->x = 0;
            
            // Check for collectibles at current grid position
            int grid_x = (int)(pacman->x + 0.5f);  // Round to nearest grid cell
            int grid_y = (int)(pacman->y + 0.5f);
            
            // Collect regular dot
            if(game->maze[grid_y][grid_x] == 2) {
                game->maze[grid_y][grid_x] = 0;  // Remove dot
                game->dots_remaining--;
                game->score += 10;
            } 
            // Collect power pill
            else if(game->maze[grid_y][grid_x] == 3) {
                game->maze[grid_y][grid_x] = 0;  // Remove power pill
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
            pacman->anim_frame = (pacman->anim_frame + 1) % 2;
        }
        
        pacman->last_move = tick;
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
    if(tick - game->power_pill_timer > 5000) {
        game->power_mode = false;
    }
    
    // Move ghost every 80 ticks (slightly slower than Puck)
    if(tick - entity->last_move > 80) {
        // Simple AI: evaluate all four directions and pick the best
        Direction possible_dirs[4] = {DirUp, DirDown, DirLeft, DirRight};
        Direction best_dir = entity->dir;
        float best_dist = 1000.0f;
        
        // Try each direction and find the one that gets closest to (or furthest from) Puck
        for(int i = 0; i < 4; i++) {
            float next_x, next_y;
            get_next_position(entity->x, entity->y, possible_dirs[i], &next_x, &next_y);
            
            // Only consider valid moves (not into walls)
            if(is_valid_position(game, (int)(next_x + 0.5f), (int)(next_y + 0.5f))) {
                // Calculate distance to Puck using simple distance formula
                float dx = next_x - game->pacman.x;
                float dy = next_y - game->pacman.y;
                float dist = dx * dx + dy * dy;  // Squared distance (avoid sqrt for speed)
                
                if(ghost->eatable) {
                    // When eatable, run away - pick direction with largest distance
                    if(dist > best_dist) {
                        best_dist = dist;
                        best_dir = possible_dirs[i];
                    }
                } else {
                    // When normal, chase - pick direction with smallest distance
                    if(dist < best_dist) {
                        best_dist = dist;
                        best_dir = possible_dirs[i];
                    }
                }
            }
        }
        
        entity->dir = best_dir;
        
        // Actually move in the chosen direction
        float next_x, next_y;
        get_next_position(entity->x, entity->y, entity->dir, &next_x, &next_y);
        
        if(is_valid_position(game, (int)(next_x + 0.5f), (int)(next_y + 0.5f))) {
            entity->x = next_x;
            entity->y = next_y;
            
            // Handle screen wrap-around
            if(entity->x < 0) entity->x = MAZE_WIDTH - 1;
            if(entity->x >= MAZE_WIDTH) entity->x = 0;
            
            // Cycle through animation frames (0, 1, 2)
            entity->anim_frame = (entity->anim_frame + 1) % 3;
        }
        
        entity->last_move = tick;
    }
    
    // Check collision with Puck
    float dx = entity->x - game->pacman.x;
    float dy = entity->y - game->pacman.y;
    float dist = dx * dx + dy * dy;
    
    // Collision threshold: 0.5 squared distance units
    if(dist < 0.5f) {
        if(ghost->eatable) {
            // Ghost was eaten - respawn at center
            entity->x = 14.0f;
            entity->y = 4.0f;
            ghost->eatable = false;
            game->score += 200;  // Bonus points for eating ghost
        } else {
            // Puck was caught - game over
            game->state = GameStateGameOver;
        }
    }
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
    // Uses text rendering as suggested: "." for dots, "*" for power pills
    for(int y = 0; y < MAZE_HEIGHT; y++) {
        for(int x = 0; x < MAZE_WIDTH; x++) {
            int px = OFFSET_X + x * CELL_SIZE;
            int py = OFFSET_Y + y * CELL_SIZE;
            
            if(game->maze[y][x] == 2) {
                // Draw regular dot
                canvas_draw_str(canvas, px + 1, py + 3, ".");
            } else if(game->maze[y][x] == 3) {
                // Draw power pill (larger)
                canvas_draw_str(canvas, px, py + 3, "*");
            }
        }
    }
    
    // Draw ghosts
    for(int i = 0; i < 3; i++) {
        Ghost* ghost = &game->ghosts[i];
        // Convert grid coordinates to pixel coordinates, centered on 7x7 icon
        int px = OFFSET_X + (int)(ghost->entity.x * CELL_SIZE) - 3;
        int py = OFFSET_Y + (int)(ghost->entity.y * CELL_SIZE) - 3;
        
        // Select icon based on state (eatable vs normal) and animation frame
        const Icon* ghost_icon;
        if(ghost->eatable) {
            // Blue/eatable ghost animation
            switch(ghost->entity.anim_frame) {
                case 0: ghost_icon = &I_prey_1; break;
                case 1: ghost_icon = &I_prey_2; break;
                case 2: ghost_icon = &I_prey_3; break;
                default: ghost_icon = &I_prey_1; break;
            }
        } else {
            // Normal ghost animation
            switch(ghost->entity.anim_frame) {
                case 0: ghost_icon = &I_monster_1; break;
                case 1: ghost_icon = &I_monster_2; break;
                case 2: ghost_icon = &I_monster_3; break;
                default: ghost_icon = &I_monster_1; break;
            }
        }
        canvas_draw_icon(canvas, px, py, ghost_icon);
    }
    
    // Draw Pac-Man (Puck)
    // Convert grid coordinates to pixel coordinates, centered on 7x7 icon
    int px = OFFSET_X + (int)(game->pacman.x * CELL_SIZE) - 3;
    int py = OFFSET_Y + (int)(game->pacman.y * CELL_SIZE) - 3;
    
    // Select icon based on direction and animation frame (open/closed mouth)
    const Icon* puck_icon;
    if(game->pacman.dir == DirRight) {
        puck_icon = game->pacman.anim_frame ? &I_puck_r_o_7x7 : &I_puck_r_c_7x7;
    } else if(game->pacman.dir == DirLeft) {
        puck_icon = game->pacman.anim_frame ? &I_puck_l_o_7x7 : &I_puck_l_c_7x7;
    } else if(game->pacman.dir == DirUp) {
        puck_icon = game->pacman.anim_frame ? &I_puck_u_o_7x7 : &I_puck_u_c_7x7;
    } else if(game->pacman.dir == DirDown) {
        puck_icon = game->pacman.anim_frame ? &I_puck_d_o_7x7 : &I_puck_d_c_7x7;
    } else {
        // Default when not moving (facing right, closed mouth)
        puck_icon = &I_puck_r_c_7x7;
    }
    canvas_draw_icon(canvas, px, py, puck_icon);
    
    // Draw score at top of screen
    char score_str[32];
    snprintf(score_str, sizeof(score_str), "Score: %d", game->score);
    canvas_draw_str(canvas, 2, 6, score_str);
    
    // Draw game over / win messages
    if(game->state == GameStateWin) {
        canvas_draw_str(canvas, 40, 32, "YOU WIN!");
    } else if(game->state == GameStateGameOver) {
        canvas_draw_str(canvas, 35, 32, "GAME OVER");
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
int32_t pacgrl_main(void* p) {
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
    uint32_t tick = 0;  // Game tick counter for timing
    
    while(running) {
        // Check for input events with 10ms timeout
        if(furi_message_queue_get(event_queue, &event, 10) == FuriStatusOk) {
            if(event.type == InputTypePress || event.type == InputTypeRepeat) {
                if(event.key == InputKeyBack) {
                    // Back button exits the game
                    running = false;
                } else if(game->state == GameStateRunning) {
                    // Game is active - handle directional input
                    switch(event.key) {
                        case InputKeyUp:
                            game->pacman.next_dir = DirUp;
                            break;
                        case InputKeyDown:
                            game->pacman.next_dir = DirDown;
                            break;
                        case InputKeyLeft:
                            game->pacman.next_dir = DirLeft;
                            break;
                        case InputKeyRight:
                            game->pacman.next_dir = DirRight;
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
            update_pacman(game, tick);
            for(int i = 0; i < 3; i++) {
                update_ghost(game, &game->ghosts[i], tick);
            }
        }
        
        tick++;  // Increment game tick
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
