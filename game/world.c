#include <game/world.h>
#include <game/storage.h>
#include <flip_storage/storage.h>

bool draw_json_world_furi(GameManager *manager, Level *level, const FuriString *json_data)
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
                manager,
                level,
                furi_string_get_cstr(icon),
                atoi(furi_string_get_cstr(x)),
                atoi(furi_string_get_cstr(y)));
        }
        else
        {
            bool is_horizontal = (furi_string_cmp(horizontal, "true") == 0);
            spawn_icon_line(
                manager,
                level,
                furi_string_get_cstr(icon),
                atoi(furi_string_get_cstr(x)),
                atoi(furi_string_get_cstr(y)),
                count,
                is_horizontal,
                17 // set as 17 for now
            );
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

static void draw_town_world(Level *level, GameManager *manager, void *context)
{
    UNUSED(context);
    if (!manager || !level)
    {
        FURI_LOG_E("Game", "Manager or level is NULL");
        return;
    }
    GameContext *game_context = game_manager_game_context_get(manager);
    level_clear(level);
    FuriString *json_data_str = furi_string_alloc();
    furi_string_cat_str(json_data_str, "{\"name\":\"shadow_woods_v5\",\"author\":\"ChatGPT\",\"json_data\":[{\"icon\":\"rock_medium\",\"x\":100,\"y\":100,\"amount\":10,\"horizontal\":true},{\"icon\":\"rock_medium\",\"x\":400,\"y\":300,\"amount\":6,\"horizontal\":true},{\"icon\":\"rock_small\",\"x\":600,\"y\":200,\"amount\":8,\"horizontal\":true},{\"icon\":\"fence\",\"x\":50,\"y\":50,\"amount\":10,\"horizontal\":true},{\"icon\":\"fence\",\"x\":250,\"y\":150,\"amount\":12,\"horizontal\":true},{\"icon\":\"fence\",\"x\":550,\"y\":350,\"amount\":12,\"horizontal\":true},{\"icon\":\"rock_large\",\"x\":400,\"y\":70,\"amount\":12,\"horizontal\":true},{\"icon\":\"rock_large\",\"x\":200,\"y\":200,\"amount\":6,\"horizontal\":false},{\"icon\":\"tree\",\"x\":5,\"y\":5,\"amount\":45,\"horizontal\":true},{\"icon\":\"tree\",\"x\":5,\"y\":5,\"amount\":20,\"horizontal\":false},{\"icon\":\"tree\",\"x\":22,\"y\":22,\"amount\":44,\"horizontal\":true},{\"icon\":\"tree\",\"x\":22,\"y\":22,\"amount\":20,\"horizontal\":false},{\"icon\":\"tree\",\"x\":5,\"y\":347,\"amount\":45,\"horizontal\":true},{\"icon\":\"tree\",\"x\":5,\"y\":364,\"amount\":45,\"horizontal\":true},{\"icon\":\"tree\",\"x\":735,\"y\":37,\"amount\":18,\"horizontal\":false},{\"icon\":\"tree\",\"x\":752,\"y\":37,\"amount\":18,\"horizontal\":false}],\"enemy_data\":[{\"id\":\"cyclops\",\"index\":0,\"start_position\":{\"x\":350,\"y\":210},\"end_position\":{\"x\":390,\"y\":210},\"move_timer\":2,\"speed\":30,\"attack_timer\":0.4,\"strength\":10,\"health\":100},{\"id\":\"ogre\",\"index\":1,\"start_position\":{\"x\":200,\"y\":320},\"end_position\":{\"x\":220,\"y\":320},\"move_timer\":0.5,\"speed\":45,\"attack_timer\":0.6,\"strength\":20,\"health\":200},{\"id\":\"ghost\",\"index\":2,\"start_position\":{\"x\":100,\"y\":80},\"end_position\":{\"x\":180,\"y\":85},\"move_timer\":2.2,\"speed\":55,\"attack_timer\":0.5,\"strength\":30,\"health\":300},{\"id\":\"ogre\",\"index\":3,\"start_position\":{\"x\":400,\"y\":50},\"end_position\":{\"x\":490,\"y\":50},\"move_timer\":1.7,\"speed\":35,\"attack_timer\":1.0,\"strength\":20,\"health\":200}],\"npc_data\":[{\"id\":\"funny\",\"index\":0,\"start_position\":{\"x\":350,\"y\":180},\"end_position\":{\"x\":350,\"y\":180},\"move_timer\":0,\"speed\":0,\"message\":\"Hello there!\"}]}");
    if (!separate_world_data("shadow_woods_v5", json_data_str))
    {
        FURI_LOG_E("Game", "Failed to separate world data");
    }
    furi_string_free(json_data_str);
    set_world(level, manager, "shadow_woods_v5");
    game_context->icon_offset = 0;
    if (!game_context->imu_present)
    {
        game_context->icon_offset += ((game_context->icon_count / 10) / 15);
    }
    player_spawn(level, manager);
}

static const LevelBehaviour _training_world = {
    .alloc = NULL,
    .free = NULL,
    .start = draw_town_world,
    .stop = NULL,
    .context_size = 0,
};

const LevelBehaviour *training_world()
{
    return &_training_world;
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
    snprintf(url, sizeof(url), "https://www.jblanked.com/flipper/api/world/v5/get/world/%s/", name);
    snprintf(fhttp->file_path, sizeof(fhttp->file_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s.json", name);
    fhttp->save_received_data = true;
    if (!flipper_http_request(fhttp, GET, url, "{\"Content-Type\": \"application/json\"}", NULL))
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