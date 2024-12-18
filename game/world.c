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
        FuriString *data = get_json_array_value_furi("json_data", i, json_data);
        if (data == NULL)
        {
            break;
        }
        FuriString *icon = get_json_value_furi("icon", data);
        FuriString *x = get_json_value_furi("x", data);
        FuriString *y = get_json_value_furi("y", data);
        FuriString *width = get_json_value_furi("width", data);
        FuriString *height = get_json_value_furi("height", data);
        FuriString *amount = get_json_value_furi("amount", data);
        FuriString *horizontal = get_json_value_furi("horizontal", data);
        if (!icon || !x || !y || !width || !height || !amount || !horizontal)
        {
            furi_string_free(data);
            furi_string_free(icon);
            furi_string_free(x);
            furi_string_free(y);
            furi_string_free(width);
            furi_string_free(height);
            furi_string_free(amount);
            furi_string_free(horizontal);
            return false;
        }
        // if amount is less than 2, we spawn a single icon
        if (atoi(furi_string_get_cstr(amount)) < 2)
        {
            spawn_icon(
                level,
                get_icon_furi(icon),
                atoi(furi_string_get_cstr(x)),
                atoi(furi_string_get_cstr(y)),
                atoi(furi_string_get_cstr(width)),
                atoi(furi_string_get_cstr(height)));
            furi_string_free(data);
            furi_string_free(icon);
            furi_string_free(x);
            furi_string_free(y);
            furi_string_free(width);
            furi_string_free(height);
            furi_string_free(amount);
            furi_string_free(horizontal);
            continue;
        }
        spawn_icon_line(
            level,
            get_icon_furi(icon),
            atoi(furi_string_get_cstr(x)),
            atoi(furi_string_get_cstr(y)),
            atoi(furi_string_get_cstr(width)),
            atoi(furi_string_get_cstr(height)),
            atoi(furi_string_get_cstr(amount)),
            furi_string_cmp(horizontal, "true") == 0);
        furi_string_free(data);
        furi_string_free(icon);
        furi_string_free(x);
        furi_string_free(y);
        furi_string_free(width);
        furi_string_free(height);
        furi_string_free(amount);
        furi_string_free(horizontal);
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

