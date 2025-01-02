#include <game/storage.h>

static bool save_uint32(const char *path_name, uint32_t value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%lu", value);
    return save_char(path_name, buffer);
}

// Helper function to save an int8_t
static bool save_int8(const char *path_name, int8_t value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%d", value);
    return save_char(path_name, buffer);
}

// Helper function to save a float
static bool save_float(const char *path_name, float value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%.6f", (double)value); // Limit to 6 decimal places
    return save_char(path_name, buffer);
}
bool save_player_context(PlayerContext *player_context)
{
    if (!player_context)
    {
        FURI_LOG_E(TAG, "Invalid player context");
        return false;
    }

    // Create the directory for saving settings
    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/data/player");

    // Create the directory
    Storage *storage = furi_record_open(RECORD_STORAGE);
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);

    // 1. Username (String)
    if (!save_char("player/username", player_context->username))
    {
        FURI_LOG_E(TAG, "Failed to save player username");
        return false;
    }

    // 2. Level (uint32_t)
    if (!save_uint32("player/level", player_context->level))
    {
        FURI_LOG_E(TAG, "Failed to save player level");
        return false;
    }

    // 3. XP (uint32_t)
    if (!save_uint32("player/xp", player_context->xp))
    {
        FURI_LOG_E(TAG, "Failed to save player xp");
        return false;
    }

    // 4. Health (uint32_t)
    if (!save_uint32("player/health", player_context->health))
    {
        FURI_LOG_E(TAG, "Failed to save player health");
        return false;
    }

    // 5. Strength (uint32_t)
    if (!save_uint32("player/strength", player_context->strength))
    {
        FURI_LOG_E(TAG, "Failed to save player strength");
        return false;
    }

    // 6. Max Health (uint32_t)
    if (!save_uint32("player/max_health", player_context->max_health))
    {
        FURI_LOG_E(TAG, "Failed to save player max health");
        return false;
    }

    // 7. Health Regen (uint32_t)
    if (!save_uint32("player/health_regen", player_context->health_regen))
    {
        FURI_LOG_E(TAG, "Failed to save player health regen");
        return false;
    }

    // 8. Elapsed Health Regen (float)
    if (!save_float("player/elapsed_health_regen", player_context->elapsed_health_regen))
    {
        FURI_LOG_E(TAG, "Failed to save player elapsed health regen");
        return false;
    }

    // 9. Attack Timer (float)
    if (!save_float("player/attack_timer", player_context->attack_timer))
    {
        FURI_LOG_E(TAG, "Failed to save player attack timer");
        return false;
    }

    // 10. Elapsed Attack Timer (float)
    if (!save_float("player/elapsed_attack_timer", player_context->elapsed_attack_timer))
    {
        FURI_LOG_E(TAG, "Failed to save player elapsed attack timer");
        return false;
    }

    // 11. Direction (enum PlayerDirection)
    {
        char direction_str[2];
        switch (player_context->direction)
        {
        case PLAYER_UP:
            strncpy(direction_str, "0", sizeof(direction_str));
            break;
        case PLAYER_DOWN:
            strncpy(direction_str, "1", sizeof(direction_str));
            break;
        case PLAYER_LEFT:
            strncpy(direction_str, "2", sizeof(direction_str));
            break;
        case PLAYER_RIGHT:
        default:
            strncpy(direction_str, "3", sizeof(direction_str));
            break;
        }
        direction_str[1] = '\0'; // Ensure null termination

        if (!save_char("player/direction", direction_str))
        {
            FURI_LOG_E(TAG, "Failed to save player direction");
            return false;
        }
    }

    // 12. State (enum PlayerState)
    {
        char state_str[2];
        switch (player_context->state)
        {
        case PLAYER_IDLE:
            strncpy(state_str, "0", sizeof(state_str));
            break;
        case PLAYER_MOVING:
            strncpy(state_str, "1", sizeof(state_str));
            break;
        case PLAYER_ATTACKING:
            strncpy(state_str, "2", sizeof(state_str));
            break;
        case PLAYER_ATTACKED:
            strncpy(state_str, "3", sizeof(state_str));
            break;
        case PLAYER_DEAD:
            strncpy(state_str, "4", sizeof(state_str));
            break;
        default:
            strncpy(state_str, "5", sizeof(state_str)); // Assuming '5' for unknown states
            break;
        }
        state_str[1] = '\0'; // Ensure null termination

        if (!save_char("player/state", state_str))
        {
            FURI_LOG_E(TAG, "Failed to save player state");
            return false;
        }
    }

    // 13. Start Position X (float)
    if (!save_float("player/start_position_x", player_context->start_position.x))
    {
        FURI_LOG_E(TAG, "Failed to save player start position x");
        return false;
    }

    // 14. Start Position Y (float)
    if (!save_float("player/start_position_y", player_context->start_position.y))
    {
        FURI_LOG_E(TAG, "Failed to save player start position y");
        return false;
    }

    // 15. dx (int8_t)
    if (!save_int8("player/dx", player_context->dx))
    {
        FURI_LOG_E(TAG, "Failed to save player dx");
        return false;
    }

    // 16. dy (int8_t)
    if (!save_int8("player/dy", player_context->dy))
    {
        FURI_LOG_E(TAG, "Failed to save player dy");
        return false;
    }

    return true;
}

