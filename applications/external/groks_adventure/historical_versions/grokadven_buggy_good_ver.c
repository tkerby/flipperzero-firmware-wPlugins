#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <stdlib.h>
#include <string.h>
#include <storage/storage.h>
#include <lib/toolbox/path.h>
#include <math.h>

// Screen dimensions for Flipper Zero
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define WORLD_WIDTH SCREEN_WIDTH // Restored to screen width
#define PLAY_HEIGHT (SCREEN_HEIGHT - 7)
#define ARRAY_SIZE (WORLD_WIDTH * PLAY_HEIGHT + 1)

// Game states
typedef enum {
    STATE_TITLE,
    STATE_GAME,
    STATE_PAUSED,
    STATE_DEATH,
    STATE_EXIT
} GameState;

// Entity types
typedef enum {
    ENTITY_PLAYER,
    ENTITY_ENEMY,
    ENTITY_BOSS,
    ENTITY_FINAL_BOSS
} EntityType;

// Entity structure
typedef struct {
    EntityType type;
    int x, y;
    int width, height;
    int health_bars;
    int power_points;
    int power_level;
    bool facing_right;
    uint64_t double_power_timer;
    uint64_t pickup_cooldown;
} Entity;

// Platform structure
typedef struct {
    int x, y;
    int width, height;
} Platform;

// Pickup types
typedef enum {
    PICKUP_ALMOND_WATER,
    PICKUP_CHEST,
    PICKUP_DOOR,
    PICKUP_FURNITURE
} PickupType;

typedef struct {
    PickupType type;
    int x, y;
    char symbol; // For furniture variety
    bool falling;
} Pickup;

// Projectile structure
typedef struct {
    int x, y;
    int dx, dy;
    bool active;
    bool from_player;
} Projectile;

typedef struct {
    bool dark_mode;
    bool fps_display;
} GameSettings;

// Game context
typedef struct {
    GameState state;
    Canvas* canvas;
    InputEvent input;
    uint32_t frame_count;
    float fps;
    uint64_t last_frame_time;
    uint64_t last_fps_check;
    uint64_t last_input_time;
    uint64_t day_night_timer;
    uint64_t weather_timer;
    uint64_t weather_duration;
    uint64_t final_boss_timer;
    uint64_t iframe_timer;
    uint64_t special_timer;
    uint64_t hold_back_timer;
    uint64_t hold_up_timer;
    uint64_t hold_down_timer;
    uint64_t hold_left_timer;
    uint64_t hold_right_timer;
    uint64_t door_timer;
    bool is_night;
    bool weather_active;
    int weather_type; // 0: none, 1: snow, 2: rain, 3: constellations
    int sun_moon_x;
    GameSettings settings;
    Entity player;
    Entity entities[5]; // Increased to support more enemies
    int entity_count;
    int almond_water_count;
    bool in_iframes;
    bool special_active;
    char world_map[ARRAY_SIZE];
    char world_objects[ARRAY_SIZE];
    char oset[5];
    Platform platforms[20];
    int platform_count;
    Pickup pickups[50];
    int pickup_count;
    Projectile player_projectile;
    Projectile enemy_projectiles[2]; // For boss
    int enemy_projectile_count;
    uint32_t world_seed;
    bool bypass_collision; // New flag for collision bypass
} GameContext;

// File paths
#define STORAGE_PATH "/ext/grokadven_03/"
#define WMA_FILE STORAGE_PATH "WMA_arr.txt"
#define WSO_FILE STORAGE_PATH "WSO_arr.txt"
#define SETTINGS_FILE STORAGE_PATH "settings.bin"
#define PLAYER_DATA_FILE STORAGE_PATH "player_data.bin"

// Initialize arrays
void init_arrays(GameContext* ctx) {
    memset(ctx->world_map, 0, ARRAY_SIZE);
    memset(ctx->world_objects, 0, ARRAY_SIZE);
    ctx->world_map[(ARRAY_SIZE-1)/2] = '~'; // Respawn point
    strcpy(ctx->oset, "000Z");
    ctx->world_seed = 0;
}

