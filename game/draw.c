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
char g_name[32];
// Draw an icon at a specific position (with collision detection)
void spawn_icon(Level *level, const char *icon_id, float x, float y)
{
    snprintf(g_name, sizeof(g_name), "%s", icon_id);
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

static void draw_menu(GameManager *manager, Canvas *canvas)
{
    GameContext *game_context = game_manager_game_context_get(manager);

    // draw background rectangle
    canvas_draw_icon(
        canvas,
        0,
        0,
        &I_icon_menu_128x64px);

    // draw menu options
    switch (game_context->menu_screen)
    {
    case GAME_MENU_INFO:
        // draw info
        // first option is highlighted
        char health[32];
        char xp[32];
        char level[32];
        char strength[32];

        snprintf(level, sizeof(level), "Level   : %ld", game_context->player_context->level);
        snprintf(health, sizeof(health), "Health  : %ld", game_context->player_context->health);
        snprintf(xp, sizeof(xp), "XP      : %ld", game_context->player_context->xp);
        snprintf(strength, sizeof(strength), "Strength: %ld", game_context->player_context->strength);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 7, 16, game_context->player_context->username);
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        canvas_draw_str(canvas, 7, 30, level);
        canvas_draw_str(canvas, 7, 37, health);
        canvas_draw_str(canvas, 7, 44, xp);
        canvas_draw_str(canvas, 7, 51, strength);

        // draw a box around the selected option
        canvas_draw_frame(canvas, 80, 18, 36, 30);
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 86, 30, "Info");
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 86, 42, "More");
        break;
    case GAME_MENU_MORE:
        // draw settings
        switch (game_context->menu_selection)
        {
        case 0:
            // first option is highlighted
            break;
        case 1:
            // second option is highlighted
            break;
        default:
            break;
        }

        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 7, 16, "FlipWorld v0.4");
        canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
        canvas_draw_str_multi(canvas, 7, 25, "Developed by\nJBlanked and Derek \nJamison. Graphics\nfrom Pr3!\n\nwww.github.com/jblanked");

        // draw a box around the selected option
        canvas_draw_frame(canvas, 80, 18, 36, 30);
        canvas_set_font(canvas, FontSecondary);
        canvas_draw_str(canvas, 86, 30, "Info");
        canvas_set_font(canvas, FontPrimary);
        canvas_draw_str(canvas, 86, 42, "More");
        break;
    default:
        break;
    }
}

static void background_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    if (!self || !context || !canvas || !manager)
        return;

    GameContext *game_context = game_manager_game_context_get(manager);

    // get player position
    Vector posi = entity_pos_get(game_context->player);

    // draw username over player's head
    draw_username(canvas, posi, game_context->player_context->username);

    // draw switch world icon
    if (game_context->is_switching_level)
    {
        canvas_draw_icon(
            canvas,
            0,
            0,
            &I_icon_world_change_128x64px);
    }

    // draw menu
    if (game_context->is_menu_open)
    {
        draw_menu(manager, canvas);
    }
};

// -------------- Entity description --------------
const EntityDescription background_desc = {
    .start = NULL,
    .stop = NULL,
    .update = NULL,
    .render = background_render,
    .collision = NULL,
    .event = NULL,
    .context_size = 0,
};

// we can return the same entity description for all backgrounds
// since they all have the same rendering function
Entity *add_background(Level *level)
{
    return level_add_entity(level, &background_desc);
}
