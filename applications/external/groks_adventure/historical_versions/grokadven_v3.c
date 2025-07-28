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
#define ARRAY_SIZE (SCREEN_WIDTH * SCREEN_HEIGHT + 1)

// Game states
typedef enum {
    STATE_TITLE,
    STATE_MENU,
    STATE_PAUSED,
    STATE_GAME,
    STATE_CREDITS,
    STATE_DEATH,
    STATE_EXIT
} GameState;

// Menu button states
typedef enum {
    MENU_START_GAME,
    MENU_LOAD_GAME,
    MENU_LOAD_DLC,
    MENU_REFRESH,
    MENU_SETTINGS,
    MENU_SURRENDER,
    MENU_RETURN_BATTLE,
    MENU_SAVE_LATER,
    MENU_SLAY_SELF,
    MENU_RELOAD,
    MENU_BUTTONS
} MenuButton;

// Settings options
typedef struct {
    bool dark_mode;
    bool difficulty_easy; // Mutually exclusive with hard
    bool difficulty_hard;
    bool brosiv;         // Mutually exclusive with yosiv
    bool yosiv;
    bool fps_display;
} GameSettings;

// Entity types
typedef enum {
    ENTITY_PLAYER,
    ENTITY_NPC,
    ENTITY_ENEMY,
    ENTITY_BOSS,
    ENTITY_FINAL_BOSS
} EntityType;

// Entity structure
typedef struct {
    EntityType type;
    int x, y;
    int width, height;
    int shared_pixel; // 0: top-left, 1: top-right, 2: bottom-right
    int health_bars;
    int power_points;
    float power_level;
    bool facing_right;
} Entity;

// Button actions
typedef enum {
    ACTION_WALK_UP,
    ACTION_WALK_RIGHT,
    ACTION_WALK_DOWN,
    ACTION_WALK_LEFT,
    ACTION_JUMP_UP,
    ACTION_JUMP_DOWN,
    ACTION_JUMP_RIGHT,
    ACTION_JUMP_LEFT,
    ACTION_ATTACK,
    ACTION_LOW_KICK,
    ACTION_PICKUP,
    ACTION_THROW,
    ACTION_INSPECT,
    ACTION_SPECIAL,
    ACTION_PAUSE,
    ACTION_COUNT
} ActionType;

typedef struct {
    ActionType type;
    char sequence[7]; // Up to 6 presses + null terminator
} ButtonAction;

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
    uint64_t pause_timer;
    uint64_t iframe_timer;
    uint64_t day_night_timer;
    uint64_t weather_timer;
    uint64_t weather_duration;
    uint64_t final_boss_timer;
    bool is_night;
    bool weather_active;
    int weather_type; // 0: none, 1: snow, 2: rain, 3: constellations
    int sun_moon_x;
    int menu_selector;
    int submenu_selector;
    int submenu_scroll;
    bool submenu_active;
    char* submenu_options[ACTION_COUNT + 1]; // +1 for Return
    int submenu_option_count;
    MenuButton current_menu[9];
    int menu_count;
    int menu_page;
    GameSettings settings;
    Entity player;
    Entity entities[3];
    int entity_count;
    float user_speed;
    bool is_running;
    uint64_t run_timer;
    int click_count[4];
    uint64_t click_timer[4];
    int back_click_count;
    uint64_t back_click_timer;
    bool in_iframes;
    char world_map[ARRAY_SIZE];
    char char_placement[ARRAY_SIZE];
    char world_objects[ARRAY_SIZE];
    char oset[5];
    char last_save[5];
    int save_count;
    int save_attempts;
    bool up_pressed;
    bool ok_pressed;
    bool back_pressed;
    ButtonAction actions[ACTION_COUNT];
    int structure_x, structure_y;
    int projectile_x, projectile_y;
    bool projectile_active;
    int respawn_x, respawn_y;
    uint32_t world_seed;
} GameContext;

// Placeholder sprites
const uint8_t player_sprite[] = {0x00, 0x00}; // 2x2
const uint8_t enemy_sprite[] = {0x00, 0x00}; // 2x2
const uint8_t boss_sprite[] = {0x00, 0x00}; // 4x3
const uint8_t projectile_sprite[] = {0x00, 0x00}; // 1x1
const uint8_t chest_sprite[] = {0x00, 0x00}; // 2x2
const uint8_t almond_water_sprite[] = {0x00, 0x00}; // 1x1
const uint8_t furniture_sprite[] = {0x00, 0x00}; // 3x3
const uint8_t door_sprite[] = {0x00, 0x00}; // 2x3

// File paths
#define STORAGE_PATH "/ext/grokadven/"
#define WMA_FILE STORAGE_PATH "WMA_arr.txt"
#define CPA_FILE STORAGE_PATH "CPA_arr.txt"
#define WSO_FILE STORAGE_PATH "WSO_arr.txt"
#define SETTINGS_FILE STORAGE_PATH "settings.bin"

// Initialize arrays
void init_arrays(GameContext* ctx) {
    memset(ctx->world_map, 0, ARRAY_SIZE);
    memset(ctx->char_placement, 0, ARRAY_SIZE);
    memset(ctx->world_objects, 0, ARRAY_SIZE);
    ctx->world_map[(ARRAY_SIZE-1)/2] = '~'; // Respawn point
    ctx->char_placement[(ARRAY_SIZE-1)/2] = '1';
    strcpy(ctx->oset, "000Z");
    ctx->world_seed = 0;
}

// Save array to file
bool save_array(const char* filename, const char* data, size_t size) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
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
    File* file = storage_file_alloc(storage);
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
    File* file = storage_file_alloc(storage);
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
    File* file = storage_file_alloc(storage);
    bool success = storage_file_open(file, SETTINGS_FILE, FSAM_READ, FSOM_OPEN_EXISTING);
    if(success) {
        storage_file_read(file, &ctx->settings, sizeof(GameSettings));
    }
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

// Initialize button actions
void init_actions(GameContext* ctx) {
    const char* action_names[ACTION_COUNT] = {
        "Walk Up", "Walk Right", "Walk Down", "Walk Left",
        "Jump Up", "Jump Down", "Jump Right", "Jump Left",
        "Attack", "Low Kick", "Pickup", "Throw", "Inspect", "Special", "Pause"
    };
    const char* action_sequences[ACTION_COUNT] = {
        "U", "R", "D", "L",
        "UUUUU", "DDDDD", "RRRRR", "LLLLL",
        "O", "DO", "B", "UO", "OO", "BOOO", "BBBBBB"
    };
    for(int i = 0; i < ACTION_COUNT; i++) {
        ctx->actions[i].type = (ActionType)i;
        strncpy(ctx->actions[i].sequence, action_sequences[i], 7);
        ctx->submenu_options[i] = (char*)action_names[i];
    }
    ctx->submenu_options[ACTION_COUNT] = "Return";
}

