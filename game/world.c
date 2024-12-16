#include <game/world.h>

void draw_bounds(Canvas *canvas)
{
    // Draw the outer bounds adjusted by camera offset
    // we draw this last to ensure users can see the bounds
    canvas_draw_frame(canvas, -camera_x, -camera_y, WORLD_WIDTH, WORLD_HEIGHT);
}

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
        char *amount = get_json_value("amount", data, 64);
        char *horizontal = get_json_value("horizontal", data, 64);
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
            "height": 16,
            "amount": 1,
            "horizontal": true
        },
        {
            "icon": "home",
            "x": 128,
            "y": 24,
            "width": 15,
            "height": 16,
            "amount": 1,
            "horizontal": true
        },
        {
            "icon": "info",
            "x": 144,
            "y": 24,
            "width": 15,
            "height": 16,
            "amount": 1,
            "horizontal": true
        },
        {
            "icon": "man",
            "x": 160,
            "y": 56,
            "width": 7,
            "height": 16,
            "amount": 1,
            "horizontal": true
        },
        {
            "icon": "woman",
            "x": 168,
            "y": 56,
            "width": 9,
            "height": 16,
            "amount": 1,
            "horizontal": true
        },
        {
            "icon": "plant",
            "x": 168,
            "y": 32,
            "width": 16,
            "height": 16,
            "amount": 1,
            "horizontal": true
        }
    ]
}

*/

void draw_tree_world(Level *level)
{
    // Spawn two full left/up tree lines
    for (int i = 0; i < 2; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, &I_icon_tree, 5, 2 + i * 17, 16, 16, 22, true);
        // Vertical line of 11 icons
        spawn_icon_line(level, &I_icon_tree, 5 + i * 17, 2, 16, 16, 11, false);
    }

    // Spawn two full down tree lines
    for (int i = 9; i < 11; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, &I_icon_tree, 5, 2 + i * 17, 16, 16, 22, true);
    }

    // Spawn two full right tree lines
    for (int i = 20; i < 22; i++)
    {
        // Vertical line of 8 icons starting further down (y=50)
        spawn_icon_line(level, &I_icon_tree, 5 + i * 17, 50, 16, 16, 8, false);
    }

    // Labyrinth lines
    // Third line (14 left, then a gap, then 3 middle)
    spawn_icon_line(level, &I_icon_tree, 5, 2 + 2 * 17, 16, 16, 14, true);
    spawn_icon_line(level, &I_icon_tree, 5 + 16 * 17, 2 + 2 * 17, 16, 16, 3, true);

    // Fourth line (3 left, 6 middle, 4 right)
    spawn_icon_line(level, &I_icon_tree, 5, 2 + 3 * 17, 16, 16, 3, true);           // 3 left
    spawn_icon_line(level, &I_icon_tree, 5 + 7 * 17, 2 + 3 * 17, 16, 16, 6, true);  // 6 middle
    spawn_icon_line(level, &I_icon_tree, 5 + 15 * 17, 2 + 3 * 17, 16, 16, 4, true); // 4 right

    // Fifth line (6 left, 7 middle)
    spawn_icon_line(level, &I_icon_tree, 5, 2 + 4 * 17, 16, 16, 6, true);
    spawn_icon_line(level, &I_icon_tree, 5 + 7 * 17, 2 + 4 * 17, 16, 16, 7, true);

    // Sixth line (5 left, 3 middle, 7 right)
    spawn_icon_line(level, &I_icon_tree, 5, 2 + 5 * 17, 16, 16, 5, true);           // 5 left
    spawn_icon_line(level, &I_icon_tree, 5 + 7 * 17, 2 + 5 * 17, 16, 16, 3, true);  // 3 middle
    spawn_icon_line(level, &I_icon_tree, 5 + 15 * 17, 2 + 5 * 17, 16, 16, 7, true); // 7 right

    // Seventh line (0 left, 7 middle, 4 right)
    spawn_icon_line(level, &I_icon_tree, 5 + 6 * 17, 2 + 6 * 17, 16, 16, 7, true);  // 7 middle
    spawn_icon_line(level, &I_icon_tree, 5 + 14 * 17, 2 + 6 * 17, 16, 16, 4, true); // 4 right

    // Eighth line (4 left, 3 middle, 4 right)
    spawn_icon_line(level, &I_icon_tree, 5, 2 + 7 * 17, 16, 16, 4, true);           // 4 left
    spawn_icon_line(level, &I_icon_tree, 5 + 7 * 17, 2 + 7 * 17, 16, 16, 3, true);  // 3 middle
    spawn_icon_line(level, &I_icon_tree, 5 + 15 * 17, 2 + 7 * 17, 16, 16, 4, true); // 4 right

    // Ninth line (3 left, 1 middle, 3 right)
    spawn_icon_line(level, &I_icon_tree, 5, 2 + 8 * 17, 16, 16, 3, true);           // 3 left
    spawn_icon_line(level, &I_icon_tree, 5 + 5 * 17, 2 + 8 * 17, 16, 16, 1, true);  // 1 middle
    spawn_icon_line(level, &I_icon_tree, 5 + 11 * 17, 2 + 8 * 17, 16, 16, 3, true); // 3 right
}