// Helper function to load an integer
bool load_number(const char *path_name, int *value)
{
    if (!path_name || !value)
    {
        FURI_LOG_E(TAG, "Invalid arguments to load_number");
        return false;
    }

    char buffer[64];

    if (!load_char(path_name, buffer, sizeof(buffer)))
    {
        FURI_LOG_E(TAG, "Failed to load number from path: %s", path_name);
        return false;
    }

    *value = atoi(buffer);
    return true;
}

// Helper function to load a float
bool load_float(const char *path_name, float *value)
{
    if (!path_name || !value)
    {
        FURI_LOG_E(TAG, "Invalid arguments to load_float");
        return false;
    }

    char buffer[64];
    if (!load_char(path_name, buffer, sizeof(buffer)))
    {
        FURI_LOG_E(TAG, "Failed to load float from path: %s", path_name);
        return false;
    }

    // check if the string is a valid float
    char *endptr;
    *value = strtof(buffer, &endptr);
    if (endptr == buffer)
    {
        FURI_LOG_E(TAG, "Failed to parse float from path: %s", path_name);
        return false;
    }

    return true;
}

// Helper function to load an int8_t
bool load_int8(const char *path_name, int8_t *value)
{
    if (!path_name || !value)
    {
        FURI_LOG_E(TAG, "Invalid arguments to load_int8");
        return false;
    }

    char buffer[64];

    if (!load_char(path_name, buffer, sizeof(buffer)))
    {
        FURI_LOG_E(TAG, "Failed to load int8 from path: %s", path_name);
        return false;
    }

    long temp = strtol(buffer, NULL, 10);
    if (temp < INT8_MIN || temp > INT8_MAX)
    {
        FURI_LOG_E(TAG, "Value out of range for int8: %ld", temp);
        return false;
    }

    // check if the string is a valid int8
    char *endptr;
    *value = (int8_t)strtol(buffer, &endptr, 10);
    if (endptr == buffer)
    {
        FURI_LOG_E(TAG, "Failed to parse int8 from path: %s", path_name);
        return false;
    }
    return true;
}

// Helper function to load a uint32_t
bool load_uint32(const char *path_name, uint32_t *value)
{
    if (!path_name || !value)
    {
        FURI_LOG_E(TAG, "Invalid arguments to load_uint32");
        return false;
    }

    char buffer[64];
    if (!load_char(path_name, buffer, sizeof(buffer)))
    {
        FURI_LOG_E(TAG, "Failed to load uint32 from path: %s", path_name);
        return false;
    }

    // check if the string is a valid uint32
    char *endptr;
    *value = strtoul(buffer, &endptr, 10);
    if (endptr == buffer)
    {
        FURI_LOG_E(TAG, "Failed to parse uint32 from path: %s", path_name);
        return false;
    }

    return true;
}

