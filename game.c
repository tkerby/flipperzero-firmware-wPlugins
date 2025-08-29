#include "game.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
static const EntityDescription submarine_desc;
static const EntityDescription torpedo_desc;
static const LevelBehaviour level;

/****** Input Handling ******/

static void handle_input(GameManager* manager, GameContext* game_context) {
    InputState input = game_manager_input_get(manager);
    uint32_t current_time = furi_get_tick();
    
    // Handle back button long/short press
    if(input.pressed & GameKeyBack) {
        game_context->back_press_start = current_time;
        game_context->back_long_press = false;
    }
    
    if(input.held & GameKeyBack) {
        if(current_time - game_context->back_press_start > 1000) { // 1 second for long press
            if(!game_context->back_long_press) {
                // Long press detected - exit game
                game_manager_game_stop(manager);
                return;
            }
        }
    }
    
    if(input.released & GameKeyBack) {
        if(current_time - game_context->back_press_start < 1000) {
            // Short press - toggle mode
            game_context->mode = (game_context->mode == GAME_MODE_NAV) ? 
                                 GAME_MODE_TORPEDO : GAME_MODE_NAV;
        }
    }
}

/****** Entities: Submarine ******/

typedef struct {
    GameContext* game_context;
} SubmarineContext;

static void submarine_start(Entity* self, GameManager* manager, void* context) {
    UNUSED(self);
    UNUSED(manager);
    SubmarineContext* sub_context = context;
    
    // Initialize submarine at center of screen
    entity_pos_set(self, (Vector){64, 32});
    
    // Add collision detection
    entity_collider_add_circle(self, 2);
    
    // Get game context reference
    sub_context->game_context = game_manager_game_context_get(manager);
}

static void submarine_update(Entity* self, GameManager* manager, void* context) {
    SubmarineContext* sub_context = context;
    GameContext* game_context = sub_context->game_context;
    
    // Handle input first
    handle_input(manager, game_context);
    
    InputState input = game_manager_input_get(manager);
    
    // Handle movement controls
    if(input.held & GameKeyLeft) {
        game_context->heading -= game_context->turn_rate;
        if(game_context->heading < 0) game_context->heading += 1.0f;
    }
    if(input.held & GameKeyRight) {
        game_context->heading += game_context->turn_rate;
        if(game_context->heading >= 1.0f) game_context->heading -= 1.0f;
    }
    if(input.held & GameKeyUp) {
        game_context->velocity += game_context->acceleration;
        if(game_context->velocity > game_context->max_velocity) {
            game_context->velocity = game_context->max_velocity;
        }
    }
    if(input.held & GameKeyDown) {
        game_context->velocity -= game_context->acceleration;
        if(game_context->velocity < 0) game_context->velocity = 0;
    }
    
    // Handle OK button (ping or fire)
    if(input.pressed & GameKeyOk) {
        if(game_context->mode == GAME_MODE_NAV) {
            // Start ping
            if(!game_context->ping_active) {
                game_context->ping_active = true;
                game_context->ping_x = game_context->pos_x;
                game_context->ping_y = game_context->pos_y;
                game_context->ping_radius = 0;
                game_context->ping_timer = furi_get_tick();
            }
        } else {
            // Fire torpedo
            if(game_context->torpedo_count < game_context->max_torpedoes) {
                Level* current_level = game_manager_current_level_get(manager);
                Entity* torpedo = level_add_entity(current_level, &torpedo_desc);
                if(torpedo) {
                    // Set torpedo initial position and heading
                    entity_pos_set(torpedo, (Vector){game_context->pos_x, game_context->pos_y});
                    game_context->torpedo_count++;
                }
            }
        }
    }
    
    // Update ping with terrain detection
    if(game_context->ping_active) {
        uint32_t current_time = furi_get_tick();
        if(current_time - game_context->ping_timer > 50) { // Update every 50ms
            game_context->ping_radius += 2;
            game_context->ping_timer = current_time;
            
            // Perform raycasting to detect terrain
            if(game_context->terrain && game_context->sonar_chart) {
                for(float angle = 0; angle < 2 * 3.14159f; angle += 0.1f) {
                    int ray_x = (int)(game_context->ping_x + cosf(angle) * game_context->ping_radius);
                    int ray_y = (int)(game_context->ping_y + sinf(angle) * game_context->ping_radius);
                    
                    if(ray_x >= 0 && ray_x < game_context->chart_width && 
                       ray_y >= 0 && ray_y < game_context->chart_height) {
                        
                        // Mark as discovered in sonar chart
                        int chart_idx = ray_y * game_context->chart_width + ray_x;
                        game_context->sonar_chart[chart_idx] = true;
                    }
                }
            }
            
            if(game_context->ping_radius > 40) { // Max ping radius (reduced for screen size)
                game_context->ping_active = false;
            }
        }
    }
    
    // Update submarine position with terrain collision
    float dx = game_context->velocity * cosf(game_context->heading * 2 * 3.14159f);
    float dy = game_context->velocity * sinf(game_context->heading * 2 * 3.14159f);
    
    float new_x = game_context->pos_x + dx;
    float new_y = game_context->pos_y + dy;
    
    // Check terrain collision
    if(game_context->terrain && terrain_check_collision(game_context->terrain, (int)new_x, (int)new_y)) {
        // Stop submarine if hitting terrain
        game_context->velocity = 0;
    } else {
        game_context->pos_x = new_x;
        game_context->pos_y = new_y;
    }
    
    // Keep submarine on screen
    if(game_context->pos_x < 2) game_context->pos_x = 2;
    if(game_context->pos_x > 126) game_context->pos_x = 126;
    if(game_context->pos_y < 2) game_context->pos_y = 2;
    if(game_context->pos_y > 62) game_context->pos_y = 62;
    
    // Update entity position
    entity_pos_set(self, (Vector){game_context->pos_x, game_context->pos_y});
}

