#include <game/storage.h>
bool save_player_context(PlayerContext *player_context)
{
    if (!player_context)
    {
        FURI_LOG_E(TAG, "Invalid player context");
        return false;
    }
    char player_context_json[512];
    snprintf(player_context_json, sizeof(player_context_json), "{\"username\":\"%s\",\"level\":%lu,\"xp\":%lu,\"health\":%lu,\"strength\":%lu,\"max_health\":%lu,\"health_regen\":%ld,\"elapsed_health_regen\":%f,\"attack_timer\":%f,\"elapsed_attack_timer\":%f,\"direction\":%u,\"state\":%u,\"start_position\":{\"x\":%f,\"y\":%f},\"dx\":%u,\"dy\":%u}",
             player_context->username, player_context->level, player_context->xp, player_context->health, player_context->strength, player_context->max_health, player_context->health_regen, (double)player_context->elapsed_health_regen, (double)player_context->attack_timer, (double)player_context->elapsed_attack_timer, player_context->direction, player_context->state, (double)player_context->start_position.x, (double)player_context->start_position.y, player_context->dx, player_context->dy);

    return save_char("player_context", player_context_json);
}

PlayerContext *load_player_context()
{
    char player_context_json[512];
    if (!load_char("player_context", player_context_json, sizeof(player_context_json)))
    {
        FURI_LOG_E(TAG, "Failed to load player context");
        return NULL;
    }
    PlayerContext *player_context = (PlayerContext *)malloc(sizeof(PlayerContext));
    if (!player_context)
    {
        FURI_LOG_E(TAG, "Failed to allocate player context");
        return NULL;
    }
    // Parse the JSON data
    char *username = get_json_value("username", player_context_json);
    char *level = get_json_value("level", player_context_json);
    char *xp = get_json_value("xp", player_context_json);
    char *health = get_json_value("health", player_context_json);
    char *strength = get_json_value("strength", player_context_json);
    char *max_health = get_json_value("max_health", player_context_json);
    char *health_regen = get_json_value("health_regen", player_context_json);
    char *elapsed_health_regen = get_json_value("elapsed_health_regen", player_context_json);
    char *attack_timer = get_json_value("attack_timer", player_context_json);
    char *elapsed_attack_timer = get_json_value("elapsed_attack_timer", player_context_json);
    char *direction = get_json_value("direction", player_context_json);
    char *state = get_json_value("state", player_context_json);
    char *dx = get_json_value("dx", player_context_json);
    char *dy = get_json_value("dy", player_context_json);
    char *start_position = get_json_value("start_position", player_context_json);
    char *start_position_x = get_json_value("x", start_position);
    char *start_position_y = get_json_value("y", start_position);

    if (!username || !level || !xp || !health || !strength || !max_health || !health_regen || !elapsed_health_regen || !attack_timer || !elapsed_attack_timer || !direction || !state || !dx || !dy || !start_position || !start_position_x || !start_position_y)
    {
        FURI_LOG_E(TAG, "Failed to parse player context");
        free(player_context);
        return NULL;
    }

    // Copy the parsed values to the player context
    strncpy(player_context->username, username, sizeof(player_context->username));
    player_context->level = atoi(level);
    player_context->xp = atoi(xp);
    player_context->health = atoi(health);
    player_context->strength = atoi(strength);
    player_context->max_health = atoi(max_health);
    player_context->health_regen = atoi(health_regen);
    player_context->elapsed_health_regen = strtod(elapsed_health_regen, NULL);
    player_context->attack_timer = strtod(attack_timer, NULL);
    player_context->elapsed_attack_timer = strtod(elapsed_attack_timer, NULL);
    player_context->direction = atoi(direction);
    player_context->state = atoi(state);
    player_context->dx = atoi(dx);
    player_context->dy = atoi(dy);
    player_context->start_position.x = atoi(start_position_x);
    player_context->start_position.y = atoi(start_position_y);

    return player_context;
}

#include <stdio.h>
#include <furi.h>
#include <furi_hal.h>
// etc.  Make sure you include the relevant headers for your project

// 1) Helper: remove first occurrence of "needle" from "string"
static inline void furi_string_remove_str(FuriString *string, const char *needle)
{
    // Remove the FIRST occurrence
    furi_string_replace_str(string, needle, "", 0);
}

