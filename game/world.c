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
        // 1) Get data array item
        char *data = get_json_array_value("json_data", i, json_data);
        if (!data)
        {
            // Means we've reached the end of the array
            break;
        }

        // 2) Extract all required fields
        char *icon = get_json_value("icon", data);
        char *x = get_json_value("x", data);
        char *y = get_json_value("y", data);
        char *amount = get_json_value("amount", data);
        char *horizontal = get_json_value("horizontal", data);

        // 3) Check for any NULL pointers
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

        // 4) Get the IconContext
        IconContext *icon_context = get_icon_context(icon);
        if (!icon_context)
        {
            FURI_LOG_E("Game", "Failed Icon: %s", icon);

            free(data);
            free(icon);
            free(x);
            free(y);
            free(amount);
            free(horizontal);

            level_clear(level);
            return false;
        }

        // 5) Decide how many icons to spawn
        int count = atoi(amount);
        if (count < 2)
        {
            // Just one icon
            spawn_icon(
                level,
                icon_context->icon,
                atoi(x),
                atoi(y),
                icon_context->width,
                icon_context->height);
        }
        else
        {
            // Spawn multiple in a line
            bool is_horizontal = (strcmp(horizontal, "true") == 0);
            spawn_icon_line(
                level,
                icon_context->icon,
                atoi(x),
                atoi(y),
                icon_context->width,
                icon_context->height,
                count,
                is_horizontal);
        }

        // 6) Cleanup
        free(data);
        free(icon);
        free(x);
        free(y);
        free(amount);
        free(horizontal);
        free(icon_context);
    }
    return true;
}

bool draw_json_world_furi(Level *level, FuriString *json_data)
{
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        // 1) Get data array item as FuriString
        FuriString *data = get_json_array_value_furi("json_data", i, json_data);
        if (!data)
        {
            // Means we've reached the end of the array
            break;
        }

        // 2) Extract all required fields
        FuriString *icon = get_json_value_furi("icon", data);
        FuriString *x = get_json_value_furi("x", data);
        FuriString *y = get_json_value_furi("y", data);
        FuriString *amount = get_json_value_furi("amount", data);
        FuriString *horizontal = get_json_value_furi("horizontal", data);

        // 3) Check for any NULL pointers
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

        // 4) Get the IconContext from a FuriString
        IconContext *icon_context = get_icon_context_furi(icon);
        if (!icon_context)
        {
            FURI_LOG_E("Game", "Failed Icon: %s", furi_string_get_cstr(icon));

            furi_string_free(data);
            furi_string_free(icon);
            furi_string_free(x);
            furi_string_free(y);
            furi_string_free(amount);
            furi_string_free(horizontal);

            level_clear(level);
            return false;
        }

        // 5) Decide how many icons to spawn
        int count = atoi(furi_string_get_cstr(amount));
        if (count < 2)
        {
            // Just one icon
            spawn_icon(
                level,
                icon_context->icon,
                atoi(furi_string_get_cstr(x)),
                atoi(furi_string_get_cstr(y)),
                icon_context->width,
                icon_context->height);
        }
        else
        {
            // Spawn multiple in a line
            bool is_horizontal = (furi_string_cmp(horizontal, "true") == 0);
            spawn_icon_line(
                level,
                icon_context->icon,
                atoi(furi_string_get_cstr(x)),
                atoi(furi_string_get_cstr(y)),
                icon_context->width,
                icon_context->height,
                count,
                is_horizontal);
        }

        // 6) Clean up after every iteration
        furi_string_free(data);
        furi_string_free(icon);
        furi_string_free(x);
        furi_string_free(y);
        furi_string_free(amount);
        furi_string_free(horizontal);
        free(icon_context);
    }
    return true;
}