// Initialize game context
static void game_init(GameContext* ctx) {
    ctx->state = STATE_TITLE;
    ctx->frame_count = 0;
    ctx->fps = 0.0f;
    ctx->last_frame_time = furi_get_tick();
    ctx->last_fps_check = ctx->last_frame_time;
    ctx->last_input_time = ctx->last_frame_time;
    ctx->pause_timer = 0;
    ctx->iframe_timer = 0;
    ctx->day_night_timer = ctx->last_frame_time;
    ctx->weather_timer = ctx->last_frame_time;
    ctx->weather_duration = 0;
    ctx->final_boss_timer = 0;
    ctx->weather_active = false;
    ctx->weather_type = 0;
    ctx->is_night = true;
    ctx->sun_moon_x = 0;
    ctx->menu_selector = 0;
    ctx->submenu_selector = 0;
    ctx->submenu_scroll = 0;
    ctx->submenu_active = false;
    ctx->submenu_option_count = 0;
    ctx->menu_page = 0;
    ctx->settings.dark_mode = true;
    ctx->settings.difficulty_hard = true;
    ctx->settings.difficulty_easy = false;
    ctx->settings.yosiv = true;
    ctx->settings.brosiv = false;
    ctx->settings.fps_display = true;
    ctx->player.type = ENTITY_PLAYER;
    ctx->player.x = SCREEN_WIDTH / 2;
    ctx->player.y = SCREEN_HEIGHT - 7;
    ctx->player.width = 2;
    ctx->player.height = 2;
    ctx->player.shared_pixel = 1;
    ctx->player.health_bars = 2;
    ctx->player.power_points = 0;
    ctx->player.power_level = 0.0f;
    ctx->player.facing_right = false;
    ctx->respawn_x = ctx->player.x;
    ctx->respawn_y = ctx->player.y;
    ctx->user_speed = 4.0f;
    ctx->is_running = false;
    ctx->run_timer = 0;
    ctx->in_iframes = false;
    ctx->projectile_active = false;
    ctx->back_click_count = 0;
    ctx->back_click_timer = 0;
    memset(ctx->click_count, 0, sizeof(ctx->click_count));
    memset(ctx->click_timer, 0, sizeof(ctx->click_timer));
    memset(ctx->last_save, 0, sizeof(ctx->last_save));
    ctx->save_count = 0;
    ctx->save_attempts = 0;
    ctx->up_pressed = false;
    ctx->ok_pressed = false;
    ctx->back_pressed = false;
    init_arrays(ctx);
    init_actions(ctx);
    ctx->entity_count = 3;
    ctx->entities[0].type = ENTITY_NPC;
    ctx->entities[0].x = SCREEN_WIDTH / 4;
    ctx->entities[0].y = SCREEN_HEIGHT - 7;
    ctx->entities[0].width = 2;
    ctx->entities[0].height = 2;
    ctx->entities[0].shared_pixel = 2;
    ctx->entities[0].health_bars = 1;
    ctx->entities[0].power_points = 0;
    ctx->entities[0].power_level = 0.0f;
    ctx->entities[0].facing_right = true;
    ctx->entities[1].type = ENTITY_NPC;
    ctx->entities[1].x = SCREEN_WIDTH * 3 / 4;
    ctx->entities[1].y = SCREEN_HEIGHT - 7;
    ctx->entities[1].width = 2;
    ctx->entities[1].height = 2;
    ctx->entities[1].shared_pixel = 2;
    ctx->entities[1].health_bars = 1;
    ctx->entities[1].power_points = 0;
    ctx->entities[1].power_level = 0.0f;
    ctx->entities[1].facing_right = false;
    ctx->entities[2].type = ENTITY_ENEMY;
    ctx->entities[2].x = SCREEN_WIDTH / 2 + 20;
    ctx->entities[2].y = SCREEN_HEIGHT - 7;
    ctx->entities[2].width = 2;
    ctx->entities[2].height = 2;
    ctx->entities[2].shared_pixel = 1;
    ctx->entities[2].health_bars = 3;
    ctx->entities[2].power_points = 0;
    ctx->entities[2].power_level = 0.0f;
    ctx->entities[2].facing_right = true;
    ctx->structure_x = SCREEN_WIDTH / 2 - 10;
    ctx->structure_y = SCREEN_HEIGHT - 10;
    if(load_settings(ctx)) {
        if(!ctx->settings.difficulty_easy && !ctx->settings.difficulty_hard) ctx->settings.difficulty_hard = true;
        if(!ctx->settings.brosiv && !ctx->settings.yosiv) ctx->settings.yosiv = true;
    }
}

// Generate world elements
static void generate_world(GameContext* ctx) {
    ctx->world_seed++;
    srand(ctx->world_seed);
    for(int i = 0; i < ARRAY_SIZE - 1; i++) {
        if(ctx->world_map[i] == 0) {
            int roll = rand() % 100;
            if(roll < 5) ctx->world_map[i] = 'W'; // Wall
            else if(roll < 10) ctx->world_map[i] = 'A'; // Almond water
            else if(roll < 12) ctx->world_map[i] = 'D'; // Door
        }
        if(ctx->world_objects[i] == 0 && rand() % 100 < 3) {
            ctx->world_objects[i] = 'F'; // Furniture
        }
    }
    for(int i = 0; i < ctx->entity_count; i++) {
        if(ctx->entities[i].type == ENTITY_NPC || ctx->entities[i].type == ENTITY_ENEMY) {
            int dx = (rand() % 3) - 1;
            int dy = (rand() % 3) - 1;
            int new_x = ctx->entities[i].x + dx;
            int new_y = ctx->entities[i].y + dy;
            if(new_x >= 0 && new_x < SCREEN_WIDTH - ctx->entities[i].width &&
               new_y >= 0 && new_y <= SCREEN_HEIGHT - 7) {
                ctx->entities[i].x = new_x;
                ctx->entities[i].y = new_y;
            }
        }
    }
}

