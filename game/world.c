#include <game/world.h>

void draw_bounds(Canvas *canvas)
{
    // Draw the outer bounds adjusted by camera offset
    // we draw this last to ensure users can see the bounds
    canvas_draw_frame(canvas, -camera_x, -camera_y, WORLD_WIDTH, WORLD_HEIGHT);
}

bool draw_json_world(Level *level, const char *json_data)
{
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        char *data = get_json_array_value("json_data", i, json_data);
        if (data == NULL)
        {
            break;
        }
        char *icon = get_json_value("icon", data);
        char *x = get_json_value("x", data);
        char *y = get_json_value("y", data);
        char *width = get_json_value("width", data);
        char *height = get_json_value("height", data);
        char *amount = get_json_value("amount", data);
        char *horizontal = get_json_value("horizontal", data);
        if (icon == NULL || x == NULL || y == NULL || width == NULL || height == NULL || amount == NULL || horizontal == NULL)
        {
            return false;
        }
        // if amount is less than 2, we spawn a single icon
        if (atoi(amount) < 2)
        {
            spawn_icon(level, get_icon(icon), atoi(x), atoi(y), atoi(width), atoi(height));
            free(data);
            free(icon);
            free(x);
            free(y);
            free(width);
            free(height);
            free(amount);
            free(horizontal);
            continue;
        }
        spawn_icon_line(level, get_icon(icon), atoi(x), atoi(y), atoi(width), atoi(height), atoi(amount), strcmp(horizontal, "true") == 0);
        free(data);
        free(icon);
        free(x);
        free(y);
        free(width);
        free(height);
        free(amount);
        free(horizontal);
    }
    return true;
}
bool draw_json_world_furi(Level *level, FuriString *json_data)
{
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        char *data = get_json_array_value("json_data", i, furi_string_get_cstr(json_data));
        if (data == NULL)
        {
            break;
        }
        char *icon = get_json_value("icon", data);
        char *x = get_json_value("x", data);
        char *y = get_json_value("y", data);
        char *width = get_json_value("width", data);
        char *height = get_json_value("height", data);
        char *amount = get_json_value("amount", data);
        char *horizontal = get_json_value("horizontal", data);
        if (icon == NULL || x == NULL || y == NULL || width == NULL || height == NULL || amount == NULL || horizontal == NULL)
        {
            return false;
        }
        // if amount is less than 2, we spawn a single icon
        if (atoi(amount) < 2)
        {
            spawn_icon(level, get_icon(icon), atoi(x), atoi(y), atoi(width), atoi(height));
            free(data);
            free(icon);
            free(x);
            free(y);
            free(width);
            free(height);
            free(amount);
            free(horizontal);
            continue;
        }
        spawn_icon_line(level, get_icon(icon), atoi(x), atoi(y), atoi(width), atoi(height), atoi(amount), strcmp(horizontal, "true") == 0);
        free(data);
        free(icon);
        free(x);
        free(y);
        free(width);
        free(height);
        free(amount);
        free(horizontal);
    }
    return true;
}

void draw_example_world(Level *level)
{
    spawn_icon(level, get_icon("earth"), 112, 56, 15, 16);
    spawn_icon(level, get_icon("home"), 128, 24, 15, 16);
    spawn_icon(level, get_icon("info"), 144, 24, 15, 16);
    spawn_icon(level, get_icon("man"), 160, 56, 7, 16);
    spawn_icon(level, get_icon("woman"), 168, 56, 9, 16);
    spawn_icon(level, get_icon("plant"), 168, 32, 16, 16);
}

void draw_tree_world(Level *level)
{
    // Spawn two full left/up tree lines
    for (int i = 0; i < 2; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, get_icon("tree"), 5, 2 + i * 17, 16, 16, 22, true);
        // Vertical line of 11 icons
        spawn_icon_line(level, get_icon("tree"), 5 + i * 17, 2, 16, 16, 11, false);
    }

    // Spawn two full down tree lines
    for (int i = 9; i < 11; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, get_icon("tree"), 5, 2 + i * 17, 16, 16, 22, true);
    }

    // Spawn two full right tree lines
    for (int i = 20; i < 22; i++)
    {
        // Vertical line of 8 icons starting further down (y=50)
        spawn_icon_line(level, get_icon("tree"), 5 + i * 17, 50, 16, 16, 8, false);
    }

    // Labyrinth lines
    // Third line (14 left, then a gap, then 3 middle)
    spawn_icon_line(level, get_icon("tree"), 5, 2 + 2 * 17, 16, 16, 14, true);
    spawn_icon_line(level, get_icon("tree"), 5 + 16 * 17, 2 + 2 * 17, 16, 16, 3, true);

    // Fourth line (3 left, 6 middle, 4 right)
    spawn_icon_line(level, get_icon("tree"), 5, 2 + 3 * 17, 16, 16, 3, true);           // 3 left
    spawn_icon_line(level, get_icon("tree"), 5 + 7 * 17, 2 + 3 * 17, 16, 16, 6, true);  // 6 middle
    spawn_icon_line(level, get_icon("tree"), 5 + 15 * 17, 2 + 3 * 17, 16, 16, 4, true); // 4 right

    // Fifth line (6 left, 7 middle)
    spawn_icon_line(level, get_icon("tree"), 5, 2 + 4 * 17, 16, 16, 6, true);
    spawn_icon_line(level, get_icon("tree"), 5 + 7 * 17, 2 + 4 * 17, 16, 16, 7, true);

    // Sixth line (5 left, 3 middle, 7 right)
    spawn_icon_line(level, get_icon("tree"), 5, 2 + 5 * 17, 16, 16, 5, true);           // 5 left
    spawn_icon_line(level, get_icon("tree"), 5 + 7 * 17, 2 + 5 * 17, 16, 16, 3, true);  // 3 middle
    spawn_icon_line(level, get_icon("tree"), 5 + 15 * 17, 2 + 5 * 17, 16, 16, 7, true); // 7 right

    // Seventh line (0 left, 7 middle, 4 right)
    spawn_icon_line(level, get_icon("tree"), 5 + 6 * 17, 2 + 6 * 17, 16, 16, 7, true);  // 7 middle
    spawn_icon_line(level, get_icon("tree"), 5 + 14 * 17, 2 + 6 * 17, 16, 16, 4, true); // 4 right

    // Eighth line (4 left, 3 middle, 4 right)
    spawn_icon_line(level, get_icon("tree"), 5, 2 + 7 * 17, 16, 16, 4, true);           // 4 left
    spawn_icon_line(level, get_icon("tree"), 5 + 7 * 17, 2 + 7 * 17, 16, 16, 3, true);  // 3 middle
    spawn_icon_line(level, get_icon("tree"), 5 + 15 * 17, 2 + 7 * 17, 16, 16, 4, true); // 4 right

    // Ninth line (3 left, 1 middle, 3 right)
    spawn_icon_line(level, get_icon("tree"), 5, 2 + 8 * 17, 16, 16, 3, true);           // 3 left
    spawn_icon_line(level, get_icon("tree"), 5 + 5 * 17, 2 + 8 * 17, 16, 16, 1, true);  // 1 middle
    spawn_icon_line(level, get_icon("tree"), 5 + 11 * 17, 2 + 8 * 17, 16, 16, 3, true); // 3 right
}