// Save array to file
bool save_array(const char* filename, const char* data, size_t size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    bool success = storage_file_open(file, filename, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        storage_file_write(file, data, size);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Load array from file
bool load_array(const char* filename, char* data, size_t size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    bool success = storage_file_open(file, filename, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) {
        storage_file_read(file, data, size);
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Save settings
bool save_settings(GameContext* ctx) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    bool success = storage_file_open(file, SETTINGS_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        storage_file_write(file, &ctx->settings, sizeof(GameSettings));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Load settings
bool load_settings(GameContext* ctx) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    bool success = storage_file_open(file, SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) {
        storage_file_read(file, &ctx->settings, sizeof(GameSettings));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Save player data
bool save_player_data(GameContext* ctx) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    bool success = storage_file_open(file, PLAYER_DATA_FILE, FSAM_WRITE, FSOM_CREATE_ALWAYS);
    if(success) {
        storage_file_write(file, &ctx->player, sizeof(Entity));
        storage_file_write(file, ctx->oset, sizeof(ctx->oset));
        storage_file_write(file, &ctx->world_seed, sizeof(uint32_t));
        storage_file_write(file, &ctx->almond_water_count, sizeof(int));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Load player data
bool load_player_data(GameContext* ctx) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;
    File* file = storage_file_alloc(storage);
    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }
    bool success = storage_file_open(file, PLAYER_DATA_FILE, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) {
        storage_file_read(file, &ctx->player, sizeof(Entity));
        storage_file_read(file, ctx->oset, sizeof(ctx->oset));
        storage_file_read(file, &ctx->world_seed, sizeof(uint32_t));
        storage_file_read(file, &ctx->almond_water_count, sizeof(int));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Check if player can level up
void check_level_up(GameContext* ctx) {
    while(ctx->player.power_points >= 100 && (ctx->player.width < 9 || ctx->player.height < 12)) {
        ctx->player.power_points -= 100;
        ctx->player.power_level++;
        if(ctx->player.width < 9) ctx->player.width++;
        if(ctx->player.height < 12) ctx->player.height++;
    }
}

// Generate world elements
static void generate_world(GameContext* ctx) {
    srand(ctx->world_seed);
    ctx->platform_count = 0;
    ctx->pickup_count = 0;
    ctx->entity_count = 2; // Minimum 2 enemies for hard mode
    // Ground platform
    ctx->platforms[ctx->platform_count].x = 0;
    ctx->platforms[ctx->platform_count].y = PLAY_HEIGHT - 6;
    ctx->platforms[ctx->platform_count].width = WORLD_WIDTH;
    ctx->platforms[ctx->platform_count].height = 6;
    ctx->platform_count++;
    // Fewer random platforms
    for(int i = 0; i < 19; i++) {
        if(rand() % 100 < 5) { // Reduced to 5% chance for platform
            int px = rand() % (WORLD_WIDTH - 50);
            int py = rand() % (PLAY_HEIGHT - 10) + 5;
            int pw = rand() % 40 + 20;
            ctx->platforms[ctx->platform_count].x = px;
            ctx->platforms[ctx->platform_count].y = py;
            ctx->platforms[ctx->platform_count].width = pw;
            ctx->platforms[ctx->platform_count].height = 3;
            ctx->platform_count++;
            if(ctx->platform_count >= 20) break;
        }
    }
    for(int i = 0; i < ARRAY_SIZE - 1; i++) {
        if(ctx->world_map[i] == 0) {
            int roll = rand() % 100;
            if(roll < 5) ctx->world_map[i] = 'W'; // Wall
            else if(roll < 15) { // Increased pickup chance
                ctx->pickups[ctx->pickup_count].type = PICKUP_ALMOND_WATER;
                ctx->pickups[ctx->pickup_count].x = i % WORLD_WIDTH;
                ctx->pickups[ctx->pickup_count].y = i / WORLD_WIDTH;
                ctx->pickups[ctx->pickup_count].falling = true;
                ctx->pickup_count++;
            } else if(roll < 25) { // Increased chest chance
                ctx->pickups[ctx->pickup_count].type = PICKUP_CHEST;
                ctx->pickups[ctx->pickup_count].x = i % WORLD_WIDTH;
                ctx->pickups[ctx->pickup_count].y = i / WORLD_WIDTH;
                ctx->pickups[ctx->pickup_count].falling = true;
                ctx->pickup_count++;
            } else if(roll < 30 && ctx->pickup_count == 0) { // Ensure at least one door if none
                ctx->pickups[ctx->pickup_count].type = PICKUP_DOOR;
                ctx->pickups[ctx->pickup_count].x = i % WORLD_WIDTH;
                ctx->pickups[ctx->pickup_count].y = i / WORLD_WIDTH - 6; // Place on ground
                ctx->pickups[ctx->pickup_count].falling = false;
                ctx->pickup_count++;
            }
        }
        if(ctx->world_objects[i] == 0 && rand() % 100 < 10) { // Increased furniture chance to 10%
            ctx->pickups[ctx->pickup_count].type = PICKUP_FURNITURE;
            ctx->pickups[ctx->pickup_count].x = i % WORLD_WIDTH;
            ctx->pickups[ctx->pickup_count].y = i / WORLD_WIDTH;
            int r = rand() % 3;
            ctx->pickups[ctx->pickup_count].symbol = (r == 0 ? 'F' : (r == 1 ? 'h' : 'I')); // Hat rack, chair, standing light
            ctx->pickups[ctx->pickup_count].falling = false;
            ctx->pickup_count++;
        }
    }
    // Ensure minimum 2 enemies with increased health
    if(ctx->entity_count < 2) {
        for(int i = 0; i < 2; i++) {
            ctx->entities[i].type = ENTITY_ENEMY;
            ctx->entities[i].x = rand() % (WORLD_WIDTH - ctx->entities[i].width);
            ctx->entities[i].y = rand() % (PLAY_HEIGHT - ctx->entities[i].height);
            ctx->entities[i].width = 3;
            ctx->entities[i].height = 4;
            ctx->entities[i].health_bars = 5; // Increased from 3 for hard mode
            ctx->entities[i].power_points = 0;
            ctx->entities[i].power_level = 0;
            ctx->entities[i].facing_right = true;
            ctx->entities[i].double_power_timer = 0;
            ctx->entities[i].pickup_cooldown = 0;
        }
        ctx->entity_count = 2;
    }
    // Add boss with increased health
    if(ctx->entity_count < 3) {
        ctx->entities[2].type = ENTITY_BOSS;
        ctx->entities[2].x = WORLD_WIDTH / 2 + 20;
        ctx->entities[2].y = PLAY_HEIGHT - 7;
        ctx->entities[2].width = 3;
        ctx->entities[2].height = 6;
        ctx->entities[2].health_bars = 10; // Increased from 5 for hard mode
        ctx->entities[2].power_points = 0;
        ctx->entities[2].power_level = 0;
        ctx->entities[2].facing_right = true;
        ctx->entities[2].double_power_timer = 0;
        ctx->entities[2].pickup_cooldown = 0;
        ctx->entity_count = 3;
    }
    ctx->world_seed++;
}

// Initialize game context
static void game_init(GameContext* ctx) {
    ctx->frame_count = 0;
    ctx->fps = 0.0f;
    ctx->last_frame_time = furi_get_tick();
    ctx->last_fps_check = ctx->last_frame_time;
    ctx->last_input_time = ctx->last_frame_time;
    ctx->day_night_timer = ctx->last_frame_time;
    ctx->weather_timer = ctx->last_frame_time;
    ctx->weather_duration = 0;
    ctx->final_boss_timer = 0;
    ctx->iframe_timer = 0;
    ctx->special_timer = 0;
    ctx->hold_back_timer = 0;
    ctx->hold_up_timer = 0;
    ctx->hold_down_timer = 0;
    ctx->hold_left_timer = 0;
    ctx->hold_right_timer = 0;
    ctx->door_timer = ctx->last_frame_time;
    ctx->weather_active = false;
    ctx->weather_type = 0;
    ctx->is_night = true;
    ctx->sun_moon_x = 0;
    ctx->settings.dark_mode = true;
    ctx->settings.fps_display = true;
    ctx->player.type = ENTITY_PLAYER;
    ctx->player.x = SCREEN_WIDTH / 2;
    ctx->player.y = PLAY_HEIGHT - 7;
    ctx->player.width = 3;
    ctx->player.height = 4;
    ctx->player.health_bars = 2;
    ctx->player.power_points = 0;
    ctx->player.power_level = 0;
    ctx->player.facing_right = false;
    ctx->player.double_power_timer = 0;
    ctx->player.pickup_cooldown = 0;
    ctx->in_iframes = false;
    ctx->special_active = false;
    ctx->almond_water_count = 0;
    ctx->player_projectile.active = false;
    ctx->enemy_projectile_count = 0;
    ctx->entity_count = 0;
    ctx->bypass_collision = false; // Initialize bypass flag
    init_arrays(ctx);
    // Load data before generating world to preserve state
    if(load_settings(ctx)) {
        ctx->settings.dark_mode = true; // Ensure hard mode (always on)
    }
    if(load_player_data(ctx)) {
        // Ensure loaded data is valid
        if(ctx->player.x < 0 || ctx->player.x >= WORLD_WIDTH - ctx->player.width) ctx->player.x = SCREEN_WIDTH / 2;
        if(ctx->player.y < 0 || ctx->player.y >= PLAY_HEIGHT - ctx->player.height) ctx->player.y = PLAY_HEIGHT - 7;
        if(ctx->almond_water_count < 0 || ctx->almond_water_count > 3) ctx->almond_water_count = 0;
    }
    load_array(WMA_FILE, ctx->world_map, ARRAY_SIZE);
    load_array(WSO_FILE, ctx->world_objects, ARRAY_SIZE);
    check_level_up(ctx);
    ctx->state = STATE_TITLE;
}

// Collision check for entity vs platform (bottom edge only for downward movement)
bool collides_with_platform(int ex, int ey, int ew, int eh, Platform* p) {
    return ex + ew > p->x + 1 && ex < p->x + p->width - 1 && // Adjusted to exclude 1-pixel borders on sides
           ey + eh >= p->y && ey < p->y + 1; // Adjusted to 1-pixel border
}

bool can_move_down(int ex, int ey, int ew, int eh, GameContext* ctx) {
    for(int i = 0; i < ctx->platform_count; i++) {
        if(collides_with_platform(ex, ey + 1, ew, eh, &ctx->platforms[i]) &&
           ey + eh <= ctx->platforms[i].y + 1) { // Allow 1-pixel under platform
            return false;
        }
    }
    return true;
}

bool collides_with_wall(int ex, int ey, int ew, int eh, GameContext* ctx) {
    for(int y = ey; y < ey + eh; y++) {
        for(int x = ex; x < ex + ew; x++) {
            int idx = y * WORLD_WIDTH + x;
            if(idx >= 0 && idx < ARRAY_SIZE && ctx->world_map[idx] == 'W') {
                return true;
            }
        }
    }
    return false;
}

bool collides_with_furniture(int ex, int ey, int ew, int eh, GameContext* ctx) {
    for(int i = 0; i < ctx->pickup_count; i++) {
        if(ctx->pickups[i].type == PICKUP_FURNITURE) {
            int px = ctx->pickups[i].x;
            int py = ctx->pickups[i].y;
            if(ex + ew > px && ex < px + 1 && ey + eh > py && ey < py + 1) {
                return true;
            }
        }
    }
    return false;
}

// Draw title screen with wax drip effect
static void draw_title(GameContext* ctx, Canvas* canvas) {
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas_set_color(canvas, ColorWhite);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 3 - 8, AlignCenter, AlignCenter, "Grok's");
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 3 + 18, AlignCenter, AlignCenter, "Adventure");
    canvas_draw_str_aligned(canvas, (SCREEN_WIDTH / 2) - 10, SCREEN_HEIGHT / 3 + 33, AlignCenter, AlignCenter, "v");
    canvas_set_font(canvas, FontBigNumbers);
    canvas_draw_str_aligned(canvas, SCREEN_WIDTH / 2, SCREEN_HEIGHT / 3 + 30, AlignCenter, AlignCenter, "3");
    // Simulate wax drip effect using ctx->last_frame_time as seed
    srand(ctx->last_frame_time);
    for(int x = 0; x < SCREEN_WIDTH; x += 4) {
        for(int y = 0; y < SCREEN_HEIGHT; y += 8) {
            if(rand() % 100 < 20) {
                canvas_draw_line(canvas, x, y, x + rand() % 3 - 1, y + rand() % 4 + 1);
            }
        }
    }
}

// Draw game world
static void draw_game(GameContext* ctx, Canvas* canvas) {
    Color bg = ctx->is_night ? ColorBlack : ColorWhite;
    Color fg = ctx->is_night ? ColorWhite : ColorBlack; // White at night, black during day
    canvas_set_color(canvas, bg);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas_set_color(canvas, fg);

    // Draw sky
    if(ctx->is_night) {
        canvas_draw_dot(canvas, ctx->sun_moon_x % SCREEN_WIDTH, 10); // Reduced to 1 star
        if(rand() % 100 < 25) canvas_draw_dot(canvas, (ctx->sun_moon_x % SCREEN_WIDTH) + 20, 12); // 25% chance for second star
        canvas_draw_circle(canvas, ctx->sun_moon_x % SCREEN_WIDTH, 10, 2);
        if(ctx->weather_active && ctx->weather_type == 3) {
            int stars[2][2] = {{20, 10}, {25, 12}}; // Reduced to 2 stars
            for(int j = 0; j < 2; j++) {
                int sx = (stars[j][0] + ctx->sun_moon_x / 2) % SCREEN_WIDTH;
                canvas_draw_dot(canvas, sx, stars[j][1]);
            }
        }
    } else {
        canvas_draw_circle(canvas, ctx->sun_moon_x % SCREEN_WIDTH, 10, 4);
    }

    // Draw weather
    if(ctx->weather_active && ctx->weather_type != 3) {
        int wx = (ctx->sun_moon_x) % SCREEN_WIDTH; // Reduced to 1 particle
        int wy = 10;
        if(ctx->weather_type == 1 && ctx->is_night) {
            canvas_draw_line(canvas, wx, wy, wx + 1, wy + 1);
        } else if(ctx->weather_type == 2 && !ctx->is_night) {
            canvas_draw_dot(canvas, wx, wy);
        }
    }

    // Draw walls
    for(int y = 0; y < PLAY_HEIGHT; y++) {
        for(int x = 0; x < WORLD_WIDTH; x++) {
            int idx = y * WORLD_WIDTH + x;
            if(ctx->world_map[idx] == 'W') {
                canvas_draw_box(canvas, x, y, 1, 1); // Draw walls as 1x1 boxes
            }
        }
    }

    // Draw platforms
    for(int i = 0; i < ctx->platform_count; i++) {
        Platform* p = &ctx->platforms[i];
        int px = p->x;
        if(px + p->width < 0 || px > SCREEN_WIDTH) continue;
        canvas_draw_line(canvas, px + 1, p->y + p->height - 1, px + p->width - 2, p->y + p->height - 1); // Adjusted to exclude 1-pixel borders
        canvas_draw_line(canvas, px + 3, p->y, px + p->width - 4, p->y);
        canvas_draw_line(canvas, px + 1, p->y + p->height - 1, px + 3, p->y + 1);
        canvas_draw_line(canvas, px + p->width - 2, p->y + p->height - 1, px + p->width - 4, p->y + 1);
        for(int py = p->y + 1; py < p->y + p->height - 1; py++) {
            canvas_draw_line(canvas, px + 1, py, px + p->width - 2, py);
        }
    }

    // Draw pickups
    for(int i = 0; i < ctx->pickup_count; i++) {
        Pickup* pk = &ctx->pickups[i];
        int px = pk->x;
        int py = pk->y;
        if(px < 0 || px > SCREEN_WIDTH) continue;
        if(pk->type == PICKUP_ALMOND_WATER) {
            canvas_draw_str(canvas, px, py, "A");
        } else if(pk->type == PICKUP_CHEST) {
            canvas_draw_rbox(canvas, px, py, 4, 4, 2);
            canvas_draw_circle(canvas, px + 2, py + 2, 1);
        } else if(pk->type == PICKUP_DOOR) {
            canvas_draw_rframe(canvas, px, py, 5, 6, 1);
            canvas_set_color(canvas, ColorWhite); // White border
            canvas_draw_line(canvas, px, py, px + 4, py); // Top border
            canvas_draw_line(canvas, px, py + 1, px, py + 5); // Left border
            canvas_draw_line(canvas, px + 4, py + 1, px + 4, py + 5); // Right border
            canvas_set_color(canvas, fg); // Reset to foreground color
        } else if(pk->type == PICKUP_FURNITURE) {
            canvas_draw_str(canvas, px, py, &pk->symbol);
        }
    }

    // Draw entities
    for(int i = -1; i < ctx->entity_count; i++) {
        Entity* e = (i == -1) ? &ctx->player : &ctx->entities[i];
        int ex = e->x;
        int ey = e->y;
        if(ex + e->width < 0 || ex > SCREEN_WIDTH) continue;
        Color entity_color = fg;
        Color border_color = bg;
        if(e == &ctx->player && ctx->special_active && (furi_get_tick() / 100 % 2 == 0)) {
            entity_color = bg;
            border_color = fg;
        }
        if(e->type == ENTITY_PLAYER || e->type == ENTITY_ENEMY) {
            canvas_set_color(canvas, entity_color);
            canvas_draw_box(canvas, ex, ey, e->width, e->height);
            canvas_set_color(canvas, border_color);
            canvas_draw_line(canvas, ex, ey, ex + e->width - 1, ey);
            canvas_draw_line(canvas, ex, ey, ex, ey + e->height - 1);
            canvas_draw_line(canvas, ex + e->width - 1, ey, ex + e->width - 1, ey + e->height - 1);
        } else {
            canvas_set_color(canvas, entity_color);
            canvas_draw_box(canvas, ex, ey, 2, 2);
            int curve_dir = e->facing_right ? 1 : -1;
            canvas_draw_line(canvas, ex + (e->facing_right ? 0 : 1), ey + 2, ex + (e->facing_right ? 1 : 0), ey + 4);
            canvas_draw_line(canvas, ex + (e->facing_right ? 1 : 0), ey + 2, ex + (e->facing_right ? 2 : -1), ey + 4);
            canvas_draw_box(canvas, ex + curve_dir, ey + 5, 2, 2);
            canvas_set_color(canvas, ColorWhite);
            canvas_draw_dot(canvas, ex + 1, ey + 1);
            canvas_draw_dot(canvas, ex + 1 + curve_dir, ey + 1);
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_frame(canvas, ex + 1, ey + 1, 1, 1);
            canvas_draw_frame(canvas, ex + 1 + curve_dir, ey + 1, 1, 1);
            canvas_set_color(canvas, fg);
        }
        // Health bars
        int bar_x = e->facing_right ? ex - 2 : ex + e->width + 1;
        for(int j = 0; j < e->health_bars; j++) {
            canvas_draw_line(canvas, bar_x, ey + j * 2, bar_x + 2, ey + j * 2);
        }
    }

    // Draw projectiles
    if(ctx->player_projectile.active) {
        int px = ctx->player_projectile.x;
        int py = ctx->player_projectile.y;
        canvas_set_color(canvas, fg);
        canvas_draw_box(canvas, px, py, 2, 2);
    }
    for(int i = 0; i < ctx->enemy_projectile_count; i++) {
        if(ctx->enemy_projectiles[i].active) {
            int px = ctx->enemy_projectiles[i].x;
            int py = ctx->enemy_projectiles[i].y;
            canvas_set_color(canvas, fg);
            canvas_draw_box(canvas, px, py, 2, 2);
        }
    }

    // UI with contrasting colors
    canvas_set_color(canvas, fg); // UI text color matches day/night
    if(ctx->settings.fps_display) {
        char fps_str[10];
        snprintf(fps_str, 10, "%.0f", (double)ctx->fps);
        canvas_draw_str(canvas, SCREEN_WIDTH - 21, 11, fps_str);
    }
    char pp_str[20];
    snprintf(pp_str, 20, "PP: %d", ctx->player.power_points);
    canvas_draw_str(canvas, 2, 11, pp_str);
    char aw_str[5];
    snprintf(aw_str, 5, "A: %d", ctx->almond_water_count);
    canvas_draw_str(canvas, 3, SCREEN_HEIGHT / 2, aw_str);
}

// Draw pause
static void draw_pause(GameContext* ctx, Canvas* canvas) {
    draw_game(ctx, canvas);
    canvas_set_color(canvas, ctx->is_night ? ColorWhite : ColorBlack);
    for(int ox = -5; ox <= 5; ox++) {
        for(int oy = -5; oy <= 5; oy++) {
            if(abs(ox) + abs(oy) <= 5) {
                canvas_draw_str(canvas, SCREEN_WIDTH / 2 - 20 + ox, SCREEN_HEIGHT / 2 + oy, "Pause");
            }
        }
    }
    canvas_set_color(canvas, ctx->is_night ? ColorBlack : ColorWhite);
    canvas_draw_str(canvas, SCREEN_WIDTH / 2 - 20, SCREEN_HEIGHT / 2, "Pause");
}

// Draw death
static void draw_death(GameContext* ctx, Canvas* canvas) {
    UNUSED(ctx);
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas_set_color(canvas, ColorWhite);
    canvas_draw_str(canvas, SCREEN_WIDTH / 2 - 20, SCREEN_HEIGHT / 2, "U DED");
}

// Handle input
static void handle_input(GameContext* ctx, InputEvent* input) {
    uint64_t time = furi_get_tick();
    ctx->last_input_time = time;

    if(ctx->state == STATE_TITLE) {
        if(input->type == InputTypePress && input->key == InputKeyOk) {
            ctx->state = STATE_GAME;
            generate_world(ctx);
        }
    } else if(ctx->state == STATE_GAME) {
        if(input->type == InputTypePress || input->type == InputTypeRepeat) {
            int dx = 0, dy = 0;
            bool is_press = (input->type == InputTypePress);
            if(input->key == InputKeyUp) {
                dy = is_press ? -2 : -4; // 2 for press, 4 for hold
                if(is_press) {
                    ctx->hold_up_timer = time;
                    ctx->bypass_collision = true; // Enable bypass on press
                }
            } else if(input->key == InputKeyDown) {
                dy = is_press ? 2 : 4; // 2 for press, 4 for hold
                if(is_press) {
                    ctx->hold_down_timer = time;
                    ctx->bypass_collision = true; // Enable bypass on press
                }
            } else if(input->key == InputKeyLeft) {
                dx = is_press ? -2 : -4; // 2 for press, 4 for hold
                if(is_press) {
                    ctx->hold_left_timer = time;
                    ctx->bypass_collision = true; // Enable bypass on press
                }
            } else if(input->key == InputKeyRight) {
                dx = is_press ? 2 : 4; // 2 for press, 4 for hold
                if(is_press) {
                    ctx->hold_right_timer = time;
                    ctx->bypass_collision = true; // Enable bypass on press
                }
            } else if(input->key == InputKeyOk && !ctx->player_projectile.active) {
                bool hit = false;
                for(int i = 0; i < ctx->entity_count; i++) {
                    Entity* e = &ctx->entities[i];
                    if(abs(ctx->player.x - e->x) <= 3 && abs(ctx->player.y - e->y) <= 3) {
                        int damage = (ctx->player.double_power_timer > time) ? 2 : 1;
                        e->health_bars -= damage;
                        hit = true;
                        if(e->health_bars <= 0) {
                            ctx->player.power_points += (e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) ? 25 : 10;
                            check_level_up(ctx);
                            if(rand() % 100 < 50) {
                                ctx->pickups[ctx->pickup_count].type = (rand() % 4 == 0 ? PICKUP_ALMOND_WATER : PICKUP_CHEST);
                                ctx->pickups[ctx->pickup_count].x = e->x;
                                ctx->pickups[ctx->pickup_count].y = e->y;
                                ctx->pickups[ctx->pickup_count].falling = true;
                                ctx->pickup_count++;
                            }
                            if(e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) {
                                ctx->final_boss_timer = time;
                            }
                        }
                    }
                }
                if(!hit) {
                    ctx->player_projectile.active = true;
                    ctx->player_projectile.x = ctx->player.x + (ctx->player.facing_right ? ctx->player.width : -2);
                    ctx->player_projectile.y = ctx->player.y + ctx->player.height / 2;
                    ctx->player_projectile.dx = ctx->player.facing_right ? 2 : -2;
                    ctx->player_projectile.dy = 0;
                    ctx->player_projectile.from_player = true;
                }
            } else if(input->key == InputKeyBack) {
                if(ctx->hold_back_timer == 0) ctx->hold_back_timer = time;
            }
            if(dx != 0 || dy != 0) {
                if(ctx->player_projectile.active) {
                    ctx->player_projectile.dx = (dx != 0) ? dx * 2 : ctx->player_projectile.dx;
                    ctx->player_projectile.dy = (dy != 0) ? dy * 2 : ctx->player_projectile.dy;
                } else {
                    int new_x = ctx->player.x + dx;
                    int new_y = ctx->player.y + dy;
                    if(ctx->bypass_collision) {
                        // Bypass collision, move until screen edge
                        if(new_x >= 0 && new_x + ctx->player.width <= WORLD_WIDTH) ctx->player.x = new_x;
                        if(new_y >= 0 && new_y + ctx->player.height <= PLAY_HEIGHT) ctx->player.y = new_y;
                        if(dx != 0) ctx->player.facing_right = (dx > 0);
                    } else {
                        if(new_x >= 0 && new_x + ctx->player.width <= WORLD_WIDTH &&
                           new_y >= 0 && new_y + ctx->player.height <= PLAY_HEIGHT &&
                           !collides_with_wall(new_x, new_y, ctx->player.width, ctx->player.height, ctx) &&
                           !collides_with_furniture(new_x, new_y, ctx->player.width, ctx->player.height, ctx) &&
                           (dy <= 0 || can_move_down(new_x, new_y, ctx->player.width, ctx->player.height, ctx))) {
                            ctx->player.x = new_x;
                            ctx->player.y = new_y;
                            if(dx != 0) ctx->player.facing_right = (dx > 0);
                        }
                    }
                }
            }
            // Door interaction
            if(input->key == InputKeyUp) {
                for(int i = 0; i < ctx->pickup_count; i++) {
                    if(ctx->pickups[i].type == PICKUP_DOOR) {
                        int dx = abs(ctx->player.x + ctx->player.width / 2 - (ctx->pickups[i].x + 2));
                        if(dx < 3 && abs(ctx->player.y + ctx->player.height - (ctx->pickups[i].y + 6)) < 1) {
                            canvas_set_color(ctx->canvas, ColorBlack);
                            canvas_draw_box(ctx->canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                            view_port_update(furi_record_open(RECORD_GUI));
                            generate_world(ctx);
                            break;
                        }
                    }
                }
            }
        } else if(input->type == InputTypeRelease) {
            if(input->key == InputKeyUp) {
                ctx->hold_up_timer = 0;
                ctx->bypass_collision = false; // Disable bypass on release
            } else if(input->key == InputKeyDown) {
                ctx->hold_down_timer = 0;
                ctx->bypass_collision = false; // Disable bypass on release
            } else if(input->key == InputKeyLeft) {
                ctx->hold_left_timer = 0;
                ctx->bypass_collision = false; // Disable bypass on release
            } else if(input->key == InputKeyRight) {
                ctx->hold_right_timer = 0;
                ctx->bypass_collision = false; // Disable bypass on release
            } else if(input->key == InputKeyBack) {
                if(time - ctx->hold_back_timer >= 3000) {
                    // Simple save and exit
                    if(save_array(WMA_FILE, ctx->world_map, ARRAY_SIZE) ||
                       save_array(WSO_FILE, ctx->world_objects, ARRAY_SIZE) ||
                       save_settings(ctx) ||
                       save_player_data(ctx)) {
                        // At least one save succeeded, proceed to exit
                    }
                    ctx->state = STATE_EXIT;
                } else {
                    ctx->state = STATE_PAUSED;
                }
                ctx->hold_back_timer = 0;
            }
        }
    } else if(ctx->state == STATE_PAUSED) {
        if(input->type == InputTypePress && (input->key == InputKeyOk || input->key == InputKeyBack)) {
            ctx->state = STATE_GAME;
        }
    } else if(ctx->state == STATE_DEATH) {
        if(time - ctx->last_frame_time > 2000) {
            ctx->player.health_bars = 2;
            ctx->player.x = rand() % (WORLD_WIDTH - ctx->player.width);
            ctx->player.y = rand() % (PLAY_HEIGHT - ctx->player.height);
            ctx->in_iframes = true;
            ctx->iframe_timer = time + 2000;
            ctx->state = STATE_GAME;
        }
    }
}

// Update game state
static void update_game(GameContext* ctx) {
    uint64_t time = furi_get_tick();
    if(time - ctx->last_fps_check > 1000) {
        ctx->fps = (float)ctx->frame_count / ((time - ctx->last_fps_check) / 1000.0f);
        ctx->last_fps_check = time;
        ctx->frame_count = 0;
    }
    if(ctx->state == STATE_TITLE) {
        if(time - ctx->last_frame_time > 2690) {
            ctx->state = STATE_GAME;
            generate_world(ctx);
            view_port_update(furi_record_open(RECORD_GUI)); // Force screen update
        }
    } else if(ctx->state == STATE_GAME) {
        if(time - ctx->last_input_time > 180000) {
            ctx->state = STATE_PAUSED;
        }
        if(time - ctx->day_night_timer > 300000) {
            ctx->is_night = !ctx->is_night;
            ctx->day_night_timer = time;
            ctx->sun_moon_x = 0;
        }
        ctx->sun_moon_x += 1;
        if(ctx->sun_moon_x > WORLD_WIDTH) ctx->sun_moon_x = 0;
        if(time - ctx->weather_timer > 60000ULL + (uint64_t)(rand() % 120000)) {
            ctx->weather_active = !ctx->weather_active;
            ctx->weather_timer = time;
            ctx->weather_duration = 30000 + rand() % 60000;
            ctx->weather_type = rand() % 4;
        }
        if(ctx->weather_active && time - ctx->weather_timer > ctx->weather_duration) {
            ctx->weather_active = false;
            ctx->weather_type = 0;
        }
        // Update pickups (falling)
        for(int i = 0; i < ctx->pickup_count; i++) {
            if(ctx->pickups[i].falling) {
                bool on_platform = false;
                for(int j = 0; j < ctx->platform_count; j++) {
                    if(collides_with_platform(ctx->pickups[i].x, ctx->pickups[i].y + 1, 1, 1, &ctx->platforms[j])) {
                        on_platform = true;
                        break;
                    }
                }
                if(!on_platform) {
                    ctx->pickups[i].y += 1;
                    if(ctx->pickups[i].y > PLAY_HEIGHT) {
                        ctx->pickups[i] = ctx->pickups[--ctx->pickup_count];
                        i--;
                    }
                } else {
                    ctx->pickups[i].falling = false;
                }
            }
        }
        // Player pickup interaction
        for(int i = 0; i < ctx->pickup_count; i++) {
            Pickup* pk = &ctx->pickups[i];
            if(!pk->falling && abs(ctx->player.x - pk->x) < ctx->player.width && abs(ctx->player.y - pk->y) < ctx->player.height) {
                if(pk->type == PICKUP_ALMOND_WATER && ctx->almond_water_count < 3) {
                    ctx->almond_water_count += 1; // Award 1 almond water
                } else if(pk->type == PICKUP_CHEST) {
                    int roll = rand() % 5;
                    if(roll == 0) {
                        if(ctx->almond_water_count < 3) ctx->almond_water_count += 1; // Award 1 almond water
                    } else if(roll == 1) {
                        if(ctx->entity_count < 4) ctx->entity_count++; // Allow up to 4 entities
                    } else if(roll == 2) {
                        ctx->entities[2].health_bars = 10; // Reset boss health
                    } else if(roll == 3) {
                    } else {
                        ctx->player.power_points += 10;
                        check_level_up(ctx);
                    }
                }
                // Remove pickup after collection
                ctx->pickups[i] = ctx->pickups[--ctx->pickup_count];
                i--;
            }
        }
        // Auto-use almond water
        if(ctx->player.health_bars <= 1 && ctx->almond_water_count > 0) {
            ctx->player.health_bars += 1;
            ctx->almond_water_count--;
        }
        // Entity updates
        for(int i = 0; i < ctx->entity_count; i++) {
            Entity* e = &ctx->entities[i];
            int dx = (rand() % 3) - 1;
            int dy = (rand() % 3) - 1;
            int dist = abs(e->x - ctx->player.x) + abs(e->y - ctx->player.y);
            if(dist <= 8) {
                dx = (ctx->player.x > e->x) ? 1 : ((ctx->player.x < e->x) ? -1 : 0);
                dy = (ctx->player.y > e->y) ? 1 : ((ctx->player.y < e->y) ? -1 : 0);
            }
            int new_x = e->x + dx;
            int new_y = e->y + dy;
            if(new_x >= 0 && new_x + e->width <= WORLD_WIDTH &&
               new_y >= 0 && new_y + e->height <= PLAY_HEIGHT &&
               !collides_with_wall(new_x, new_y, e->width, e->height, ctx) &&
               !collides_with_furniture(new_x, new_y, e->width, e->height, ctx) &&
               (dy <= 0 || can_move_down(new_x, new_y, e->width, e->height, ctx))) {
                e->x = new_x;
                e->y = new_y;
                if(dx != 0) e->facing_right = (dx > 0);
            }
            if((e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) && time > e->pickup_cooldown) {
                for(int j = 0; j < ctx->pickup_count; j++) {
                    if(!ctx->pickups[j].falling && abs(e->x - ctx->pickups[j].x) < e->width && abs(e->y - ctx->pickups[j].y) < e->height) {
                        if(ctx->pickups[j].type == PICKUP_ALMOND_WATER) {
                            e->health_bars += 1;
                        } else if(ctx->pickups[j].type == PICKUP_CHEST) {
                            e->double_power_timer = time + 3000;
                            e->pickup_cooldown = time + 6000;
                        }
                        ctx->pickups[j] = ctx->pickups[--ctx->pickup_count];
                        j--;
                    }
                }
            }
            if(rand() % 100 < 5) {
                if(e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) {
                    if(ctx->enemy_projectile_count < 2) {
                        Projectile* proj = &ctx->enemy_projectiles[ctx->enemy_projectile_count++];
                        proj->active = true;
                        proj->x = e->x + (e->facing_right ? e->width : -2);
                        proj->y = e->y + e->height / 2;
                        proj->dx = e->facing_right ? 2 : -2;
                        proj->dy = 0;
                        proj->from_player = false;
                    }
                } else {
                    if(ctx->enemy_projectile_count < 1) {
                        Projectile* proj = &ctx->enemy_projectiles[0];
                        proj->active = true;
                        proj->x = e->x + (e->facing_right ? e->width : -2);
                        proj->y = e->y + e->height / 2;
                        proj->dx = e->facing_right ? 2 : -2;
                        proj->dy = 0;
                        proj->from_player = false;
                        ctx->enemy_projectile_count = 1;
                    }
                }
            }
            if(!ctx->in_iframes && abs(e->x - ctx->player.x) < e->width && abs(e->y - ctx->player.y) < e->height) {
                ctx->player.health_bars--;
                ctx->in_iframes = true;
                ctx->iframe_timer = time + 230;
                if(ctx->player_projectile.active) {
                    ctx->player.health_bars = 2;
                    ctx->special_active = true;
                    ctx->special_timer = time + 2000;
                    ctx->in_iframes = true;
                    ctx->iframe_timer = time + 2000;
                }
                if(ctx->player.health_bars <= 0) {
                    ctx->state = STATE_DEATH;
                    ctx->last_frame_time = time;
                }
            }
        }
        // Projectile updates
        if(ctx->player_projectile.active) {
            ctx->player_projectile.x += ctx->player_projectile.dx;
            ctx->player_projectile.y += ctx->player_projectile.dy;
            if(ctx->player_projectile.x < 0 || ctx->player_projectile.x >= WORLD_WIDTH || ctx->player_projectile.y < 0 || ctx->player_projectile.y >= PLAY_HEIGHT) {
                ctx->player_projectile.active = false;
            } else {
                for(int i = 0; i < ctx->platform_count; i++) {
                    if(collides_with_platform(ctx->player_projectile.x, ctx->player_projectile.y, 2, 2, &ctx->platforms[i])) {
                        ctx->player_projectile.active = false;
                        break;
                    }
                }
                for(int i = 0; i < ctx->entity_count; i++) {
                    Entity* e = &ctx->entities[i];
                    if(abs(ctx->player_projectile.x - e->x) < e->width && abs(ctx->player_projectile.y - e->y) < e->height) {
                        int damage = (ctx->player.double_power_timer > time) ? 2 : 1;
                        e->health_bars -= damage;
                        ctx->player_projectile.active = false;
                        if(e->health_bars <= 0) {
                            ctx->player.power_points += (e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) ? 25 : 10;
                            check_level_up(ctx);
                            if(e->type == ENTITY_BOSS || e->type == ENTITY_FINAL_BOSS) {
                                ctx->final_boss_timer = time;
                            }
                        }
                    }
                }
            }
        }
        for(int i = 0; i < ctx->enemy_projectile_count; i++) {
            if(ctx->enemy_projectiles[i].active) {
                ctx->enemy_projectiles[i].x += ctx->enemy_projectiles[i].dx;
                ctx->enemy_projectiles[i].y += ctx->enemy_projectiles[i].dy;
                if(ctx->enemy_projectiles[i].x < 0 || ctx->enemy_projectiles[i].x >= WORLD_WIDTH || ctx->enemy_projectiles[i].y < 0 || ctx->enemy_projectiles[i].y >= PLAY_HEIGHT) {
                    ctx->enemy_projectiles[i].active = false;
                } else {
                    for(int j = 0; j < ctx->platform_count; j++) {
                        if(collides_with_platform(ctx->enemy_projectiles[i].x, ctx->enemy_projectiles[i].y, 2, 2, &ctx->platforms[j])) {
                            ctx->enemy_projectiles[i].active = false;
                            break;
                        }
                    }
                    if(!ctx->in_iframes && abs(ctx->enemy_projectiles[i].x - ctx->player.x) < ctx->player.width && abs(ctx->enemy_projectiles[i].y - ctx->player.y) < ctx->player.height) {
                        ctx->player.health_bars--;
                        ctx->enemy_projectiles[i].active = false;
                        ctx->in_iframes = true;
                        ctx->iframe_timer = time + 230;
                        if(ctx->player_projectile.active) {
                            ctx->player.health_bars = 2;
                            ctx->special_active = true;
                            ctx->special_timer = time + 2000;
                            ctx->in_iframes = true;
                            ctx->iframe_timer = time + 2000;
                        }
                        if(ctx->player.health_bars <= 0) {
                            ctx->state = STATE_DEATH;
                            ctx->last_frame_time = time;
                        }
                    }
                }
            }
        }
        // Door timer
        if(ctx->pickup_count == 0 && time - ctx->door_timer > 300000) { // 5 minutes
            ctx->pickups[ctx->pickup_count].type = PICKUP_DOOR;
            ctx->pickups[ctx->pickup_count].x = rand() % WORLD_WIDTH;
            ctx->pickups[ctx->pickup_count].y = PLAY_HEIGHT - 6;
            ctx->pickups[ctx->pickup_count].falling = false;
            ctx->pickup_count++;
            ctx->door_timer = time;
        }
        // Timers
        if(ctx->in_iframes && time > ctx->iframe_timer) {
            ctx->in_iframes = false;
        }
        if(ctx->special_active && time > ctx->special_timer) {
            ctx->special_active = false;
        }
        if(time - ctx->final_boss_timer > 540000 && ctx->oset[3] == '1') {
            ctx->oset[3] = 'z';
            ctx->entities[2].type = ENTITY_FINAL_BOSS;
            ctx->entities[2].health_bars = 10;
            ctx->entities[2].x = ctx->player.x + (ctx->player.facing_right ? 20 : -20);
            ctx->entities[2].y = ctx->player.y;
        }
    }
    ctx->last_frame_time = time;
    ctx->frame_count++;
}

// Render callback
static void render_callback(Canvas* canvas, void* context) {
    GameContext* ctx = context;
    ctx->canvas = canvas;
    if(ctx->state == STATE_TITLE) {
        draw_title(ctx, canvas);
    } else if(ctx->state == STATE_GAME) {
        draw_game(ctx, canvas);
    } else if(ctx->state == STATE_PAUSED) {
        draw_pause(ctx, canvas);
    } else if(ctx->state == STATE_DEATH) {
        draw_death(ctx, canvas);
    }
}

// Input callback
static void input_callback(InputEvent* input, void* context) {
    GameContext* ctx = context;
    ctx->input = *input;
    handle_input(ctx, input);
}

// Main entry point
int32_t grokadven_app(void* p) {
    UNUSED(p);
    GameContext* ctx = malloc(sizeof(GameContext));
    if(!ctx) return -1;
    game_init(ctx);

    ViewPort* view_port = view_port_alloc();
    if(!view_port) {
        free(ctx);
        return -1;
    }
    view_port_draw_callback_set(view_port, render_callback, ctx);
    view_port_input_callback_set(view_port, input_callback, ctx);

    Gui* gui = furi_record_open(RECORD_GUI);
    if(!gui) {
        view_port_free(view_port);
        free(ctx);
        return -1;
    }
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    while(ctx->state != STATE_EXIT) {
        furi_delay_ms(1000 / 13);
        update_game(ctx);
        view_port_update(view_port);
    }

    // Cleanup and delay to ensure proper exit
    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    furi_delay_ms(500); // Delay to allow system stabilization
    free(ctx);
    return 0;
}