// Draw title screen
static void draw_title(GameContext* ctx, Canvas* canvas) {
    uint64_t time = furi_get_tick();
    float progress = (float)(time - ctx->last_frame_time) / 10000.0f;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas_set_color(canvas, ColorWhite);

    // Scrolling lore
    if(progress < 1.0f) {
        canvas_set_font(canvas, FontSecondary);
        int y_offset = (int)(SCREEN_HEIGHT + 100 - (progress * (SCREEN_HEIGHT + 100)));
        const char* lore = "In the endless Backrooms, Grok wanders the liminal halls of flickering fluorescent lights and damp corridors. Trapped in a maze of old offices and radioactive waterways, survival depends on almond water, the life-giving elixir. Shadows cast by unseen windows reveal a sun and moon, a cruel illusion of freedom. Entities lurk in the corners, and the final boss, a monstrous presence, awaits to challenge Grok's resolve.";
        char line[32];
        int line_count = 0;
        int pos = 0;
        int y = y_offset;
        while(lore[pos] && y < SCREEN_HEIGHT) {
            int len = 0;
            while(lore[pos + len] && lore[pos + len] != '\n' && len < 31) len++;
            strncpy(line, lore + pos, len);
            line[len] = '\0';
            if(y >= -8) canvas_draw_str(canvas, 10, y, line);
            pos += len + (lore[pos + len] == '\n' ? 1 : 0);
            y += 10;
            line_count++;
        }
        canvas_draw_rframe(canvas, SCREEN_WIDTH/2 - 31, SCREEN_HEIGHT - 8, 62, 8, 1);
        canvas_draw_str(canvas, SCREEN_WIDTH/2 - 30, SCREEN_HEIGHT - 2, "Press OK to Continue");
        if(ctx->input.type == InputTypePress && ctx->input.key == InputKeyOk) {
            ctx->state = STATE_MENU;
            ctx->last_frame_time = time;
        }
    } else {
        // Title screen
        canvas_set_font(canvas, FontBigNumbers);
        int title_width = (int)(SCREEN_WIDTH * 0.48f);
        int title_x = (SCREEN_WIDTH - title_width) / 2;
        canvas_draw_rbox(canvas, title_x, 5, title_width, 15, 3);
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, title_x + 5, 18, "Grok Adventure");
        canvas_set_color(canvas, ColorBlack);
        if(progress >= 1.1f) {
            canvas_set_font(canvas, FontSecondary);
            canvas_draw_str(canvas, SCREEN_WIDTH/2 - 40, 24, "A Grok made mini-Adventure");
        }
        if(progress >= 1.2f) {
            canvas_draw_str(canvas, SCREEN_WIDTH/2 - 30, SCREEN_HEIGHT - 2, "  OK -> START");
        }
        if(ctx->input.type == InputTypePress) {
            if(ctx->input.key == InputKeyOk || progress >= 1.2f) {
                ctx->state = STATE_MENU;
                ctx->last_frame_time = time;
            } else {
                ctx->last_frame_time = (uint64_t)((1.2f - progress) * 10000.0f);
            }
        }
    }
}