void draw_tree_world(Level *level)
{
    IconContext *tree_icon = get_icon_context("tree");
    if (!tree_icon)
    {
        FURI_LOG_E("Game", "Failed to get tree icon context");
        return;
    }
    // Spawn two full left/up tree lines
    for (int i = 0; i < 2; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, tree_icon->icon, 5, 2 + i * 17, tree_icon->width, tree_icon->height, 22, true);
        // Vertical line of 11 icons
        spawn_icon_line(level, tree_icon->icon, 5 + i * 17, 2, tree_icon->width, tree_icon->height, 11, false);
    }

    // Spawn two full down tree lines
    for (int i = 9; i < 11; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, tree_icon->icon, 5, 2 + i * 17, tree_icon->width, tree_icon->height, 22, true);
    }

    // Spawn two full right tree lines
    for (int i = 20; i < 22; i++)
    {
        // Vertical line of 8 icons starting further down (y=50)
        spawn_icon_line(level, tree_icon->icon, 5 + i * 17, 50, tree_icon->width, tree_icon->height, 8, false);
    }

    // Labyrinth lines
    // Third line (14 left, then a gap, then 3 middle)
    spawn_icon_line(level, tree_icon->icon, 5, 2 + 2 * 17, tree_icon->width, tree_icon->height, 14, true);
    spawn_icon_line(level, tree_icon->icon, 5 + 16 * 17, 2 + 2 * 17, tree_icon->width, tree_icon->height, 3, true);

    // Fourth line (3 left, 6 middle, 4 right)
    spawn_icon_line(level, tree_icon->icon, 5, 2 + 3 * 17, tree_icon->width, tree_icon->height, 3, true);           // 3 left
    spawn_icon_line(level, tree_icon->icon, 5 + 7 * 17, 2 + 3 * 17, tree_icon->width, tree_icon->height, 6, true);  // 6 middle
    spawn_icon_line(level, tree_icon->icon, 5 + 15 * 17, 2 + 3 * 17, tree_icon->width, tree_icon->height, 4, true); // 4 right

    // Fifth line (6 left, 7 middle)
    spawn_icon_line(level, tree_icon->icon, 5, 2 + 4 * 17, tree_icon->width, tree_icon->height, 6, true);
    spawn_icon_line(level, tree_icon->icon, 5 + 7 * 17, 2 + 4 * 17, tree_icon->width, tree_icon->height, 7, true);

    // Sixth line (5 left, 3 middle, 7 right)
    spawn_icon_line(level, tree_icon->icon, 5, 2 + 5 * 17, tree_icon->width, tree_icon->height, 5, true);           // 5 left
    spawn_icon_line(level, tree_icon->icon, 5 + 7 * 17, 2 + 5 * 17, tree_icon->width, tree_icon->height, 3, true);  // 3 middle
    spawn_icon_line(level, tree_icon->icon, 5 + 15 * 17, 2 + 5 * 17, tree_icon->width, tree_icon->height, 7, true); // 7 right

    // Seventh line (0 left, 7 middle, 4 right)
    spawn_icon_line(level, tree_icon->icon, 5 + 6 * 17, 2 + 6 * 17, tree_icon->width, tree_icon->height, 7, true);  // 7 middle
    spawn_icon_line(level, tree_icon->icon, 5 + 14 * 17, 2 + 6 * 17, tree_icon->width, tree_icon->height, 4, true); // 4 right

    // Eighth line (4 left, 3 middle, 4 right)
    spawn_icon_line(level, tree_icon->icon, 5, 2 + 7 * 17, tree_icon->width, tree_icon->height, 4, true);           // 4 left
    spawn_icon_line(level, tree_icon->icon, 5 + 7 * 17, 2 + 7 * 17, tree_icon->width, tree_icon->height, 3, true);  // 3 middle
    spawn_icon_line(level, tree_icon->icon, 5 + 15 * 17, 2 + 7 * 17, tree_icon->width, tree_icon->height, 4, true); // 4 right

    // Ninth line (3 left, 1 middle, 3 right)
    spawn_icon_line(level, tree_icon->icon, 5, 2 + 8 * 17, tree_icon->width, tree_icon->height, 3, true);           // 3 left
    spawn_icon_line(level, tree_icon->icon, 5 + 5 * 17, 2 + 8 * 17, tree_icon->width, tree_icon->height, 1, true);  // 1 middle
    spawn_icon_line(level, tree_icon->icon, 5 + 11 * 17, 2 + 8 * 17, tree_icon->width, tree_icon->height, 3, true); // 3 right

    free(tree_icon);
}

