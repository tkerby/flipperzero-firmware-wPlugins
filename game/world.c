#include <game/world.h>
#include <game/storage.h>
#include <flip_storage/storage.h>
void draw_bounds(Canvas *canvas)
{
    // Draw the outer bounds adjusted by camera offset
    // we draw this last to ensure users can see the bounds
    canvas_draw_frame(canvas, -camera_x, -camera_y, WORLD_WIDTH, WORLD_HEIGHT);
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

    FlipperHTTP *fhttp = flipper_http_alloc();
    if (!fhttp)
    {
        FURI_LOG_E("Game", "Failed to allocate HTTP");
        return NULL;
    }

    char url[256];
    snprintf(url, sizeof(url), "https://www.flipsocial.net/api/world/v3/get/world/%s/", name);
    snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s.json", name);
    fhttp->save_received_data = true;
    if (!flipper_http_get_request_with_headers(fhttp, url, "{\"Content-Type\": \"application/json\"}"))
    {
        FURI_LOG_E("Game", "Failed to send HTTP request");
        flipper_http_free(fhttp);
        return NULL;
    }
    fhttp->state = RECEIVING;
    furi_timer_start(fhttp->get_timeout_timer, TIMEOUT_DURATION_TICKS);
    while (fhttp->state == RECEIVING && furi_timer_is_running(fhttp->get_timeout_timer) > 0)
    {
        // Wait for the request to be received
        furi_delay_ms(100);
    }
    furi_timer_stop(fhttp->get_timeout_timer);
    if (fhttp->state != IDLE)
    {
        FURI_LOG_E("Game", "Failed to receive world data");
        flipper_http_free(fhttp);
        return NULL;
    }
    flipper_http_free(fhttp);
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