// Draw menu
static void draw_menu(GameContext* ctx, Canvas* canvas) {
    uint64_t time = furi_get_tick();
    canvas_set_color(canvas, ctx->settings.dark_mode ? ColorBlack : ColorWhite);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas_set_color(canvas, ctx->settings.dark_mode ? ColorWhite : ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, 9);
    canvas_set_color(canvas, ctx->settings.dark_mode ? ColorBlack : ColorWhite);
    canvas_draw_str(canvas, 2, 8, ctx->state == STATE_PAUSED ? "Paused" : "Home Menu");
    canvas_set_color(canvas, ctx->settings.dark_mode ? ColorWhite : ColorBlack);

    const char* menu_labels[] = {
        [MENU_START_GAME] = "Start Game",
        [MENU_LOAD_GAME] = "Load Game",
        [MENU_LOAD_DLC] = "Load DLC",
        [MENU_REFRESH] = "Refresh",
        [MENU_SETTINGS] = "Settings",
        [MENU_SURRENDER] = "Surrender",
        [MENU_RETURN_BATTLE] = "Return to Battle",
        [MENU_SAVE_LATER] = "Save for Later",
        [MENU_SLAY_SELF] = "Slay Thy Self",
        [MENU_RELOAD] = "Re-Load",
        [MENU_BUTTONS] = "Buttons"
    };
    if(ctx->state == STATE_MENU) {
        ctx->menu_count = 9;
        if(ctx->menu_page == 0) {
            ctx->current_menu[0] = MENU_START_GAME;
            ctx->current_menu[1] = MENU_LOAD_GAME;
            ctx->current_menu[2] = MENU_REFRESH;
            ctx->current_menu[3] = MENU_SETTINGS;
            ctx->current_menu[4] = MENU_SURRENDER;
            ctx->current_menu[5] = MENU_BUTTONS;
        } else {
            ctx->current_menu[0] = MENU_BUTTONS;
            ctx->current_menu[1] = MENU_START_GAME;
            ctx->current_menu[2] = MENU_LOAD_GAME;
            ctx->current_menu[3] = MENU_REFRESH;
            ctx->current_menu[4] = MENU_SETTINGS;
            ctx->current_menu[5] = MENU_SURRENDER;
        }
    } else {
        ctx->menu_count = 9;
        if(ctx->menu_page == 0) {
            ctx->current_menu[0] = MENU_RETURN_BATTLE;
            ctx->current_menu[1] = MENU_SAVE_LATER;
            ctx->current_menu[2] = MENU_SLAY_SELF;
            ctx->current_menu[3] = MENU_RELOAD;
            ctx->current_menu[4] = MENU_SETTINGS;
            ctx->current_menu[5] = MENU_SURRENDER;
        } else {
            ctx->current_menu[0] = MENU_BUTTONS;
            ctx->current_menu[1] = MENU_RETURN_BATTLE;
            ctx->current_menu[2] = MENU_SAVE_LATER;
            ctx->current_menu[3] = MENU_SLAY_SELF;
            ctx->current_menu[4] = MENU_RELOAD;
            ctx->current_menu[5] = MENU_SETTINGS;
        }
    }

    int column_width = (int)(SCREEN_WIDTH * 0.565f); // 13% more space
    for(int col = 0; col < 2; col++) {
        int x = 10 + col * (column_width + 5);
        for(int row = 0; row < 3; row++) {
            int idx = col * 3 + row;
            if(idx >= ctx->menu_count) break;
            int y = 20 + row * 15;
            if(idx == ctx->menu_selector) {
                canvas_draw_rframe(canvas, x - 2, y - 11, strlen(menu_labels[ctx->current_menu[idx]]) * 6 + 4, 11, 2);
                canvas_draw_str(canvas, x - 6, y - 2, ">");
            }
            canvas_draw_str(canvas, x, y, menu_labels[ctx->current_menu[idx]]);
            canvas_draw_line(canvas, x, y + 3, x + strlen(menu_labels[ctx->current_menu[idx]]) * 6, y + 3);
        }
    }

    if(ctx->submenu_active) {
        int submenu_x = 0;
        int submenu_width = SCREEN_WIDTH;
        canvas_set_color(canvas, ctx->settings.dark_mode ? ColorBlack : ColorWhite);
        canvas_draw_box(canvas, submenu_x, 10, submenu_width, SCREEN_HEIGHT - 10);
        canvas_set_color(canvas, ctx->settings.dark_mode ? ColorWhite : ColorBlack);
        if(ctx->submenu_option_count == 0) {
            canvas_set_font(canvas, FontPrimary);
            canvas_draw_str(canvas, submenu_x + 10, SCREEN_HEIGHT/2, "N/A");
            canvas_set_font(canvas, FontSecondary);
            if(time - ctx->last_input_time > 3000) {
                ctx->submenu_active = false;
            }
        } else {
            int display_count = ctx->submenu_option_count > 4 ? 4 : ctx->submenu_option_count;
            bool show_up_arrow = ctx->submenu_scroll > 0;
            bool show_down_arrow = ctx->submenu_scroll + display_count < ctx->submenu_option_count;
            if(show_up_arrow) {
                canvas_draw_triangle(canvas, SCREEN_WIDTH/2, 12, 4, 4, CanvasDirectionBottomToTop);
                if(ctx->submenu_selector == -1) {
                    canvas_draw_box(canvas, SCREEN_WIDTH/2 - 2, 10, 5, 5);
                }
            }
            if(show_down_arrow) {
                canvas_draw_triangle(canvas, SCREEN_WIDTH/2, SCREEN_HEIGHT - 12, 4, 4, CanvasDirectionTopToBottom);
                if(ctx->submenu_selector == display_count) {
                    canvas_draw_box(canvas, SCREEN_WIDTH/2 - 2, SCREEN_HEIGHT - 14, 5, 5);
                }
            }
            for(int i = 0; i < display_count; i++) {
                int idx = ctx->submenu_scroll + i;
                int y = 20 + i * 15;
                if(i == ctx->submenu_selector && idx != ACTION_COUNT) {
                    canvas_draw_str(canvas, submenu_x + 2, y + 3, ">");
                }
                canvas_draw_str(canvas, submenu_x + 8, y, ctx->submenu_options[idx]);
                if(idx < ACTION_COUNT && ctx->current_menu[ctx->menu_selector] == MENU_BUTTONS && ctx->state == STATE_MENU) {
                    for(int j = 0; j < 6; j++) {
                        int seq_x = submenu_x + 70 + j * 10;
                        if(ctx->submenu_selector == i && ctx->actions[idx].sequence[j] != '\0') {
                            canvas_draw_rframe(canvas, seq_x - 1, y - 3, 8, 8, 1);
                        }
                        char seq[2] = {ctx->actions[idx].sequence[j], '\0'};
                        if(seq[0] == '\0') seq[0] = '_';
                        canvas_draw_str(canvas, seq_x, y + 5, seq);
                        canvas_draw_line(canvas, seq_x, y + 7, seq_x + 6, y + 7);
                    }
                } else if(idx < ACTION_COUNT && ctx->current_menu[ctx->menu_selector] == MENU_SETTINGS) {
                    int slider_x = submenu_x + submenu_width - 20;
                    bool is_on = false;
                    if(idx == 0) is_on = ctx->settings.dark_mode;
                    else if(idx == 1) is_on = ctx->settings.difficulty_easy;
                    else if(idx == 2) is_on = ctx->settings.difficulty_hard;
                    else if(idx == 3) is_on = ctx->settings.brosiv;
                    else if(idx == 4) is_on = ctx->settings.yosiv;
                    else if(idx == 5) is_on = ctx->settings.fps_display;
                    canvas_draw_circle(canvas, slider_x + (is_on ? 5 : -5), y + 3, 3);
                    if(!is_on) {
                        canvas_set_color(canvas, ctx->settings.dark_mode ? ColorBlack : ColorWhite);
                        canvas_draw_circle(canvas, slider_x - 5, y + 3, 2);
                        canvas_set_color(canvas, ctx->settings.dark_mode ? ColorWhite : ColorBlack);
                    }
                    if(i == ctx->submenu_selector && is_on != (slider_x == (slider_x + 5))) {
                        canvas_set_color(canvas, ctx->settings.dark_mode ? ColorWhite : ColorBlack);
                        canvas_draw_circle(canvas, slider_x + (is_on ? 5 : -5), y + 3, 2);
                        canvas_set_color(canvas, ctx->settings.dark_mode ? ColorBlack : ColorWhite);
                        canvas_draw_circle(canvas, slider_x + (is_on ? -5 : 5), y + 3, 2);
                        canvas_set_color(canvas, ctx->settings.dark_mode ? ColorWhite : ColorBlack);
                    }
                }
            }
        }
    }
}