static void submarine_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(self);
    UNUSED(manager);
    SubmarineContext* sub_context = context;
    GameContext* game_context = sub_context->game_context;
    
    // Draw terrain (discovered areas only)
    if(game_context->terrain && game_context->sonar_chart) {
        for(int y = 0; y < 64 && y < game_context->chart_height; y++) {
            for(int x = 0; x < 128 && x < game_context->chart_width; x++) {
                int chart_idx = y * game_context->chart_width + x;
                if(game_context->sonar_chart[chart_idx] && 
                   terrain_check_collision(game_context->terrain, x, y)) {
                    canvas_draw_dot(canvas, x, y);
                }
            }
        }
    }
    
    // Draw submarine
    canvas_draw_disc(canvas, game_context->pos_x, game_context->pos_y, 2);
    
    // Draw heading indicator
    float head_x = game_context->pos_x + cosf(game_context->heading * 2 * 3.14159f) * 8;
    float head_y = game_context->pos_y + sinf(game_context->heading * 2 * 3.14159f) * 8;
    canvas_draw_line(canvas, game_context->pos_x, game_context->pos_y, head_x, head_y);
    
    // Draw velocity vector or torpedo targeting
    if(game_context->mode == GAME_MODE_NAV && game_context->velocity > 0.01f) {
        // Navigation mode: show velocity vector
        float vel_x = game_context->pos_x + cosf(game_context->heading * 2 * 3.14159f) * game_context->velocity * 20;
        float vel_y = game_context->pos_y + sinf(game_context->heading * 2 * 3.14159f) * game_context->velocity * 20;
        canvas_draw_line(canvas, head_x, head_y, vel_x, vel_y);
    } else if(game_context->mode == GAME_MODE_TORPEDO) {
        // Torpedo mode: show targeting cone
        float range = 30.0f;
        float cone_angle = 0.1f; // ~6 degrees
        
        float target1_x = game_context->pos_x + cosf((game_context->heading + cone_angle) * 2 * 3.14159f) * range;
        float target1_y = game_context->pos_y + sinf((game_context->heading + cone_angle) * 2 * 3.14159f) * range;
        float target2_x = game_context->pos_x + cosf((game_context->heading - cone_angle) * 2 * 3.14159f) * range;
        float target2_y = game_context->pos_y + sinf((game_context->heading - cone_angle) * 2 * 3.14159f) * range;
        
        canvas_draw_line(canvas, game_context->pos_x, game_context->pos_y, target1_x, target1_y);
        canvas_draw_line(canvas, game_context->pos_x, game_context->pos_y, target2_x, target2_y);
    }
    
    // Draw ping
    if(game_context->ping_active) {
        canvas_draw_circle(canvas, game_context->ping_x, game_context->ping_y, game_context->ping_radius);
    }
    
    // Draw HUD
    canvas_printf(canvas, 2, 8, "V:%.2f H:%.2f", (double)game_context->velocity, (double)game_context->heading);
    canvas_printf(canvas, 2, 62, "%s T:%d/%d", 
                  game_context->mode == GAME_MODE_NAV ? "NAV" : "TORP",
                  game_context->torpedo_count, game_context->max_torpedoes);
}

static const EntityDescription submarine_desc = {
    .start = submarine_start,
    .stop = NULL,
    .update = submarine_update,
    .render = submarine_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(SubmarineContext),
};

/****** Entities: Torpedo ******/

typedef struct {
    float heading;
    float speed;
    GameContext* game_context;
} TorpedoContext;

static void torpedo_start(Entity* self, GameManager* manager, void* context) {
    UNUSED(self);
    TorpedoContext* torp_context = context;
    GameContext* game_context = game_manager_game_context_get(manager);
    
    // Initialize torpedo with submarine's heading
    torp_context->heading = game_context->heading;
    torp_context->speed = 0.15f; // Faster than submarine max speed
    torp_context->game_context = game_context;
    
    // Add collision detection
    entity_collider_add_circle(self, 1);
}