bool load_player_context(PlayerContext *player_context)
{
    if (!player_context)
    {
        FURI_LOG_E(TAG, "Invalid player context");
        return false;
    }

    // 1. Username (String)
    if (!load_char("player/username", player_context->username, sizeof(player_context->username)))
    {
        FURI_LOG_E(TAG, "No data or parse error for username. Using default: 'Unknown'");
        memset(player_context->username, 0, sizeof(player_context->username));
        strncpy(player_context->username, "Unknown", sizeof(player_context->username) - 1);
    }

    // 2. Level (uint32_t)
    {
        uint32_t temp = 1; // Default
        if (!load_char("player/level", (char *)&temp, sizeof(temp)))
        {
            FURI_LOG_E(TAG, "No data or parse error for level. Using default: 1");
        }
        else
        {
            // char buffer[64];
            if (load_uint32("player/level", &temp))
            {
                player_context->level = temp;
            }
            else
            {
                FURI_LOG_E(TAG, "Failed to parse level. Using default: 1");
                player_context->level = 1;
            }
        }
        player_context->level = temp;
    }

    // 3. XP (uint32_t)
    {
        uint32_t temp = 0; // Default
        if (!load_uint32("player/xp", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for xp. Using default: 0");
            temp = 0;
        }
        player_context->xp = temp;
    }

    // 4. Health (uint32_t)
    {
        uint32_t temp = 100; // Default
        if (!load_uint32("player/health", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for health. Using default: 100");
            temp = 100;
        }
        player_context->health = temp;
    }

    // 5. Strength (uint32_t)
    {
        uint32_t temp = 10; // Default
        if (!load_uint32("player/strength", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for strength. Using default: 10");
            temp = 10;
        }
        player_context->strength = temp;
    }

    // 6. Max Health (uint32_t)
    {
        uint32_t temp = 100; // Default
        if (!load_uint32("player/max_health", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for max_health. Using default: 100");
            temp = 100;
        }
        player_context->max_health = temp;
    }

    // 7. Health Regen (uint32_t)
    {
        uint32_t temp = 1; // Default
        if (!load_uint32("player/health_regen", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for health_regen. Using default: 1");
            temp = 1;
        }
        player_context->health_regen = temp;
    }

    // 8. Elapsed Health Regen (float)
    {
        float temp = 0.0f; // Default
        if (!load_float("player/elapsed_health_regen", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for elapsed_health_regen. Using default: 0.0f");
            temp = 0.0f;
        }
        player_context->elapsed_health_regen = temp;
    }

    // 9. Attack Timer (float)
    {
        float temp = 0.1f; // Default
        if (!load_float("player/attack_timer", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for attack_timer. Using default: 0.1f");
            temp = 0.1f;
        }
        player_context->attack_timer = temp;
    }

    // 10. Elapsed Attack Timer (float)
    {
        float temp = 0.0f; // Default
        if (!load_float("player/elapsed_attack_timer", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for elapsed_attack_timer. Using default: 0.0f");
            temp = 0.0f;
        }
        player_context->elapsed_attack_timer = temp;
    }

    // 11. Direction (enum PlayerDirection)
    {
        int direction_int = 3; // Default to PLAYER_RIGHT
        if (!load_number("player/direction", &direction_int))
        {
            FURI_LOG_E(TAG, "No data or parse error for direction. Defaulting to PLAYER_RIGHT");
            direction_int = 3;
        }

        switch (direction_int)
        {
        case 0:
            player_context->direction = PLAYER_UP;
            break;
        case 1:
            player_context->direction = PLAYER_DOWN;
            break;
        case 2:
            player_context->direction = PLAYER_LEFT;
            break;
        case 3:
            player_context->direction = PLAYER_RIGHT;
            break;
        default:
            FURI_LOG_E(TAG, "Invalid direction value: %d. Defaulting to PLAYER_RIGHT", direction_int);
            player_context->direction = PLAYER_RIGHT;
            break;
        }
    }

    // 12. State (enum PlayerState)
    {
        int state_int = 0; // Default to PLAYER_IDLE
        if (!load_number("player/state", &state_int))
        {
            FURI_LOG_E(TAG, "No data or parse error for state. Defaulting to PLAYER_IDLE");
            state_int = 0;
        }

        switch (state_int)
        {
        case 0:
            player_context->state = PLAYER_IDLE;
            break;
        case 1:
            player_context->state = PLAYER_MOVING;
            break;
        case 2:
            player_context->state = PLAYER_ATTACKING;
            break;
        case 3:
            player_context->state = PLAYER_ATTACKED;
            break;
        case 4:
            player_context->state = PLAYER_DEAD;
            break;
        default:
            FURI_LOG_E(TAG, "Invalid state value: %d. Defaulting to PLAYER_IDLE", state_int);
            player_context->state = PLAYER_IDLE;
            break;
        }
    }

    // 13. Start Position X (float)
    {
        float temp = 192.0f; // Default
        if (!load_float("player/start_position_x", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for start_position_x. Using default: 192.0f");
            temp = 192.0f;
        }
        player_context->start_position.x = temp;
    }

    // 14. Start Position Y (float)
    {
        float temp = 96.0f; // Default
        if (!load_float("player/start_position_y", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for start_position_y. Using default: 96.0f");
            temp = 96.0f;
        }
        player_context->start_position.y = temp;
    }

    // 15. dx (int8_t)
    {
        int8_t temp = 1; // Default
        if (!load_int8("player/dx", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for dx. Using default: 1");
            temp = 1;
        }
        player_context->dx = temp;
    }

    // 16. dy (int8_t)
    {
        int8_t temp = 0; // Default
        if (!load_int8("player/dy", &temp))
        {
            FURI_LOG_E(TAG, "No data or parse error for dy. Using default: 0");
            temp = 0;
        }
        player_context->dy = temp;
    }

    return true;
}

static inline void furi_string_remove_str(FuriString *string, const char *needle)
{
    furi_string_replace_str(string, needle, "", 0);
}

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
    if (!file_json_data || furi_string_size(file_json_data) == 0)
    {
        FURI_LOG_E("Game", "Failed to get json data in separate_world_data");
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
    storage_file_close(file);

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