// Draw game world
static void draw_game(GameContext* ctx, Canvas* canvas) {
    canvas_set_color(canvas, ctx->is_night ? ColorBlack : ColorWhite);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 7);
    canvas_set_color(canvas, ctx->is_night ? ColorWhite : ColorBlack);
    canvas_draw_line(canvas, 0, SCREEN_HEIGHT - 7, SCREEN_WIDTH, SCREEN_HEIGHT - 7);

    // Draw sky with Backrooms windows
    if(ctx->is_night) {
        for(int i = 0; i < 10; i++) {
            canvas_draw_dot(canvas, rand() % SCREEN_WIDTH, rand() % (SCREEN_HEIGHT - 7));
        }
        canvas_draw_circle(canvas, ctx->sun_moon_x, 10, 2);
        if(ctx->weather_active && ctx->weather_type == 3) {
            int stars[7][2] = {{20, 10}, {25, 12}, {30, 10}, {35, 15}, {40, 10}, {45, 12}, {50, 10}};
            if(rand() % 2) {
                stars[0][0] = 60; stars[0][1] = 15;
                stars[1][0] = 65; stars[1][1] = 15;
                stars[2][0] = 70; stars[2][1] = 15;
                stars[3][0] = 65; stars[3][1] = 10;
                stars[4][0] = 65; stars[4][1] = 20;
            }
            for(int i = 0; i < (rand() % 2 ? 3 : 7); i++) {
                int x = (stars[i][0] + ctx->sun_moon_x / 2) % SCREEN_WIDTH;
                canvas_draw_line(canvas, x - 2, stars[i][1], x + 2, stars[i][1]);
                canvas_draw_line(canvas, x, stars[i][1] - 2, x, stars[i][1] + 2);
            }
        }
    } else {
        for(int i = 0; i < 3; i++) {
            int x = rand() % (SCREEN_WIDTH - 9) + 5;
            int w = rand() % 5 + 5;
            canvas_draw_rbox(canvas, x, 5 + i * 5, w, 3, 1);
        }
        canvas_set_color(canvas, ColorBlack);
        canvas_draw_circle(canvas, ctx->sun_moon_x, 10, 4);
        canvas_set_color(canvas, ctx->is_night ? ColorWhite : ColorBlack);
    }

    // Draw weather
    if(ctx->weather_active && ctx->weather_type != 3) {
        for(int i = 0; i < 5; i++) {
            int x = rand() % SCREEN_WIDTH;
            int y = rand() % (SCREEN_HEIGHT - 7);
            if(ctx->weather_type == 1 && ctx->is_night) {
                canvas_set_color(canvas, ColorWhite);
                canvas_draw_line(canvas, x, y, x + 1, y + 1);
            } else if(ctx->weather_type == 2 && !ctx->is_night) {
                canvas_set_color(canvas, ColorBlack);
                canvas_draw_dot(canvas, x, y);
            }
        }
        canvas_set_color(canvas, ctx->is_night ? ColorWhite : ColorBlack);
    }

    // Draw platform
    canvas_set_color(canvas, ctx->is_night ? ColorWhite : ColorBlack);
    canvas_draw_box(canvas, ctx->structure_x, ctx->structure_y, 20, 3);
    canvas_set_color(canvas, ctx->is_night ? ColorBlack : ColorWhite);
    canvas_draw_line(canvas, ctx->structure_x, ctx->structure_y - 1, ctx->structure_x + 20, ctx->structure_y - 1);

    // Draw floor
    canvas_set_color(canvas, ctx->is_night ? ColorWhite : ColorBlack);
    canvas_draw_box(canvas, 0, SCREEN_HEIGHT - 6, SCREEN_WIDTH, 6);

    // Draw projectile
    if(ctx->projectile_active) {
        canvas_draw_box(canvas, ctx->projectile_x, ctx->projectile_y, 1, 1);
    }

    // Draw entities
    for(int i = -1; i < ctx->entity_count; i++) {
        Entity* e = i == -1 ? &ctx->player : &ctx->entities[i];
        int x = e->x, y = e->y, w = e->width, h = e->height;
        if(e->shared_pixel == 0) {
            canvas_draw_box(canvas, x, y, w, h);
        } else if(e->shared_pixel == 1) {
            canvas_draw_box(canvas, x - w + 1, y, w, h);
        } else {
            canvas_draw_box(canvas, x - w + 1, y - h + 1, w, h);
        }
        int bar_x = e->facing_right ? x - 2 : x + w + 1;
        int bar_h = e->health_bars == 1 ? h : h * (e->health_bars - 1);
        if(bar_h > 0) {
            canvas_draw_line(canvas, bar_x, y - bar_h + 1, bar_x, y);
        }
        for(int j = 1; j < e->health_bars; j++) {
            int dot_y = y - j * 2;
            canvas_draw_dot(canvas, bar_x + 1, dot_y);
            if(e->type != ENTITY_BOSS && e->type != ENTITY_FINAL_BOSS) {
                canvas_set_color(canvas, ctx->is_night ? ColorBlack : ColorWhite);
                canvas_draw_dot(canvas, bar_x + 2, dot_y);
                canvas_set_color(canvas, ctx->is_night ? ColorWhite : ColorBlack);
            }
        }
    }

    // FPS counter
    if(ctx->settings.fps_display && ctx->state == STATE_GAME) {
        char fps_str[3];
        snprintf(fps_str, 3, "%d", (int)ctx->fps);
        canvas_draw_str(canvas, SCREEN_WIDTH - 10, 5, fps_str);
    }
}

// Draw credits
static void draw_credits(GameContext* ctx, Canvas* canvas) {
    uint64_t time = furi_get_tick();
    float progress = (float)(time - ctx->last_frame_time) / 10000.0f;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    canvas_set_color(canvas, ColorWhite);
    int y_offset = (int)(SCREEN_HEIGHT + 100 - (progress * (SCREEN_HEIGHT + 100)));
    const char* credits = "\n\nGrokAdven\n\n\n\nIdea by: Z0MBI3D\n\ncode generated by:\nGrok AI\nxAI -> Grok 3\n\n\n\nSpecial Thanks\nThe Squ33\n\n";
    char line[32];
    int pos = 0;
    int y = y_offset;
    while(credits[pos] && y < SCREEN_HEIGHT) {
        int len = 0;
        while(credits[pos + len] && credits[pos + len] != '\n' && len < 31) len++;
        strncpy(line, credits + pos, len);
        line[len] = '\0';
        if(y >= -8 && line[0] != '\0') canvas_draw_str(canvas, 10, y, line);
        pos += len + (credits[pos + len] == '\n' ? 1 : 0);
        y += (line[0] == '\0' && credits[pos] == '\n' ? 14 : 10);
    }
    if(progress >= 1.0f) {
        canvas_draw_str(canvas, 10, SCREEN_HEIGHT/2, "Thank you. Enjoy your day.");
        if(time - ctx->last_frame_time > 3140) {
            ctx->state = STATE_EXIT;
        }
    }
}