// 2) Adjusted function: returns exactly "json_data":[ ... ]
static FuriString *enemy_data(const FuriString *world_data)
{
    size_t enemy_data_pos = furi_string_search_str(world_data, "enemy_data", 0);
    if (enemy_data_pos == FURI_STRING_FAILURE)
    {
        FURI_LOG_E("Game", "Failed to find enemy_data in world data");

        return NULL;
    }

    size_t bracket_start = furi_string_search_char(world_data, '[', enemy_data_pos);
    if (bracket_start == FURI_STRING_FAILURE)
    {
        FURI_LOG_E("Game", "Failed to find start of enemy_data array");

        return NULL;
    }

    size_t bracket_end = furi_string_search_char(world_data, ']', bracket_start);
    if (bracket_end == FURI_STRING_FAILURE)
    {
        FURI_LOG_E("Game", "Failed to find end of enemy_data array");

        return NULL;
    }

    FuriString *enemy_data_str = furi_string_alloc();
    if (!enemy_data_str)
    {
        FURI_LOG_E("Game", "Failed to allocate enemy_data string");

        return NULL;
    }

    furi_string_cat_str(enemy_data_str, "{\"enemy_data\":");

    {
        FuriString *temp_sub = furi_string_alloc();

        furi_string_set_strn(
            temp_sub,
            furi_string_get_cstr(world_data) + bracket_start,
            (bracket_end + 1) - bracket_start);

        furi_string_cat(enemy_data_str, temp_sub);
        furi_string_free(temp_sub);
    }

    furi_string_cat_str(enemy_data_str, "}");

    return enemy_data_str;
}

static FuriString *json_data(const FuriString *world_data)
{
    size_t json_data_pos = furi_string_search_str(world_data, "json_data", 0);
    if (json_data_pos == FURI_STRING_FAILURE)
    {
        FURI_LOG_E("Game", "Failed to find json_data in world data");

        return NULL;
    }

    size_t bracket_start = furi_string_search_char(world_data, '[', json_data_pos);
    if (bracket_start == FURI_STRING_FAILURE)
    {
        FURI_LOG_E("Game", "Failed to find start of json_data array");

        return NULL;
    }

    size_t bracket_end = furi_string_search_char(world_data, ']', bracket_start);
    if (bracket_end == FURI_STRING_FAILURE)
    {
        FURI_LOG_E("Game", "Failed to find end of json_data array");

        return NULL;
    }

    FuriString *json_data_str = furi_string_alloc();
    if (!json_data_str)
    {
        FURI_LOG_E("Game", "Failed to allocate json_data string");

        return NULL;
    }

    furi_string_cat_str(json_data_str, "{\"json_data\":");

    {
        FuriString *temp_sub = furi_string_alloc();

        furi_string_set_strn(
            temp_sub,
            furi_string_get_cstr(world_data) + bracket_start,
            (bracket_end + 1) - bracket_start);

        furi_string_cat(json_data_str, temp_sub);
        furi_string_free(temp_sub);
    }

    furi_string_cat_str(json_data_str, "}");

    return json_data_str;
}

bool separate_world_data(char *id, FuriString *world_data)
{
    if (!id || !world_data)
    {
        FURI_LOG_E("Game", "Invalid parameters");
        return false;
    }
    FuriString *file_json_data = json_data(world_data);
    if (!file_json_data)
    {
        FURI_LOG_E("Game", "Failed to get json data");
        return false;
    }

    // Save file_json_data to disk
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s", id);

    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);

    File *file = storage_file_alloc(storage);
    char file_path[256];
    snprintf(file_path, sizeof(file_path),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s/%s_json_data.json",
             id, id);

    if (!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        FURI_LOG_E("Game", "Failed to open file for writing: %s", file_path);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        furi_string_free(file_json_data);
        return false;
    }

    size_t data_size = furi_string_size(file_json_data);
    if (storage_file_write(file, furi_string_get_cstr(file_json_data), data_size) != data_size)
    {
        FURI_LOG_E("Game", "Failed to write json_data");
    }

    furi_string_replace_at(file_json_data, 0, 1, "");
    furi_string_replace_at(file_json_data, furi_string_size(file_json_data) - 1, 1, "");
    // include the comma at the end of the json_data array
    furi_string_cat_str(file_json_data, ",");
    furi_string_remove_str(world_data, furi_string_get_cstr(file_json_data));
    furi_string_free(file_json_data);

    FuriString *file_enemy_data = enemy_data(world_data);
    if (!file_enemy_data)
    {
        FURI_LOG_E("Game", "Failed to get enemy data");
        return false;
    }

    snprintf(file_path, sizeof(file_path),
             STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/%s/%s_enemy_data.json",
             id, id);

    if (!storage_file_open(file, file_path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        FURI_LOG_E("Game", "Failed to open file for writing: %s", file_path);
        storage_file_free(file);
        furi_record_close(RECORD_STORAGE);
        furi_string_free(file_enemy_data);
        return false;
    }

    data_size = furi_string_size(file_enemy_data);
    if (storage_file_write(file, furi_string_get_cstr(file_enemy_data), data_size) != data_size)
    {
        FURI_LOG_E("Game", "Failed to write enemy_data");
    }

    // Clean up
    furi_string_free(file_enemy_data);
    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);

    return true;
}
