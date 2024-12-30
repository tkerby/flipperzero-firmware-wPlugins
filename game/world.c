#include <game/world.h>
#include <game/storage.h>
#include <flip_storage/storage.h>
void draw_bounds(Canvas *canvas)
{
    // Draw the outer bounds adjusted by camera offset
    // we draw this last to ensure users can see the bounds
    canvas_draw_frame(canvas, -camera_x, -camera_y, WORLD_WIDTH, WORLD_HEIGHT);
}

bool draw_json_world(Level *level, const char *json_data)
{
    int levels_added = 0;
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        char *data = get_json_array_value("json_data", i, json_data);
        if (!data)
        {
            break;
        }

        char *icon = get_json_value("icon", data);
        char *x = get_json_value("x", data);
        char *y = get_json_value("y", data);
        char *amount = get_json_value("amount", data);
        char *horizontal = get_json_value("horizontal", data);

        if (!icon || !x || !y || !amount || !horizontal)
        {
            FURI_LOG_E("Game", "Failed Data: %s", data);

            // Free everything carefully
            if (data)
                free(data);
            if (icon)
                free(icon);
            if (x)
                free(x);
            if (y)
                free(y);
            if (amount)
                free(amount);
            if (horizontal)
                free(horizontal);

            level_clear(level);
            return false;
        }

        int count = atoi(amount);
        if (count < 2)
        {
            // Just one icon
            spawn_icon(
                level,
                icon,
                atoi(x),
                atoi(y));
        }
        else
        {
            bool is_horizontal = (strcmp(horizontal, "true") == 0);
            spawn_icon_line(
                level,
                icon,
                atoi(x),
                atoi(y),
                count,
                is_horizontal);
        }
        free(data);
        free(icon);
        free(x);
        free(y);
        free(amount);
        free(horizontal);
        levels_added++;
    }
    return levels_added > 0;
}

bool draw_json_world_furi(Level *level, const FuriString *json_data)
{
    if (!json_data)
    {
        FURI_LOG_E("Game", "JSON data is NULL");
        return false;
    }
    int levels_added = 0;
    FURI_LOG_I("Game", "Looping through world data");
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        FURI_LOG_I("Game", "Looping through world data: %d", i);
        FuriString *data = get_json_array_value_furi("json_data", i, json_data);
        if (!data)
        {
            break;
        }

        FuriString *icon = get_json_value_furi("icon", data);
        FuriString *x = get_json_value_furi("x", data);
        FuriString *y = get_json_value_furi("y", data);
        FuriString *amount = get_json_value_furi("amount", data);
        FuriString *horizontal = get_json_value_furi("horizontal", data);

        if (!icon || !x || !y || !amount || !horizontal)
        {
            FURI_LOG_E("Game", "Failed Data: %s", furi_string_get_cstr(data));

            if (data)
                furi_string_free(data);
            if (icon)
                furi_string_free(icon);
            if (x)
                furi_string_free(x);
            if (y)
                furi_string_free(y);
            if (amount)
                furi_string_free(amount);
            if (horizontal)
                furi_string_free(horizontal);

            level_clear(level);
            return false;
        }

        int count = atoi(furi_string_get_cstr(amount));
        if (count < 2)
        {
            // Just one icon
            spawn_icon(
                level,
                furi_string_get_cstr(icon),
                atoi(furi_string_get_cstr(x)),
                atoi(furi_string_get_cstr(y)));
        }
        else
        {
            bool is_horizontal = (furi_string_cmp(horizontal, "true") == 0);
            spawn_icon_line(
                level,
                furi_string_get_cstr(icon),
                atoi(furi_string_get_cstr(x)),
                atoi(furi_string_get_cstr(y)),
                count,
                is_horizontal);
        }

        furi_string_free(data);
        furi_string_free(icon);
        furi_string_free(x);
        furi_string_free(y);
        furi_string_free(amount);
        furi_string_free(horizontal);
        levels_added++;
    }
    FURI_LOG_I("Game", "Finished loading world data");
    return levels_added > 0;
}

