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
// Draw a half section of icons (16 width)
void draw_icon_half_world(Canvas *canvas, bool right, const Icon *icon)
{
    for (int i = 0; i < 10; i++)
    {
        if (right)
        {
            draw_icon_line(canvas, (Vector){WORLD_WIDTH / 2 + 6, i * 19 + 2}, 11, true, icon);
        }
        else
        {
            draw_icon_line(canvas, (Vector){0, i * 19 + 2}, 11, true, icon);
        }
    }
}
// Draw an icon at a specific position (with collision detection)
void spawn_icon(Level *level, const Icon *icon, float x, float y, uint8_t width, uint8_t height)
{
    Entity *e = level_add_entity(level, &icon_desc);
    IconContext *icon_ctx = entity_context_get(e);
    icon_ctx->icon = icon;
    icon_ctx->width = width;
    icon_ctx->height = height;
    // Set the entity position to the center of the icon
    entity_pos_set(e, (Vector){x + 8, y + 8});
}
// Draw a line of icons at a specific position (with collision detection)
void spawn_icon_line(Level *level, const Icon *icon, float x, float y, uint8_t width, uint8_t height, uint8_t amount, bool horizontal)
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

            spawn_icon(level, icon, x + (i * 17), y, width, height);
        }
        else
        {
            // check if element is outside the world
            if (y + (i * 17) > WORLD_HEIGHT)
            {
                break;
            }

            spawn_icon(level, icon, x, y + (i * 17), width, height);
        }
    }
}