void draw_town_world(Level *level)
{
    // house-fence group 1
    spawn_icon(level, get_icon("house"), 148, 36, 48, 32);
    spawn_icon(level, get_icon("fence"), 148, 72, 16, 8);
    spawn_icon(level, get_icon("fence"), 164, 72, 16, 8);
    spawn_icon(level, get_icon("fence_end"), 180, 72, 16, 8);

    // house-fence group 4 (the left of group 1)
    spawn_icon(level, get_icon("house"), 96, 36, 48, 32);
    spawn_icon(level, get_icon("fence"), 96, 72, 16, 8);
    spawn_icon(level, get_icon("fence"), 110, 72, 16, 8);
    spawn_icon(level, get_icon("fence_end"), 126, 72, 16, 8);

    // house-fence group 5 (the left of group 4)
    spawn_icon(level, get_icon("house"), 40, 36, 48, 32);
    spawn_icon(level, get_icon("fence"), 40, 72, 16, 8);
    spawn_icon(level, get_icon("fence"), 56, 72, 16, 8);
    spawn_icon(level, get_icon("fence_end"), 72, 72, 16, 8);

    // line of fences on the 8th row (using spawn_icon_line)
    spawn_icon_line(level, get_icon("fence"), 8, 100, 16, 8, 10, true);

    // plants spaced out underneath the fences
    spawn_icon_line(level, get_icon("plant"), 40, 110, 16, 16, 6, true);
    spawn_icon_line(level, get_icon("flower"), 40, 140, 16, 16, 6, true);

    // man and woman
    spawn_icon(level, get_icon("man"), 156, 110, 7, 16);
    spawn_icon(level, get_icon("woman"), 164, 110, 9, 16);

    // lake
    // Top row
    spawn_icon(level, get_icon("lake_top_left"), 240, 52, 24, 22);
    spawn_icon(level, get_icon("lake_top"), 264, 52, 31, 22);
    spawn_icon(level, get_icon("lake_top_right"), 295, 52, 24, 22);

    // Middle row
    spawn_icon(level, get_icon("lake_left"), 231, 74, 11, 31);
    spawn_icon(level, get_icon("lake_right"), 317, 74, 11, 31);

    // Bottom row
    spawn_icon(level, get_icon("lake_bottom_left"), 240, 105, 24, 22);
    spawn_icon(level, get_icon("lake_bottom"), 264, 124, 31, 12);
    spawn_icon(level, get_icon("lake_bottom_right"), 295, 105, 24, 22);

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
}
/*
{
    "name": "town_world",
    "author": "JBlanked",
    "json_data": [
        { "icon": "house",     "x": 148, "y": 36, "width": 48, "height": 32, "amount": 1, "horizontal": true },
        { "icon": "fence",     "x": 148, "y": 72, "width": 16, "height": 8,  "amount": 1, "horizontal": true },
        { "icon": "fence",     "x": 164, "y": 72, "width": 16, "height": 8,  "amount": 1, "horizontal": true },
        { "icon": "fence_end", "x": 180, "y": 72, "width": 16, "height": 8,  "amount": 1, "horizontal": true },
        { "icon": "house",     "x": 96,  "y": 36, "width": 48, "height": 32, "amount": 1, "horizontal": true },
        { "icon": "fence",     "x": 96,  "y": 72, "width": 16, "height": 8,  "amount": 1, "horizontal": true },
        { "icon": "fence",     "x": 110, "y": 72, "width": 16, "height": 8,  "amount": 1, "horizontal": true },
        { "icon": "fence_end", "x": 126, "y": 72, "width": 16, "height": 8,  "amount": 1, "horizontal": true },
        { "icon": "house",     "x": 40,  "y": 36, "width": 48, "height": 32, "amount": 1, "horizontal": true },
        { "icon": "fence",     "x": 40,  "y": 72, "width": 16, "height": 8,  "amount": 1, "horizontal": true },
        { "icon": "fence",     "x": 56,  "y": 72, "width": 16, "height": 8,  "amount": 1, "horizontal": true },
        { "icon": "fence_end", "x": 72,  "y": 72, "width": 16, "height": 8,  "amount": 1, "horizontal": true },
        { "icon": "fence",  "x": 8,  "y": 100, "width": 16, "height": 8,  "amount": 10, "horizontal": true },
        { "icon": "plant",  "x": 40, "y": 110, "width": 16, "height": 16, "amount": 6, "horizontal": true },
        { "icon": "flower", "x": 40, "y": 140, "width": 16, "height": 16, "amount": 6, "horizontal": true },
        { "icon": "man",    "x": 156, "y": 110, "width": 7,  "height": 16, "amount": 1, "horizontal": true },
        { "icon": "woman",  "x": 164, "y": 110, "width": 9,  "height": 16, "amount": 1, "horizontal": true },
        { "icon": "lake_top_left",     "x": 240, "y": 52,  "width": 24, "height": 22, "amount": 1, "horizontal": true },
        { "icon": "lake_top",          "x": 264, "y": 52,  "width": 31, "height": 22, "amount": 1, "horizontal": true },
        { "icon": "lake_top_right",    "x": 295, "y": 52,  "width": 24, "height": 22, "amount": 1, "horizontal": true },
        { "icon": "lake_left",         "x": 231, "y": 74,  "width": 11, "height": 31, "amount": 1, "horizontal": true },
        { "icon": "lake_right",        "x": 317, "y": 74,  "width": 11, "height": 31, "amount": 1, "horizontal": true },
        { "icon": "lake_bottom_left",  "x": 240, "y": 105, "width": 24, "height": 22, "amount": 1, "horizontal": true },
        { "icon": "lake_bottom",       "x": 264, "y": 124, "width": 31, "height": 12, "amount": 1, "horizontal": true },
        { "icon": "lake_bottom_right", "x": 295, "y": 105, "width": 24, "height": 22, "amount": 1, "horizontal": true },
        { "icon": "tree", "x": 5,   "y": 2,   "width": 16, "height": 16, "amount": 22, "horizontal": true },
        { "icon": "tree", "x": 5,   "y": 2,   "width": 16, "height": 16, "amount": 11, "horizontal": false },
        { "icon": "tree", "x": 22,  "y": 2,   "width": 16, "height": 16, "amount": 22, "horizontal": true },
        { "icon": "tree", "x": 22,  "y": 2,   "width": 16, "height": 16, "amount": 11, "horizontal": false },
        { "icon": "tree", "x": 5,   "y": 155, "width": 16, "height": 16, "amount": 22, "horizontal": true },
        { "icon": "tree", "x": 5,   "y": 172, "width": 16, "height": 16, "amount": 22, "horizontal": true },
        { "icon": "tree", "x": 345, "y": 50,  "width": 16, "height": 16, "amount": 8,  "horizontal": false },
        { "icon": "tree", "x": 362, "y": 50,  "width": 16, "height": 16, "amount": 8,  "horizontal": false }
    ]
}
*/

FuriString *fetch_world(char *name, void *app)
{
    if (!app || !name)
    {
        FURI_LOG_E("Game", "App or name is NULL");
        return NULL;
    }
    if (!flipper_http_init(flipper_http_rx_callback, app))
    {
        FURI_LOG_E("Game", "Failed to initialize HTTP");
        return NULL;
    }
    char url[256];
    snprintf(url, sizeof(url), "https://www.flipsocial.net/api/world/get/world/%s/", name);
    snprintf(fhttp.file_path, sizeof(fhttp.file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s.json", name);
    fhttp.save_received_data = true;
    if (!flipper_http_get_request_with_headers(url, "{\"Content-Type\": \"application/json\"}"))
    {
        FURI_LOG_E("Game", "Failed to send HTTP request");
        return NULL;
    }
    furi_timer_start(fhttp.get_timeout_timer, TIMEOUT_DURATION_TICKS);
    while (fhttp.state == RECEIVING && furi_timer_is_running(fhttp.get_timeout_timer) > 0)
    {
        // Wait for the request to be received
        furi_delay_ms(100);
    }
    furi_timer_stop(fhttp.get_timeout_timer);
    if (fhttp.state != IDLE)
    {
        FURI_LOG_E("Game", "Failed to receive world data");
        return NULL;
    }
    FuriString *returned_data = flipper_http_load_from_file(fhttp.file_path);
    if (!returned_data)
    {
        FURI_LOG_E("Game", "Failed to load world data from file");
        return NULL;
    }
    return returned_data;
}