void draw_town_world(Level *level)
{
    // define all the icons
    IconContext *house_icon = get_icon_context("house");
    IconContext *fence_icon = get_icon_context("fence");
    IconContext *fence_end_icon = get_icon_context("fence_end");
    IconContext *plant_icon = get_icon_context("plant");
    IconContext *flower_icon = get_icon_context("flower");
    IconContext *man_icon = get_icon_context("man");
    IconContext *woman_icon = get_icon_context("woman");
    IconContext *lake_top_left_icon = get_icon_context("lake_top_left");
    IconContext *lake_top_icon = get_icon_context("lake_top");
    IconContext *lake_top_right_icon = get_icon_context("lake_top_right");
    IconContext *lake_left_icon = get_icon_context("lake_left");
    IconContext *lake_right_icon = get_icon_context("lake_right");
    IconContext *lake_bottom_left_icon = get_icon_context("lake_bottom_left");
    IconContext *lake_bottom_icon = get_icon_context("lake_bottom");
    IconContext *lake_bottom_right_icon = get_icon_context("lake_bottom_right");
    IconContext *tree_icon = get_icon_context("tree");

    // check if any of the icons are NULL
    if (!house_icon || !fence_icon || !fence_end_icon || !plant_icon || !flower_icon ||
        !man_icon || !woman_icon || !lake_top_left_icon || !lake_top_icon || !lake_top_right_icon ||
        !lake_left_icon || !lake_right_icon || !lake_bottom_left_icon || !lake_bottom_icon || !lake_bottom_right_icon || !tree_icon)
    {
        FURI_LOG_E("Game", "Failed to get icon context");
        return;
    }

    // house-fence group 1
    spawn_icon(level, house_icon->icon, 148, 36, house_icon->width, house_icon->height);
    spawn_icon(level, fence_icon->icon, 148, 72, fence_icon->width, fence_icon->height);
    spawn_icon(level, fence_icon->icon, 164, 72, fence_icon->width, fence_icon->height);
    spawn_icon(level, fence_end_icon->icon, 180, 72, fence_end_icon->width, fence_end_icon->height);

    // house-fence group 4 (the left of group 1)
    spawn_icon(level, house_icon->icon, 96, 36, house_icon->width, house_icon->height);
    spawn_icon(level, fence_icon->icon, 96, 72, fence_icon->width, fence_icon->height);
    spawn_icon(level, fence_icon->icon, 110, 72, fence_icon->width, fence_icon->height);
    spawn_icon(level, fence_end_icon->icon, 126, 72, fence_end_icon->width, fence_end_icon->height);

    // house-fence group 5 (the left of group 4)
    spawn_icon(level, house_icon->icon, 40, 36, house_icon->width, house_icon->height);
    spawn_icon(level, fence_icon->icon, 40, 72, fence_icon->width, fence_icon->height);
    spawn_icon(level, fence_icon->icon, 56, 72, fence_icon->width, fence_icon->height);
    spawn_icon(level, fence_end_icon->icon, 72, 72, fence_end_icon->width, fence_end_icon->height);

    // line of fences on the 8th row (using spawn_icon_line)
    spawn_icon_line(level, fence_icon->icon, 8, 100, fence_icon->width, fence_icon->height, 10, true);

    // plants spaced out underneath the fences
    spawn_icon_line(level, plant_icon->icon, 40, 110, plant_icon->width, plant_icon->height, 6, true);
    spawn_icon_line(level, flower_icon->icon, 40, 140, flower_icon->width, flower_icon->height, 6, true);

    // man and woman
    spawn_icon(level, man_icon->icon, 156, 110, man_icon->width, man_icon->height);
    spawn_icon(level, woman_icon->icon, 164, 110, woman_icon->width, woman_icon->height);

    // lake
    // Top row
    spawn_icon(level, lake_top_left_icon->icon, 240, 52, lake_top_left_icon->width, lake_top_left_icon->height);
    spawn_icon(level, lake_top_icon->icon, 264, 52, lake_top_icon->width, lake_top_icon->height);
    spawn_icon(level, lake_top_right_icon->icon, 295, 52, lake_top_right_icon->width, lake_top_right_icon->height);

    // Middle row
    spawn_icon(level, lake_left_icon->icon, 231, 74, lake_left_icon->width, lake_left_icon->height);
    spawn_icon(level, lake_right_icon->icon, 317, 74, lake_right_icon->width, lake_right_icon->height);

    // Bottom row
    spawn_icon(level, lake_bottom_left_icon->icon, 240, 105, lake_bottom_left_icon->width, lake_bottom_left_icon->height);
    spawn_icon(level, lake_bottom_icon->icon, 264, 124, lake_bottom_icon->width, lake_bottom_icon->height);
    spawn_icon(level, lake_bottom_right_icon->icon, 295, 105, lake_bottom_right_icon->width, lake_bottom_right_icon->height);

    // Spawn two full left/up tree lines
    for (int i = 0; i < 2; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, tree_icon->icon, 5, 2 + i * 17, tree_icon->width, tree_icon->height, 22, true);
        // Vertical line of 11 icons
        spawn_icon_line(level, tree_icon->icon, 5 + i * 17, 2, tree_icon->width, tree_icon->height, 11, false);
    }

    // Spawn two full down tree lines
    for (int i = 9; i < 11; i++)
    {
        // Horizontal line of 22 icons
        spawn_icon_line(level, tree_icon->icon, 5, 2 + i * 17, tree_icon->width, tree_icon->height, 22, true);
    }

    // Spawn two full right tree lines
    for (int i = 20; i < 22; i++)
    {
        // Vertical line of 8 icons starting further down (y=50)
        spawn_icon_line(level, tree_icon->icon, 5 + i * 17, 50, tree_icon->width, tree_icon->height, 8, false);
    }
    free(house_icon);
    free(fence_icon);
    free(fence_end_icon);
    free(plant_icon);
    free(flower_icon);
    free(man_icon);
    free(woman_icon);
    free(lake_top_left_icon);
    free(lake_top_icon);
    free(lake_top_right_icon);
    free(lake_left_icon);
    free(lake_right_icon);
    free(lake_bottom_left_icon);
    free(lake_bottom_icon);
    free(lake_bottom_right_icon);
    free(tree_icon);
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
    snprintf(url, sizeof(url), "https://www.flipsocial.net/api/world/get/world/%s/", name);
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
    return returned_data;
}