void draw_tree_world(Level *level)
{
    // Spawn two full left/up tree lines
    for (int i = 0; i < 2; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, "tree", 5, 2 + i * 17, 22, true);
        // Vertical line of 11 icons
        spawn_icon_line(level, "tree", 5 + i * 17, 2, 11, false);
    }

    // Spawn two full down tree lines
    for (int i = 9; i < 11; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, "tree", 5, 2 + i * 17, 22, true);
    }

    // Spawn two full right tree lines
    for (int i = 20; i < 22; i++)
    {
        // Vertical line of 8 icons starting further down (y=50)
        spawn_icon_line(level, "tree", 5 + i * 17, 50, 8, false);
    }

    // Labyrinth lines
    // Third line (14 left, then a gap, then 3 middle)
    spawn_icon_line(level, "tree", 5, 2 + 2 * 17, 14, true);
    spawn_icon_line(level, "tree", 5 + 16 * 17, 2 + 2 * 17, 3, true);

    // Fourth line (3 left, 6 middle, 4 right)
    spawn_icon_line(level, "tree", 5, 2 + 3 * 17, 3, true);           // 3 left
    spawn_icon_line(level, "tree", 5 + 7 * 17, 2 + 3 * 17, 6, true);  // 6 middle
    spawn_icon_line(level, "tree", 5 + 15 * 17, 2 + 3 * 17, 4, true); // 4 right

    // Fifth line (6 left, 7 middle)
    spawn_icon_line(level, "tree", 5, 2 + 4 * 17, 6, true);
    spawn_icon_line(level, "tree", 5 + 7 * 17, 2 + 4 * 17, 7, true);

    // Sixth line (5 left, 3 middle, 7 right)
    spawn_icon_line(level, "tree", 5, 2 + 5 * 17, 5, true);           // 5 left
    spawn_icon_line(level, "tree", 5 + 7 * 17, 2 + 5 * 17, 3, true);  // 3 middle
    spawn_icon_line(level, "tree", 5 + 15 * 17, 2 + 5 * 17, 7, true); // 7 right

    // Seventh line (0 left, 7 middle, 4 right)
    spawn_icon_line(level, "tree", 5 + 6 * 17, 2 + 6 * 17, 7, true);  // 7 middle
    spawn_icon_line(level, "tree", 5 + 14 * 17, 2 + 6 * 17, 4, true); // 4 right

    // Eighth line (4 left, 3 middle, 4 right)
    spawn_icon_line(level, "tree", 5, 2 + 7 * 17, 4, true);           // 4 left
    spawn_icon_line(level, "tree", 5 + 7 * 17, 2 + 7 * 17, 3, true);  // 3 middle
    spawn_icon_line(level, "tree", 5 + 15 * 17, 2 + 7 * 17, 4, true); // 4 right

    // Ninth line (3 left, 1 middle, 3 right)
    spawn_icon_line(level, "tree", 5, 2 + 8 * 17, 3, true);           // 3 left
    spawn_icon_line(level, "tree", 5 + 5 * 17, 2 + 8 * 17, 1, true);  // 1 middle
    spawn_icon_line(level, "tree", 5 + 11 * 17, 2 + 8 * 17, 3, true); // 3 right
}

void draw_town_world(Level *level)
{

    // house-fence group 1
    spawn_icon(level, "house", 164, 40);
    spawn_icon(level, "fence", 148, 64);
    spawn_icon(level, "fence", 164, 64);
    spawn_icon(level, "fence_end", 180, 64);

    // house-fence group 4 (the left of group 1)
    spawn_icon(level, "house", 110, 40);
    spawn_icon(level, "fence", 96, 64);
    spawn_icon(level, "fence", 110, 64);
    spawn_icon(level, "fence_end", 126, 64);

    // house-fence group 5 (the left of group 4)
    spawn_icon(level, "house", 56, 40);
    spawn_icon(level, "fence", 40, 64);
    spawn_icon(level, "fence", 56, 64);
    spawn_icon(level, "fence_end", 72, 64);

    // line of fences on the 8th row (using spawn_icon_line)
    spawn_icon_line(level, "fence", 8, 96, 10, true);

    // plants spaced out underneath the fences
    spawn_icon_line(level, "plant", 40, 110, 6, true);
    spawn_icon_line(level, "flower", 40, 140, 6, true);

    // man and woman
    spawn_icon(level, "man", 156, 110);
    spawn_icon(level, "woman", 164, 110);

    // lake
    // Top row
    spawn_icon(level, "lake_top_left", 240, 62);
    spawn_icon(level, "lake_top", 264, 57);
    spawn_icon(level, "lake_top_right", 295, 62);

    // Middle row
    spawn_icon(level, "lake_left", 231, 84);
    spawn_icon(level, "lake_right", 304, 84);

    // Bottom row
    spawn_icon(level, "lake_bottom_left", 240, 115);
    spawn_icon(level, "lake_bottom", 264, 120);
    spawn_icon(level, "lake_bottom_right", 295, 115);

    // Spawn two full left/up tree lines
    for (int i = 0; i < 2; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, "tree", 5, 2 + i * 17, 22, true);
        // Vertical line of 11 icons
        spawn_icon_line(level, "tree", 5 + i * 17, 2, 11, false);
    }

    // Spawn two full down tree lines
    for (int i = 9; i < 11; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, "tree", 5, 2 + i * 17, 22, true);
    }

    // Spawn two full right tree lines
    for (int i = 20; i < 22; i++)
    {
        // Vertical line of 8 icons starting further down (y=50)
        spawn_icon_line(level, "tree", 5 + i * 17, 50, 8, false);
    }
}

FuriString *fetch_world(const char *name)
{
    if (!name)
    {
        FURI_LOG_E("Game", "World name is NULL");
        return NULL;
    }
    if (!app_instance)
    {
        // as long as the game is running, app_instance should be non-NULL
        FURI_LOG_E("Game", "App instance is NULL");
        return NULL;
    }

    if (!flipper_http_init(flipper_http_rx_callback, app_instance))
    {
        FURI_LOG_E("Game", "Failed to initialize HTTP");
        return NULL;
    }
    char url[256];
    snprintf(url, sizeof(url), "https://www.flipsocial.net/api/world/v2/get/world/%s/", name);
    snprintf(fhttp.file_path, sizeof(fhttp.file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s.json", name);
    fhttp.save_received_data = true;
    if (!flipper_http_get_request_with_headers(url, "{\"Content-Type\": \"application/json\"}"))
    {
        FURI_LOG_E("Game", "Failed to send HTTP request");
        flipper_http_deinit();
        return NULL;
    }
    fhttp.state = RECEIVING;
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
        flipper_http_deinit();
        return NULL;
    }
    flipper_http_deinit();
    FuriString *returned_data = load_furi_world(name);
    if (!returned_data)
    {
        FURI_LOG_E("Game", "Failed to load world data from file");
        return NULL;
    }
    if (!separate_world_data((char *)name, returned_data))
    {
        FURI_LOG_E("Game", "Failed to separate world data");
        furi_string_free(returned_data);
        return NULL;
    }
    return returned_data;
}