// Draw death screen
static void draw_death(GameContext* ctx, Canvas* canvas) {
    uint64_t time = furi_get_tick();
    float progress = (float)(time - ctx->last_frame_time) / 2000.0f;
    canvas_set_color(canvas, ColorBlack);
    canvas_draw_box(canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
    if(progress < 1.0f) {
        canvas_set_color(canvas, ColorWhite);
        canvas_draw_str(canvas, SCREEN_WIDTH/2 - 20, SCREEN_HEIGHT/2, "U DED");
    } else {
        ctx->player.x = ctx->respawn_x;
        ctx->player.y = ctx->respawn_y;
        ctx->player.health_bars = 2;
        ctx->state = STATE_GAME;
        ctx->last_frame_time = time;
    }
}

// Handle input
static void handle_input(GameContext* ctx, InputEvent* input) {
    uint64_t time = furi_get_tick();
    ctx->last_input_time = time;

    if(input->type == InputTypePress || input->type == InputTypeLong) {
        if(input->key == InputKeyUp) ctx->up_pressed = true;
        else if(input->key == InputKeyOk) ctx->ok_pressed = true;
        else if(input->key == InputKeyBack) ctx->back_pressed = true;
    } else if(input->type == InputTypeRelease) {
        if(input->key == InputKeyUp) ctx->up_pressed = false;
        else if(input->key == InputKeyOk) ctx->ok_pressed = false;
        else if(input->key == InputKeyBack) ctx->back_pressed = false;
    }

    if(ctx->state == STATE_TITLE) {
        if(input->type == InputTypePress && ctx->input.key == InputKeyOk) {
            ctx->state = STATE_MENU;
            ctx->last_frame_time = time;
        }
    } else if(ctx->state == STATE_MENU || ctx->state == STATE_PAUSED) {
        if(input->type == InputTypePress) {
            if(!ctx->submenu_active) {
                if(input->key == InputKeyUp) {
                    ctx->menu_selector = (ctx->menu_selector - 1 + ctx->menu_count) % ctx->menu_count;
                } else if(input->key == InputKeyDown) {
                    ctx->menu_selector = (ctx->menu_selector + 1) % ctx->menu_count;
                } else if(input->key == InputKeyRight) {
                    ctx->menu_page = (ctx->menu_page - 1 + 2) % 2;
                    ctx->menu_selector = 0;
                } else if(input->key == InputKeyLeft) {
                    ctx->menu_page = (ctx->menu_page + 1) % 2;
                    ctx->menu_selector = 0;
                } else if(input->key == InputKeyOk) {
                    MenuButton selected = ctx->current_menu[ctx->menu_selector];
                    if(selected == MENU_START_GAME || selected == MENU_RETURN_BATTLE) {
                        ctx->state = STATE_GAME;
                        ctx->day_night_timer = time;
                        ctx->player.x = ctx->respawn_x;
                        ctx->player.y = ctx->respawn_y;
                        save_array(WMA_FILE, ctx->world_map, ARRAY_SIZE);
                        save_array(CPA_FILE, ctx->char_placement, ARRAY_SIZE);
                        save_array(WSO_FILE, ctx->world_objects, ARRAY_SIZE);
                        save_settings(ctx);
                    } else if(selected == MENU_LOAD_GAME) {
                        ctx->submenu_active = true;
                        ctx->submenu_selector = 0;
                        ctx->submenu_scroll = 0;
                        ctx->submenu_option_count = 0;
                    } else if(selected == MENU_LOAD_DLC) {
                        ctx->submenu_active = true;
                        ctx->submenu_selector = 0;
                        ctx->submenu_scroll = 0;
                        ctx->submenu_option_count = 0;
                    } else if(selected == MENU_REFRESH) {
                        canvas_set_color(ctx->canvas, ColorBlack);
                        canvas_draw_box(ctx->canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                        view_port_update(furi_record_open(RECORD_GUI));
                        furi_delay_ms(500);
                        ctx->state = STATE_MENU;
                    } else if(selected == MENU_SETTINGS) {
                        ctx->submenu_active = true;
                        ctx->submenu_selector = 0;
                        ctx->submenu_scroll = 0;
                        ctx->submenu_option_count = 7;
                        ctx->submenu_options[0] = "Dark Mode";
                        ctx->submenu_options[1] = "Difficulty Easy";
                        ctx->submenu_options[2] = "Difficulty Hard";
                        ctx->submenu_options[3] = "Brosiv";
                        ctx->submenu_options[4] = "Yosiv";
                        ctx->submenu_options[5] = "FPS";
                        ctx->submenu_options[6] = "Return";
                    } else if(selected == MENU_SURRENDER) {
                        canvas_set_color(ctx->canvas, ColorWhite);
                        canvas_draw_box(ctx->canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                        view_port_update(furi_record_open(RECORD_GUI));
                        ctx->state = STATE_CREDITS;
                        ctx->last_frame_time = time;
                    } else if(selected == MENU_SAVE_LATER) {
                        char filename[32];
                        snprintf(filename, 32, STORAGE_PATH "GA%04d_gg.txt", ctx->save_count);
                        bool success = save_array(WMA_FILE, ctx->world_map, ARRAY_SIZE) &&
                                       save_array(CPA_FILE, ctx->char_placement, ARRAY_SIZE) &&
                                       save_array(WSO_FILE, ctx->world_objects, ARRAY_SIZE) &&
                                       save_settings(ctx);
                        if(success) {
                            strncpy(ctx->last_save, filename + 15, 4);
                            ctx->save_count = (ctx->save_count + 1) % 10;
                            ctx->save_attempts = 0;
                            ctx->state = STATE_GAME;
                        } else {
                            ctx->save_attempts++;
                            if(ctx->save_attempts >= 3) {
                                ctx->submenu_active = true;
                                ctx->submenu_selector = 0;
                                ctx->submenu_scroll = 0;
                                ctx->submenu_option_count = 4;
                                ctx->submenu_options[0] = "Game Saves Are Failing.";
                                ctx->submenu_options[1] = "Please Try Again Later";
                                ctx->submenu_options[2] = "Press Any Button to Play";
                                ctx->submenu_options[3] = "Return";
                            }
                        }
                    } else if(selected == MENU_SLAY_SELF) {
                        ctx->state = STATE_DEATH;
                        ctx->last_frame_time = time;
                    } else if(selected == MENU_RELOAD) {
                        if(ctx->last_save[0] != '\0') {
                            canvas_set_color(ctx->canvas, ColorBlack);
                            canvas_draw_box(ctx->canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                            canvas_set_color(ctx->canvas, ColorWhite);
                            canvas_draw_str(ctx->canvas, SCREEN_WIDTH/2 - 20, SCREEN_HEIGHT/2, "Loading...");
                            view_port_update(furi_record_open(RECORD_GUI));
                            load_array(WMA_FILE, ctx->world_map, ARRAY_SIZE);
                            load_array(CPA_FILE, ctx->char_placement, ARRAY_SIZE);
                            load_array(WSO_FILE, ctx->world_objects, ARRAY_SIZE);
                            load_settings(ctx);
                            ctx->player.x = ctx->respawn_x;
                            ctx->player.y = ctx->respawn_y;
                            ctx->player.health_bars = 2;
                            ctx->player.power_points = 0;
                            ctx->player.power_level = 0.0f;
                            ctx->state = STATE_GAME;
                        }
                    } else if(selected == MENU_BUTTONS) {
                        ctx->submenu_active = true;
                        ctx->submenu_selector = 0;
                        ctx->submenu_scroll = 0;
                        ctx->submenu_option_count = ACTION_COUNT + 1;
                    }
                } else if(input->key == InputKeyBack && ctx->back_click_count == 1 && time - ctx->back_click_timer < 1600) {
                    ctx->submenu_active = false;
                    ctx->back_click_count = 0;
                    save_settings(ctx);
                } else if(input->key == InputKeyBack) {
                    ctx->back_click_count++;
                    ctx->back_click_timer = time;
                }
            } else {
                if(input->key == InputKeyUp && ctx->submenu_selector > -1) {
                    ctx->submenu_selector--;
                    if(ctx->submenu_selector < 0 && ctx->submenu_scroll > 0) {
                        ctx->submenu_scroll--;
                    }
                } else if(input->key == InputKeyDown && ctx->submenu_selector < 3) {
                    if(ctx->submenu_scroll + ctx->submenu_selector + 1 < ctx->submenu_option_count) {
                        ctx->submenu_selector++;
                    }
                    if(ctx->submenu_selector == 4 && ctx->submenu_scroll + 4 < ctx->submenu_option_count) {
                        ctx->submenu_scroll++;
                        ctx->submenu_selector = 3;
                    }
                } else if(input->key == InputKeyOk) {
                    int idx = ctx->submenu_scroll + ctx->submenu_selector;
                    if(idx == ACTION_COUNT || (ctx->current_menu[ctx->menu_selector] == MENU_SAVE_LATER && idx == 3)) {
                        ctx->submenu_active = false;
                        save_settings(ctx);
                    } else if(ctx->current_menu[ctx->menu_selector] == MENU_SETTINGS && idx < 6) {
                        bool* setting = NULL;
                        if(idx == 0) setting = &ctx->settings.dark_mode;
                        else if(idx == 1) setting = &ctx->settings.difficulty_easy;
                        else if(idx == 2) setting = &ctx->settings.difficulty_hard;
                        else if(idx == 3) setting = &ctx->settings.brosiv;
                        else if(idx == 4) setting = &ctx->settings.yosiv;
                        else if(idx == 5) setting = &ctx->settings.fps_display;
                        if(setting) {
                            *setting = !*setting;
                            if(idx == 1 && *setting) ctx->settings.difficulty_hard = false;
                            if(idx == 2 && *setting) ctx->settings.difficulty_easy = false;
                            if(idx == 3 && *setting) ctx->settings.yosiv = false;
                            if(idx == 4 && *setting) ctx->settings.brosiv = false;
                            if(idx == 1 && !ctx->settings.difficulty_easy && !ctx->settings.difficulty_hard) ctx->settings.difficulty_hard = true;
                            if(idx == 3 && !ctx->settings.brosiv && !ctx->settings.yosiv) ctx->settings.yosiv = true;
                            save_settings(ctx);
                        }
                    } else if(ctx->current_menu[ctx->menu_selector] == MENU_BUTTONS && ctx->state == STATE_MENU && idx < ACTION_COUNT) {
                        char* seq = ctx->actions[idx].sequence;
                        int pos = 0;
                        for(int i = 0; seq[i] != '\0' && seq[i] != '_'; i++) pos = i + 1;
                        if(pos < 6) {
                            const char* options = "DULROB2369_";
                            int opt_idx = strchr(options, seq[pos] ? seq[pos] : '_') - options;
                            seq[pos] = options[(opt_idx + 1) % strlen(options)];
                            if(seq[pos] == '_') seq[pos] = '\0';
                        }
                    } else if(ctx->current_menu[ctx->menu_selector] == MENU_SAVE_LATER && idx == 2) {
                        ctx->submenu_active = false;
                        ctx->state = STATE_GAME;
                    }
                } else if(input->key == InputKeyLeft || input->key == InputKeyRight) {
                    int idx = ctx->submenu_scroll + ctx->submenu_selector;
                    if(ctx->current_menu[ctx->menu_selector] == MENU_BUTTONS && idx < ACTION_COUNT && ctx->state == STATE_MENU) {
                        char* seq = ctx->actions[idx].sequence;
                        int pos = 0;
                        for(int i = 0; seq[i] != '\0' && seq[i] != '_'; i++) pos = i + 1;
                        if(input->key == InputKeyRight && pos < 6) {
                            seq[pos] = '_';
                        } else if(input->key == InputKeyLeft && pos > 0) {
                            seq[pos-1] = '\0';
                        }
                    }
                }
            }
        }
    } else if(ctx->state == STATE_GAME) {
        if(input->type == InputTypePress) {
            int dir = -1;
            if(input->key == InputKeyUp && strcmp(ctx->actions[ACTION_WALK_UP].sequence, "U") == 0) dir = 0;
            else if(input->key == InputKeyRight && strcmp(ctx->actions[ACTION_WALK_RIGHT].sequence, "R") == 0) dir = 1;
            else if(input->key == InputKeyDown && strcmp(ctx->actions[ACTION_WALK_DOWN].sequence, "D") == 0) dir = 2;
            else if(input->key == InputKeyLeft && strcmp(ctx->actions[ACTION_WALK_LEFT].sequence, "L") == 0) dir = 3;
            else if(input->key == InputKeyBack && strcmp(ctx->actions[ACTION_PAUSE].sequence, "BBBBBB") == 0) {
                ctx->back_click_count++;
                ctx->back_click_timer = time;
            } else if(input->key == InputKeyOk && strcmp(ctx->actions[ACTION_ATTACK].sequence, "O") == 0 && !ctx->projectile_active) {
                bool enemy_near = false;
                for(int i = 0; i < ctx->entity_count; i++) {
                    if(ctx->entities[i].type == ENTITY_ENEMY || ctx->entities[i].type == ENTITY_BOSS || ctx->entities[i].type == ENTITY_FINAL_BOSS) {
                        int dx = ctx->player.x - ctx->entities[i].x;
                        int dy = ctx->player.y - ctx->entities[i].y;
                        if(abs(dx) <= 3 && abs(dy) <= 3 && ((ctx->player.facing_right && dx > 0) || (!ctx->player.facing_right && dx < 0))) {
                            enemy_near = true;
                            break;
                        }
                    }
                }
                if(!enemy_near) {
                    ctx->projectile_active = true;
                    ctx->projectile_x = ctx->player.x + (ctx->player.facing_right ? ctx->player.width : -1);
                    ctx->projectile_y = ctx->player.y;
                }
            }
            if(ctx->back_click_count >= 6 && time - ctx->back_click_timer < 1800) {
                ctx->state = STATE_PAUSED;
                ctx->back_click_count = 0;
            }
            if(dir >= 0) {
                ctx->click_count[dir]++;
                ctx->click_timer[dir] = time;
                if(ctx->click_count[dir] >= 5 && time - ctx->click_timer[dir] < 1600) {
                    if(dir == 0) {
                        ctx->player.y -= 4;
                        if(ctx->player.y < 0) ctx->player.y = 0;
                    } else if(dir == 2) {
                        ctx->player.y += 4;
                        if(ctx->player.y > SCREEN_HEIGHT - 7) ctx->player.y = SCREEN_HEIGHT - 7;
                    } else {
                        int new_x = ctx->player.x + (dir == 1 ? 4 : -4);
                        if(new_x < 4 || new_x > SCREEN_WIDTH - 5) {
                            canvas_set_color(ctx->canvas, ColorBlack);
                            canvas_draw_box(ctx->canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
                            view_port_update(furi_record_open(RECORD_GUI));
                            furi_delay_ms(100);
                            ctx->player.x = (dir == 1 ? 1 : SCREEN_WIDTH - 2);
                            ctx->structure_x = SCREEN_WIDTH / 2 - 10;
                            canvas_set_color(ctx->canvas, ctx->is_night ? ColorBlack : ColorWhite);
                            canvas_draw_box(ctx->canvas, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT - 7);
                            view_port_update(furi_record_open(RECORD_GUI));
                        } else {
                            ctx->player.x = new_x;
                        }
                    }
                    ctx->click_count[dir] = 0;
                } else if(ctx->player.facing_right != (dir == 1)) {
                    ctx->player.facing_right = (dir == 1);
                    ctx->player.shared_pixel = dir == 1 ? 0 : 1;
                } else {
                    float speed = (3 * ctx->user_speed - 11) / 10.0f;
                    if(speed < 0.1f) speed = 0.1f;
                    int move = (int)(3 * speed);
                    if(time - ctx->click_timer[dir] > 1890) {
                        move *= 3;
                        ctx->is_running = true;
                    }
                    if(dir == 0) ctx->player.y -= move;
                    else if(dir == 1) ctx->player.x += move;
                    else if(dir == 2) ctx->player.y += move;
                    else if(dir == 3) ctx->player.x -= move;
                    if(ctx->player.x < 0) ctx->player.x = 0;
                    if(ctx->player.x > SCREEN_WIDTH - ctx->player.width) ctx->player.x = SCREEN_WIDTH - ctx->player.width;
                    if(ctx->player.y < 0) ctx->player.y = 0;
                    if(ctx->player.y > SCREEN_HEIGHT - 7) ctx->player.y = SCREEN_HEIGHT - 7;
                }
            } else if(input->key == InputKeyBack && !ctx->in_iframes && strcmp(ctx->actions[ACTION_PAUSE].sequence, "BBBBBB") != 0) {
                ctx->in_iframes = true;
                ctx->iframe_timer = time;
            }
        } else if(input->type == InputTypeLong && input->key == InputKeyBack && time - ctx->last_input_time > 1000 && !ctx->in_iframes) {
            ctx->in_iframes = true;
            ctx->iframe_timer = time;
        }
    } else if(ctx->state == STATE_DEATH && input->type == InputTypePress) {
        ctx->player.x = ctx->respawn_x;
        ctx->player.y = ctx->respawn_y;
        ctx->player.health_bars = 2;
        ctx->state = STATE_GAME;
        ctx->last_frame_time = time;
    }
}

// Update game state
static void update_game(GameContext* ctx) {
    uint64_t time = furi_get_tick();
    if(ctx->settings.fps_display && ctx->state == STATE_GAME && time - ctx->last_fps_check > 91) {
        ctx->fps = 1000.0f / (time - ctx->last_frame_time);
        ctx->last_fps_check = time;
    }
    if(ctx->state == STATE_GAME) {
        if(time - ctx->last_input_time > 180000) {
            ctx->state = STATE_TITLE;
            ctx->last_frame_time = time;
        }
        if(time - ctx->day_night_timer > 300000) {
            ctx->is_night = !ctx->is_night;
            ctx->day_night_timer = time;
            ctx->sun_moon_x = 0;
        }
        ctx->sun_moon_x = (int)((float)(time - ctx->day_night_timer) / 300000.0f * SCREEN_WIDTH);
        if(ctx->sun_moon_x > SCREEN_WIDTH) ctx->sun_moon_x = SCREEN_WIDTH;
        uint64_t weather_check = (uint64_t)(rand() % 120000 + 60000);
        if(time - ctx->weather_timer > weather_check) {
            ctx->weather_active = !ctx->weather_active;
            ctx->weather_timer = time;
            ctx->weather_duration = (uint64_t)(rand() % 60000 + 30000);
            ctx->weather_type = ctx->is_night ? (rand() % 2 ? 1 : 3) : 2;
        }
        if(ctx->weather_active && time - ctx->weather_timer > ctx->weather_duration) {
            ctx->weather_active = false;
            ctx->weather_type = 0;
        }
        if(ctx->projectile_active) {
            ctx->projectile_x += ctx->player.facing_right ? 2 : -2;
            if(ctx->projectile_x < 0 || ctx->projectile_x >= SCREEN_WIDTH) {
                ctx->projectile_active = false;
            } else {
                for(int i = 0; i < ctx->entity_count; i++) {
                    if(ctx->entities[i].type == ENTITY_ENEMY || ctx->entities[i].type == ENTITY_BOSS || ctx->entities[i].type == ENTITY_FINAL_BOSS) {
                        if(abs(ctx->projectile_x - ctx->entities[i].x) <= ctx->entities[i].width &&
                           abs(ctx->projectile_y - ctx->entities[i].y) <= ctx->entities[i].height) {
                            ctx->entities[i].health_bars--;
                            ctx->projectile_active = false;
                            if(ctx->entities[i].health_bars <= 0 && ctx->entities[i].type == ENTITY_FINAL_BOSS) {
                                ctx->oset[3] = '1';
                                ctx->final_boss_timer = time;
                            }
                        }
                    }
                }
            }
        }
        if(ctx->oset[3] == '1' && time - ctx->final_boss_timer > 540000) {
            ctx->oset[3] = 'z';
            ctx->entities[2].type = ENTITY_FINAL_BOSS;
            ctx->entities[2].health_bars = 5;
            ctx->entities[2].x = SCREEN_WIDTH / 2 + 20;
            ctx->entities[2].y = SCREEN_HEIGHT - 7;
            ctx->entities[2].width = 4;
            ctx->entities[2].height = 3;
        }
        for(int i = 0; i < ctx->entity_count; i++) {
            if(ctx->entities[i].power_points >= 19) {
                ctx->entities[i].power_level += 0.5f;
                ctx->entities[i].power_points = 0;
                if((int)ctx->entities[i].power_level % 9 == 0 && ctx->entities[i].height < 4) {
                    ctx->entities[i].height++;
                }
                if((int)ctx->entities[i].power_level % 18 == 0 && ctx->entities[i].width < 3) {
                    ctx->entities[i].width++;
                }
            }
        }
        if(ctx->player.power_points >= 19) {
            ctx->player.power_level += 0.5f;
            ctx->player.power_points = 0;
            if((int)ctx->player.power_level % 9 == 0 && ctx->player.height < 4) {
                ctx->player.height++;
            }
            if((int)ctx->player.power_level % 18 == 0 && ctx->player.width < 3) {
                ctx->player.width++;
            }
        }
        generate_world(ctx);
    }
    if(ctx->in_iframes && time - ctx->iframe_timer > 230) {
        ctx->in_iframes = false;
    }
    if(time - ctx->back_click_timer > 1800) {
        ctx->back_click_count = 0;
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
    } else if(ctx->state == STATE_MENU || ctx->state == STATE_PAUSED) {
        draw_menu(ctx, canvas);
    } else if(ctx->state == STATE_GAME) {
        draw_game(ctx, canvas);
    } else if(ctx->state == STATE_CREDITS) {
        draw_credits(ctx, canvas);
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
int32_t grokadven_v3_app(void* p) {
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

    view_port_enabled_set(view_port, false);
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);
    free(ctx);
    return 0;
}