static void torpedo_update(Entity* self, GameManager* manager, void* context) {
    UNUSED(manager);
    TorpedoContext* torp_context = context;
    Vector pos = entity_pos_get(self);
    
    // Move torpedo
    float dx = torp_context->speed * cosf(torp_context->heading * 2 * 3.14159f);
    float dy = torp_context->speed * sinf(torp_context->heading * 2 * 3.14159f);
    
    pos.x += dx;
    pos.y += dy;
    
    // Check terrain collision
    if(torp_context->game_context->terrain && 
       terrain_check_collision(torp_context->game_context->terrain, (int)pos.x, (int)pos.y)) {
        // Torpedo hit terrain - remove it
        Level* current_level = game_manager_current_level_get(manager);
        torp_context->game_context->torpedo_count--;
        level_remove_entity(current_level, self);
        return;
    }
    
    // Check screen boundaries
    if(pos.x < 0 || pos.x > 128 || pos.y < 0 || pos.y > 64) {
        // Torpedo left screen - remove it
        Level* current_level = game_manager_current_level_get(manager);
        torp_context->game_context->torpedo_count--;
        level_remove_entity(current_level, self);
        return;
    }
    
    entity_pos_set(self, pos);
}

static void torpedo_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(manager);
    UNUSED(context);
    Vector pos = entity_pos_get(self);
    
    // Draw torpedo as small filled circle
    canvas_draw_disc(canvas, pos.x, pos.y, 1);
}

static void torpedo_stop(Entity* self, GameManager* manager, void* context) {
    UNUSED(self);
    UNUSED(manager);
    TorpedoContext* torp_context = context;
    
    // Decrement torpedo count when torpedo is destroyed
    if(torp_context->game_context->torpedo_count > 0) {
        torp_context->game_context->torpedo_count--;
    }
}

static const EntityDescription torpedo_desc = {
    .start = torpedo_start,
    .stop = torpedo_stop,
    .update = torpedo_update,
    .render = torpedo_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(TorpedoContext),
};

/****** Level ******/

static void level_alloc(Level* level, GameManager* manager, void* context) {
    UNUSED(context);
    UNUSED(manager);
    
    // Add submarine entity to the level
    level_add_entity(level, &submarine_desc);
}

static const LevelBehaviour level = {
    .alloc = level_alloc,
    .free = NULL,
    .start = NULL,
    .stop = NULL,
    .context_size = 0,
};

/****** Game ******/

static void game_start(GameManager* game_manager, void* ctx) {
    GameContext* game_context = ctx;
    
    // Initialize terrain system first
    game_context->terrain = terrain_manager_alloc(12345, 0.5f); // seed=12345, elevation=0.5
    
    // Initialize sonar chart (same size as screen for now)
    game_context->chart_width = 128;
    game_context->chart_height = 64;
    size_t chart_size = game_context->chart_width * game_context->chart_height;
    game_context->sonar_chart = malloc(chart_size * sizeof(bool));
    if(game_context->sonar_chart) {
        memset(game_context->sonar_chart, 0, chart_size * sizeof(bool));
    }
    
    // Find a safe starting position in water
    game_context->pos_x = 64;
    game_context->pos_y = 32;
    
    // Search for water if starting position is in terrain
    if(game_context->terrain) {
        bool found_water = false;
        for(int attempts = 0; attempts < 100 && !found_water; attempts++) {
            int test_x = 10 + (attempts * 3) % 108; // Scan across screen
            int test_y = 10 + (attempts / 36) % 44; // Scan down screen
            
            if(!terrain_check_collision(game_context->terrain, test_x, test_y)) {
                game_context->pos_x = test_x;
                game_context->pos_y = test_y;
                found_water = true;
            }
        }
        
        // Add initial sonar coverage around starting position
        if(game_context->sonar_chart) {
            int start_x = (int)game_context->pos_x;
            int start_y = (int)game_context->pos_y;
            
            for(int dy = -10; dy <= 10; dy++) {
                for(int dx = -10; dx <= 10; dx++) {
                    int map_x = start_x + dx;
                    int map_y = start_y + dy;
                    
                    if(map_x >= 0 && map_x < game_context->chart_width &&
                       map_y >= 0 && map_y < game_context->chart_height) {
                        int chart_idx = map_y * game_context->chart_width + map_x;
                        game_context->sonar_chart[chart_idx] = true;
                    }
                }
            }
        }
    }
    game_context->velocity = 0;
    game_context->heading = 0;
    game_context->mode = GAME_MODE_NAV;
    
    game_context->torpedo_count = 0;
    game_context->max_torpedoes = 6;
    
    game_context->ping_active = false;
    game_context->ping_radius = 0;
    
    game_context->back_press_start = 0;
    game_context->back_long_press = false;
    
    // Game settings
    game_context->max_velocity = 0.1f;
    game_context->turn_rate = 0.002f;
    game_context->acceleration = 0.002f;
    
    // Add level to the game
    game_manager_add_level(game_manager, &level);
}

static void game_stop(void* ctx) {
    GameContext* game_context = ctx;
    
    // Clean up terrain system
    if(game_context->terrain) {
        terrain_manager_free(game_context->terrain);
    }
    
    // Clean up sonar chart
    if(game_context->sonar_chart) {
        free(game_context->sonar_chart);
    }
}

const Game game = {
    .target_fps = 30,
    .show_fps = false,
    .always_backlight = true,
    .start = game_start,
    .stop = game_stop,
    .context_size = sizeof(GameContext),
};