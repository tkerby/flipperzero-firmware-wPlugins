#include <game/draw.h>

// Global variables to store camera position
int camera_x = 0;
int camera_y = 0;

// Background rendering function (no collision detection)
void draw_background(Canvas *canvas, Vector pos)
{
    // Clear the canvas
    canvas_clear(canvas);

    // Calculate camera offset to center the player
    camera_x = pos.x - (SCREEN_WIDTH / 2);
    camera_y = pos.y - (SCREEN_HEIGHT / 2);

    // Clamp camera position to prevent showing areas outside the world
    camera_x = CLAMP(camera_x, WORLD_WIDTH - SCREEN_WIDTH, 0);
    camera_y = CLAMP(camera_y, WORLD_HEIGHT - SCREEN_HEIGHT, 0);

    // Draw the outer bounds adjusted by camera offset
    draw_bounds(canvas);
}

// Draw the user stats (health, xp, and level)
void draw_user_stats(Canvas *canvas, Vector pos, GameManager *manager)
{
    GameContext *game_context = game_manager_game_context_get(manager);
    PlayerContext *player = game_context->player_context;

    // first draw a black rectangle to make the text more readable
    canvas_invert_color(canvas);
    canvas_draw_box(canvas, pos.x - 1, pos.y - 7, 34, 21);
    canvas_invert_color(canvas);

    char health[32];
    char xp[32];
    char level[32];

    snprintf(health, sizeof(health), "HP : %ld", player->health);
    snprintf(xp, sizeof(xp), "XP : %ld", player->xp);
    snprintf(level, sizeof(level), "LVL: %ld", player->level);

    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    canvas_draw_str(canvas, pos.x, pos.y, health);
    canvas_draw_str(canvas, pos.x, pos.y + 7, xp);
    canvas_draw_str(canvas, pos.x, pos.y + 14, level);
}

void draw_username(Canvas *canvas, Vector pos, char *username)
{
    // first draw a black rectangle to make the text more readable
    // draw box around the username
    canvas_invert_color(canvas);
    canvas_draw_box(canvas, pos.x - camera_x - (strlen(username) * 2) - 1, pos.y - camera_y - 14, strlen(username) * 4 + 1, 8);
    canvas_invert_color(canvas);

    // draw username over player's head
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    canvas_draw_str(canvas, pos.x - camera_x - (strlen(username) * 2), pos.y - camera_y - 7, username);
}

// Draw a line of icons (16 width)
void draw_icon_line(Canvas *canvas, Vector pos, int amount, bool horizontal, const Icon *icon)
{
    for (int i = 0; i < amount; i++)
    {
        if (horizontal)
        {
            // check if element is outside the world
            if (pos.x + (i * 17) > WORLD_WIDTH)
            {
                break;
            }

            canvas_draw_icon(canvas, pos.x + (i * 17) - camera_x, pos.y - camera_y, icon);
        }
        else
        {
            // check if element is outside the world
            if (pos.y + (i * 17) > WORLD_HEIGHT)
            {
                break;
            }

            canvas_draw_icon(canvas, pos.x - camera_x, pos.y + (i * 17) - camera_y, icon);
        }
    }
}
char g_temp_spawn_name[32];
// Draw an icon at a specific position (with collision detection)
void spawn_icon(Level *level, const char *icon_id, float x, float y)
{
    snprintf(g_temp_spawn_name, sizeof(g_temp_spawn_name), "%s", icon_id);
    Entity *e = level_add_entity(level, &icon_desc);
    entity_pos_set(e, (Vector){x, y});
}
// Draw a line of icons at a specific position (with collision detection)
void spawn_icon_line(Level *level, const char *icon_id, float x, float y, uint8_t amount, bool horizontal)
{
    for (int i = 0; i < amount; i++)
    {
        if (horizontal)
        {
            // check if element is outside the world
            if (x + (i * 17) > WORLD_WIDTH)
            {
                break;
            }

            spawn_icon(level, icon_id, x + (i * 17), y);
        }
        else
        {
            // check if element is outside the world
            if (y + (i * 17) > WORLD_HEIGHT)
            {
                break;
            }

            spawn_icon(level, icon_id, x, y + (i * 17));
        }
    }
}