/* JSON of the draw_tree_world
{
    "name" : "tree_world",
    "author" : "JBlanked",
    "json_data" : [
        {"icon" : "tree", "x" : 5, "y" : 2, "width" : 16, "height" : 16, "amount" : 22, "horizontal" : true},
        {"icon" : "tree", "x" : 5, "y" : 2, "width" : 16, "height" : 16, "amount" : 11, "horizontal" : false},
        {"icon" : "tree", "x" : 22, "y" : 2, "width" : 16, "height" : 16, "amount" : 22, "horizontal" : true},
        {"icon" : "tree", "x" : 22, "y" : 2, "width" : 16, "height" : 16, "amount" : 11, "horizontal" : false},
        {"icon" : "tree", "x" : 5, "y" : 155, "width" : 16, "height" : 16, "amount" : 22, "horizontal" : true},
        {"icon" : "tree", "x" : 5, "y" : 172, "width" : 16, "height" : 16, "amount" : 22, "horizontal" : true},
        {"icon" : "tree", "x" : 345, "y" : 50, "width" : 16, "height" : 16, "amount" : 8, "horizontal" : false},
        {"icon" : "tree", "x" : 362, "y" : 50, "width" : 16, "height" : 16, "amount" : 8, "horizontal" : false},
        {"icon" : "tree", "x" : 5, "y" : 36, "width" : 16, "height" : 16, "amount" : 14, "horizontal" : true},
        {"icon" : "tree", "x" : 277, "y" : 36, "width" : 16, "height" : 16, "amount" : 3, "horizontal" : true},
        {"icon" : "tree", "x" : 5, "y" : 53, "width" : 16, "height" : 16, "amount" : 3, "horizontal" : true},
        {"icon" : "tree", "x" : 124, "y" : 53, "width" : 16, "height" : 16, "amount" : 6, "horizontal" : true},
        {"icon" : "tree", "x" : 260, "y" : 53, "width" : 16, "height" : 16, "amount" : 4, "horizontal" : true},
        {"icon" : "tree", "x" : 5, "y" : 70, "width" : 16, "height" : 16, "amount" : 6, "horizontal" : true},
        {"icon" : "tree", "x" : 124, "y" : 70, "width" : 16, "height" : 16, "amount" : 7, "horizontal" : true},
        {"icon" : "tree", "x" : 5, "y" : 87, "width" : 16, "height" : 16, "amount" : 5, "horizontal" : true},
        {"icon" : "tree", "x" : 124, "y" : 87, "width" : 16, "height" : 16, "amount" : 3, "horizontal" : true},
        {"icon" : "tree", "x" : 260, "y" : 87, "width" : 16, "height" : 16, "amount" : 7, "horizontal" : true},
        {"icon" : "tree", "x" : 107, "y" : 104, "width" : 16, "height" : 16, "amount" : 7, "horizontal" : true},
        {"icon" : "tree", "x" : 243, "y" : 104, "width" : 16, "height" : 16, "amount" : 4, "horizontal" : true},
        {"icon" : "tree", "x" : 5, "y" : 121, "width" : 16, "height" : 16, "amount" : 4, "horizontal" : true},
        {"icon" : "tree", "x" : 124, "y" : 121, "width" : 16, "height" : 16, "amount" : 3, "horizontal" : true},
        {"icon" : "tree", "x" : 260, "y" : 121, "width" : 16, "height" : 16, "amount" : 4, "horizontal" : true},
        {"icon" : "tree", "x" : 5, "y" : 138, "width" : 16, "height" : 16, "amount" : 3, "horizontal" : true},
        {"icon" : "tree", "x" : 90, "y" : 138, "width" : 16, "height" : 16, "amount" : 1, "horizontal" : true},
        {"icon" : "tree", "x" : 192, "y" : 138, "width" : 16, "height" : 16, "amount" : 3, "horizontal" : true}
    ]
}
*/
