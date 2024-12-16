#include <draw/world.h>

void draw_bounds(Canvas *canvas)
{
    // Draw the outer bounds adjusted by camera offset
    // we draw this last to ensure users can see the bounds
    canvas_draw_frame(canvas, -camera_x, -camera_y, WORLD_WIDTH, WORLD_HEIGHT);
}

void draw_example_world(Level *level)
{
    spawn_icon(level, &I_icon_earth, 112, 56, 15, 16);
    spawn_icon(level, &I_icon_home, 128, 24, 15, 16);
    spawn_icon(level, &I_icon_info, 144, 24, 15, 16);
    spawn_icon(level, &I_icon_man, 160, 56, 7, 16);
    spawn_icon(level, &I_icon_woman, 168, 56, 9, 16);
    spawn_icon(level, &I_icon_plant, 168, 32, 16, 16);
}

/* JSON of the draw_example_world with fields icon, x, y, width, height
{
    "name": "Example World",
    "author": "JBlanked",
    "json_data": [
        {
            "icon": "earth",
            "x": 112,
            "y": 56,
            "width": 15,
            "height": 16
        },
        {
            "icon": "home",
            "x": 128,
            "y": 24,
            "width": 15,
            "height": 16
        },
        {
            "icon": "info",
            "x": 144,
            "y": 24,
            "width": 15,
            "height": 16
        },
        {
            "icon": "man",
            "x": 160,
            "y": 56,
            "width": 7,
            "height": 16
        },
        {
            "icon": "woman",
            "x": 168,
            "y": 56,
            "width": 9,
            "height": 16
        },
        {
            "icon": "plant",
            "x": 168,
            "y": 32,
            "width": 16,
            "height": 16
        }
    ]
}

*/

bool draw_json_world(Level *level, FuriString *json_data)
{
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        char *data = get_json_array_value("json_data", i, (char *)furi_string_get_cstr(json_data), MAX_WORLD_TOKENS);
        if (data == NULL)
        {
            break;
        }
        char *icon = get_json_value("icon", data, 64);
        char *x = get_json_value("x", data, 64);
        char *y = get_json_value("y", data, 64);
        char *width = get_json_value("width", data, 64);
        char *height = get_json_value("height", data, 64);
        if (icon == NULL || x == NULL || y == NULL || width == NULL || height == NULL)
        {
            return false;
        }
        spawn_icon(level, get_icon(icon), atoi(x), atoi(y), atoi(width), atoi(height));
    }
    return true;
}

void draw_tree_world(Level *level)
{
    // Spawn two full left/up tree lines
    for (int i = 0; i < 2; i++)
    {
        for (int j = 0; j < 22; j++)
        {
            spawn_icon(level, &I_icon_tree, 5 + j * 17, 2 + i * 17, 16, 16); // Horizontal lines
        }
        for (int j = 0; j < 11; j++)
        {
            spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + j * 17, 16, 16); // Vertical lines
        }
    }

    // Spawn two full down tree lines
    for (int i = 9; i < 11; i++)
    {
        for (int j = 0; j < 22; j++)
        {
            spawn_icon(level, &I_icon_tree, 5 + j * 17, 2 + i * 17, 16, 16); // Horizontal lines
        }
    }

    // Spawn two full right tree lines
    for (int i = 20; i < 22; i++)
    {
        for (int j = 0; j < 8; j++)
        {
            spawn_icon(level, &I_icon_tree, 5 + i * 17, 50 + j * 17, 16, 16); // Vertical lines
        }
    }

    // Spawn labyrinth lines
    // Third line (14 left, 3 middle, 0 right) - exit line
    for (int i = 0; i < 14; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 2 * 17, 16, 16);
    }
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 16 * 17 + i * 17, 2 + 2 * 17, 16, 16);
    }

    // Fourth line (3 left, 6 middle, 4 right)
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 3 * 17, 16, 16); // 3 left
    }
    for (int i = 0; i < 6; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 7 * 17 + i * 17, 2 + 3 * 17, 16, 16); // 6 middle
    }
    for (int i = 0; i < 4; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 15 * 17 + i * 17, 2 + 3 * 17, 16, 16); // 4 right
    }

    // Fifth line (6 left, 7 middle, 0 right)
    for (int i = 0; i < 6; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 4 * 17, 16, 16); // 6 left
    }
    for (int i = 0; i < 7; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 7 * 17 + i * 17, 2 + 4 * 17, 16, 16); // 7 middle
    }

    // Sixth line (5 left, 6 middle, 7 right)
    for (int i = 0; i < 5; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 5 * 17, 16, 16); // 5 left
    }
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 7 * 17 + i * 17, 2 + 5 * 17, 16, 16); // 3 middle
    }
    for (int i = 0; i < 7; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 15 * 17 + i * 17, 2 + 5 * 17, 16, 16); // 7 right
    }

    // Seventh line (0 left, 7 middle, 4 right)
    for (int i = 0; i < 7; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 6 * 17 + i * 17, 2 + 6 * 17, 16, 16); // 7 middle
    }
    for (int i = 0; i < 4; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 14 * 17 + i * 17, 2 + 6 * 17, 16, 16); // 4 right
    }

    // Eighth line (4 left, 3 middle, 4 right)
    for (int i = 0; i < 4; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 7 * 17, 16, 16); // 4 left
    }
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 7 * 17 + i * 17, 2 + 7 * 17, 16, 16); // 3 middle
    }
    for (int i = 0; i < 4; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 15 * 17 + i * 17, 2 + 7 * 17, 16, 16); // 4 right
    }

    // Ninth line (3 left, 2 middle, 3 right)
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + i * 17, 2 + 8 * 17, 16, 16); // 3 left
    }
    for (int i = 0; i < 1; i++) // 2 middle
    {
        spawn_icon(level, &I_icon_tree, 5 + 5 * 17 + i * 17, 2 + 8 * 17, 16, 16);
    }
    for (int i = 0; i < 3; i++)
    {
        spawn_icon(level, &I_icon_tree, 5 + 11 * 17 + i * 17, 2 + 8 * 17, 16, 16); // 3 right
    }
}