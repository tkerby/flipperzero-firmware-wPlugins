#include "run/run.hpp"
#include "run/sprites.hpp"
#include "app.hpp"
#include "jsmn/jsmn.h"

FlipWorldRun::FlipWorldRun()
{
    // Initialize chunked messages array
    for (size_t i = 0; i < MAX_CHUNKED_MESSAGES; i++)
    {
        chunkedMessages[i].id = 0;
        chunkedMessages[i].totalChunks = 0;
        chunkedMessages[i].receivedChunks = 0;
        chunkedMessages[i].data = nullptr;
        chunkedMessages[i].dataSize = 0;
        chunkedMessages[i].dataCapacity = 0;
        chunkedMessages[i].lastUpdateTime = 0;
    }
    chunkedMessageCount = 0;

    // Initialize message queue
    for (size_t i = 0; i < MAX_QUEUED_MESSAGES; i++)
    {
        messageQueue[i].message = nullptr;
        messageQueue[i].messageLen = 0;
    }
    queueHead = 0;
    queueTail = 0;
    queueSize = 0;
}

FlipWorldRun::~FlipWorldRun()
{
    // Clean up currentIconGroup if allocated
    if (currentIconGroup)
    {
        if (currentIconGroup->icons)
        {
            free(currentIconGroup->icons);
        }
        free(currentIconGroup);
        currentIconGroup = nullptr;
    }

    // Clean up chunked messages
    for (size_t i = 0; i < MAX_CHUNKED_MESSAGES; i++)
    {
        if (chunkedMessages[i].data)
        {
            free(chunkedMessages[i].data);
            chunkedMessages[i].data = nullptr;
        }
    }

    // Clean up message queue
    for (size_t i = 0; i < MAX_QUEUED_MESSAGES; i++)
    {
        if (messageQueue[i].message)
        {
            free(messageQueue[i].message);
            messageQueue[i].message = nullptr;
        }
    }
}

bool FlipWorldRun::addRemotePlayer(const char *username)
{
    // Only add remote players in PvE mode
    if (!isPvEMode || !username)
    {
        FURI_LOG_W("FlipWorldRun", "Cannot add remote player: isPvEMode=%s, username=%s",
                   isPvEMode ? "true" : "false", username ? username : "null");
        return false;
    }

    if (!engine || !engine->getGame() || !engine->getGame()->current_level)
    {
        FURI_LOG_E("FlipWorldRun", "Cannot add remote player: game engine not ready");
        return false;
    }

    auto currentLevel = engine->getGame()->current_level;

    // Check if player already exists
    for (int i = 0; i < currentLevel->getEntityCount(); i++)
    {
        Entity *entity = currentLevel->getEntity(i);
        if (entity && entity->type == ENTITY_PLAYER &&
            entity->name && strcmp(entity->name, username) == 0)
        {
            // Player already exists
            return true;
        }
    }

    char *persistentUsername = (char *)malloc(strlen(username) + 1);
    if (!persistentUsername)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to allocate memory for remote player username");
        return false;
    }
    strcpy(persistentUsername, username);

    // Add new remote player entity
    Entity *remotePlayer = new Entity(
        persistentUsername,         // name
        ENTITY_PLAYER,              // type
        Vector(384, 192),           // position
        Vector(15, 11),             // size
        player_left_sword_15x11px,  // sprite_data
        player_left_sword_15x11px,  // sprite_left_data
        player_right_sword_15x11px, // sprite_right_data
        nullptr,                    // start
        nullptr,                    // stop
        nullptr,                    // update
        pveRender,                  // render callback for PvE mode
        nullptr,                    // collision callback
        SPRITE_3D_NONE              // no 3D sprite for remote players
    );

    if (remotePlayer)
    {
        remotePlayer->name = persistentUsername;
        // Set some default stats for now (should be updated later)
        remotePlayer->health = 100.0f;
        remotePlayer->max_health = 100.0f;
        remotePlayer->strength = 10.0f;
        remotePlayer->xp = 0.0f;
        remotePlayer->level = 1.0f;

        currentLevel->entity_add(remotePlayer);
        return true;
    }
    else
    {
        FURI_LOG_E("FlipWorldRun", "Failed to create remote player entity for '%s'", username);
        free(persistentUsername);
    }

    return false;
}

void FlipWorldRun::cleanupExpiredChunkedMessages()
{
    uint32_t currentTime = furi_get_tick();
    const uint32_t CHUNK_TIMEOUT = 3000;

    for (size_t i = 0; i < MAX_CHUNKED_MESSAGES; i++)
    {
        if (chunkedMessages[i].data &&
            (currentTime - chunkedMessages[i].lastUpdateTime) > CHUNK_TIMEOUT)
        {
            FURI_LOG_W("FlipWorldRun", "Cleaning up expired chunked message ID %d (%zu bytes)",
                       chunkedMessages[i].id, chunkedMessages[i].dataCapacity);
            free(chunkedMessages[i].data);
            chunkedMessages[i].data = nullptr;
            chunkedMessages[i].id = 0;
            chunkedMessages[i].totalChunks = 0;
            chunkedMessages[i].receivedChunks = 0;
            chunkedMessages[i].dataSize = 0;
            chunkedMessages[i].dataCapacity = 0;
            chunkedMessages[i].lastUpdateTime = 0;
            if (chunkedMessageCount > 0)
            {
                chunkedMessageCount--;
            }
        }
    }

    // Clean up - if we have more than 50% full, clean up the oldest ones
    if (chunkedMessageCount > MAX_CHUNKED_MESSAGES * 0.5)
    {
        FURI_LOG_W("FlipWorldRun", "Too many concurrent chunked messages (%zu), cleaning oldest", chunkedMessageCount);
        uint32_t oldestTime = currentTime;
        size_t oldestIndex = SIZE_MAX;

        for (size_t i = 0; i < MAX_CHUNKED_MESSAGES; i++)
        {
            if (chunkedMessages[i].data && chunkedMessages[i].lastUpdateTime < oldestTime)
            {
                oldestTime = chunkedMessages[i].lastUpdateTime;
                oldestIndex = i;
            }
        }

        if (oldestIndex != SIZE_MAX)
        {
            FURI_LOG_W("FlipWorldRun", "Force cleaning oldest chunked message ID %d",
                       chunkedMessages[oldestIndex].id);
            free(chunkedMessages[oldestIndex].data);
            chunkedMessages[oldestIndex].data = nullptr;
            chunkedMessages[oldestIndex].id = 0;
            chunkedMessages[oldestIndex].totalChunks = 0;
            chunkedMessages[oldestIndex].receivedChunks = 0;
            chunkedMessages[oldestIndex].dataSize = 0;
            chunkedMessages[oldestIndex].dataCapacity = 0;
            chunkedMessages[oldestIndex].lastUpdateTime = 0;
            if (chunkedMessageCount > 0)
            {
                chunkedMessageCount--;
            }
        }
    }
}

void FlipWorldRun::debounceInput()
{
    static uint8_t debounceCounter = 0;
    if (shouldDebounce)
    {
        lastInput = InputKeyMAX;
        debounceCounter++;
        if (debounceCounter < 2)
        {
            return;
        }
        debounceCounter = 0;
        shouldDebounce = false;
        inputHeld = false;
    }
}

void FlipWorldRun::endGame()
{
    shouldReturnToMenu = true;
    isGameRunning = false;

    if (player)
    {
        player->userRequest(RequestTypeSaveStats); // Save player stats before ending the game
    }

    if (engine)
    {
        engine->stop();
        engine.reset();
    }

    if (draw)
    {
        draw.reset();
    }

    isPvEMode = false;
    isLobbyHost = false;
}

bool FlipWorldRun::entityJsonUpdate(Entity *entity)
{
    FlipWorldApp *app = static_cast<FlipWorldApp *>(appContext);
    if (!app)
    {
        FURI_LOG_E("Game", "entityJsonUpdate: App context is not set");
        return false;
    }
    auto response = app->getLastResponse();
    if (!response)
    {
        FURI_LOG_E("Game", "entityJsonUpdate: Response is null");
        return false;
    }
    if (strlen(response) == 0)
    {
        // no need for error log here, just return false
        return false;
    }

    // parse the response and set the position
    /* expected response:
    {
        "u": "JBlanked",
        "xp": 37743,
        "h": 207,
        "ehr": 0.7,
        "eat": 127.5,
        "d": 2,
        "s": 1,
        "sp": {
            "x": 381.0,
            "y": 192.0
        }
    }
    */

    char *u = get_json_value("u", response);
    if (!u)
    {
        FURI_LOG_E("Game", "entityJsonUpdate: Failed to get username");
        return false;
    }

    // check if the username matches
    if (strcmp(u, entity->name) != 0)
    {
        free(u);
        return false;
    }

    free(u); // free username

    // we need the health, elapsed attack timer, direction, xp, and position
    char *h = get_json_value("h", response);
    char *eat = get_json_value("eat", response);
    char *d = get_json_value("d", response);
    char *xp = get_json_value("xp", response);
    char *sp = get_json_value("sp", response);
    char *x = get_json_value("x", sp);
    char *y = get_json_value("y", sp);

    if (!h || !eat || !d || !sp || !x || !y || !xp)
    {
        if (h)
            free(h);
        if (eat)
            free(eat);
        if (d)
            free(d);
        if (sp)
            free(sp);
        if (x)
            free(x);
        if (y)
            free(y);
        if (xp)
            free(xp);
        return false;
    }

    // set enemy info
    entity->health = (float)atoi(h); // h is an int
    if (entity->health <= 0)
    {
        entity->health = 0;
        entity->state = ENTITY_DEAD;
        entity->position_set((Vector){-100, -100});
        free(h);
        free(eat);
        free(d);
        free(sp);
        free(x);
        free(y);
        free(xp);
        return true;
    }

    entity->elapsed_attack_timer = atof_(eat);

    switch (atoi(d))
    {
    case 0:
        entity->direction = ENTITY_LEFT;
        break;
    case 1:
        entity->direction = ENTITY_RIGHT;
        break;
    case 2:
        entity->direction = ENTITY_UP;
        break;
    case 3:
        entity->direction = ENTITY_DOWN;
        break;
    default:
        entity->direction = ENTITY_RIGHT;
        break;
    }

    entity->xp = (atoi)(xp); // xp is an int
    entity->level = 1;
    uint32_t xp_required = 100; // Base XP for level 2

    while (entity->level < 100 && entity->xp >= xp_required) // Maximum level supported
    {
        entity->level++;
        xp_required = (uint32_t)(xp_required * 1.5); // 1.5 growth factor per level
    }

    // set position
    entity->position_set(Vector(atof_(x), atof_(y)));

    // free the strings
    free(h);
    free(eat);
    free(d);
    free(sp);
    free(x);
    free(y);
    free(xp);

    return true;
}

const char *FlipWorldRun::entityToJson(Entity *entity, bool websocketParsing) const
{
    // "safely" append string to dynamically allocated buffer
    auto append_to_buffer = [](char **buffer, size_t *size, size_t *capacity, const char *str)
    {
        size_t str_len = strlen(str);
        size_t new_size = *size + str_len;

        if (new_size >= *capacity)
        {
            size_t new_capacity = *capacity * 2;
            while (new_capacity <= new_size)
            {
                new_capacity *= 2;
            }
            char *new_buffer = (char *)realloc(*buffer, new_capacity);
            if (!new_buffer)
            {
                return false;
            }
            *buffer = new_buffer;
            *capacity = new_capacity;
        }

        memcpy(*buffer + *size, str, str_len);
        (*buffer)[new_size] = '\0'; // Null terminate
        *size = new_size;
        return true;
    };

    // Helper function to convert direction Vector to numeric code
    auto direction_to_code = [](Vector dir) -> int
    {
        if (dir.x == -1 && dir.y == 0)
            return 0; // ENTITY_LEFT
        if (dir.x == 1 && dir.y == 0)
            return 1; // ENTITY_RIGHT
        if (dir.x == 0 && dir.y == -1)
            return 2; // ENTITY_UP
        if (dir.x == 0 && dir.y == 1)
            return 3; // ENTITY_DOWN
        return 1;     // default to right
    };

    // Initial capacity for the JSON buffer
    size_t json_capacity = 512;
    size_t json_size = 0;
    char *json = (char *)malloc(json_capacity);
    if (!json)
    {
        FURI_LOG_E(TAG, "Failed to allocate JSON string");
        return NULL;
    }
    json[0] = '\0'; // Initialize empty string

    if (!append_to_buffer(&json, &json_size, &json_capacity, "{"))
    {
        free(json);
        return NULL;
    }

    if (websocketParsing)
    {
        // Minimal JSON for WebSocket (abbreviated, <128 characters)
        // "u": username
        if (!append_to_buffer(&json, &json_size, &json_capacity, "\"u\":\"") ||
            !append_to_buffer(&json, &json_size, &json_capacity, entity->name) ||
            !append_to_buffer(&json, &json_size, &json_capacity, "\","))
        {
            free(json);
            return NULL;
        }

        // "xp": experience
        char buffer[32];
        snprintf(buffer, sizeof(buffer), "\"xp\":%.0f,", (double)entity->xp);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // "h": health
        snprintf(buffer, sizeof(buffer), "\"h\":%.0f,", (double)entity->health);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // "ehr": elapsed health regen (1 decimal)
        snprintf(buffer, sizeof(buffer), "\"ehr\":%.1f,", (double)entity->elapsed_health_regen);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // "eat": elapsed attack timer (1 decimal)
        snprintf(buffer, sizeof(buffer), "\"eat\":%.1f,", (double)entity->elapsed_attack_timer);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // "d": direction (numeric code)
        snprintf(buffer, sizeof(buffer), "\"d\":%d,", direction_to_code(entity->direction));
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // "s": state (numeric code)
        snprintf(buffer, sizeof(buffer), "\"s\":%d,", entity->state);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // instead of start position, send the current
        // "sp": start position object with x and y (1 decimal)
        snprintf(buffer, sizeof(buffer), "\"sp\":{\"x\":%.1f,\"y\":%.1f}",
                 (double)entity->position.x, (double)entity->position.y);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }
    }
    else
    {
        // Full JSON output (unchanged)
        // 1. Username
        if (!append_to_buffer(&json, &json_size, &json_capacity, "\"username\":\"") ||
            !append_to_buffer(&json, &json_size, &json_capacity, entity->name) ||
            !append_to_buffer(&json, &json_size, &json_capacity, "\","))
        {
            free(json);
            return NULL;
        }

        // 2. Level
        char buffer[64];
        snprintf(buffer, sizeof(buffer), "\"level\":%.0f,", (double)entity->level);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 3. XP
        snprintf(buffer, sizeof(buffer), "\"xp\":%.0f,", (double)entity->xp);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 4. Health
        snprintf(buffer, sizeof(buffer), "\"health\":%.0f,", (double)entity->health);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 5. Strength
        snprintf(buffer, sizeof(buffer), "\"strength\":%.0f,", (double)entity->strength);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 6. Max Health
        snprintf(buffer, sizeof(buffer), "\"max_health\":%.0f,", (double)entity->max_health);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 7. Health Regen
        snprintf(buffer, sizeof(buffer), "\"health_regen\":%.0f,", (double)entity->health_regen);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 8. Elapsed Health Regen
        snprintf(buffer, sizeof(buffer), "\"elapsed_health_regen\":%.6f,", (double)entity->elapsed_health_regen);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 9. Attack Timer
        snprintf(buffer, sizeof(buffer), "\"attack_timer\":%.6f,", (double)entity->attack_timer);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 10. Elapsed Attack Timer
        snprintf(buffer, sizeof(buffer), "\"elapsed_attack_timer\":%.6f,", (double)entity->elapsed_attack_timer);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 11. Direction (string representation)
        const char *direction_str;
        Vector dir = entity->direction;
        if (dir.x == 0 && dir.y == -1)
        {
            direction_str = "\"direction\":\"up\",";
        }
        else if (dir.x == 0 && dir.y == 1)
        {
            direction_str = "\"direction\":\"down\",";
        }
        else if (dir.x == -1 && dir.y == 0)
        {
            direction_str = "\"direction\":\"left\",";
        }
        else
        {
            direction_str = "\"direction\":\"right\",";
        }
        if (!append_to_buffer(&json, &json_size, &json_capacity, direction_str))
        {
            free(json);
            return NULL;
        }

        // 12. State (string representation)
        const char *state_str;
        switch (entity->state)
        {
        case ENTITY_IDLE:
            state_str = "\"state\":\"idle\",";
            break;
        case ENTITY_MOVING:
            state_str = "\"state\":\"moving\",";
            break;
        case ENTITY_ATTACKING:
            state_str = "\"state\":\"attacking\",";
            break;
        case ENTITY_ATTACKED:
            state_str = "\"state\":\"attacked\",";
            break;
        case ENTITY_DEAD:
            state_str = "\"state\":\"dead\",";
            break;
        default:
            state_str = "\"state\":\"unknown\",";
            break;
        }
        if (!append_to_buffer(&json, &json_size, &json_capacity, state_str))
        {
            free(json);
            return NULL;
        }

        // 13. Start Position X
        snprintf(buffer, sizeof(buffer), "\"start_position_x\":%.6f,", (double)entity->start_position.x);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 14. Start Position Y
        snprintf(buffer, sizeof(buffer), "\"start_position_y\":%.6f,", (double)entity->start_position.y);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 15. dx (direction x component)
        snprintf(buffer, sizeof(buffer), "\"dx\":%.0f,", (double)entity->direction.x);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }

        // 16. dy (direction y component)
        snprintf(buffer, sizeof(buffer), "\"dy\":%.0f", (double)entity->direction.y);
        if (!append_to_buffer(&json, &json_size, &json_capacity, buffer))
        {
            free(json);
            return NULL;
        }
    }

    if (!append_to_buffer(&json, &json_size, &json_capacity, "}"))
    {
        free(json);
        return NULL;
    }

    // For websocket, output only the minimal JSON
    if (websocketParsing)
    {
        return json;
    }
    else
    {
        // Allocate buffer for the wrapped JSON
        size_t wrapped_capacity = json_size + 256; // Extra space for wrapper
        char *json_data = (char *)malloc(wrapped_capacity);
        if (!json_data)
        {
            FURI_LOG_E(TAG, "Failed to allocate wrapped JSON string");
            free(json);
            return NULL;
        }

        snprintf(json_data, wrapped_capacity, "{\"username\":\"%s\",\"game_stats\":%s}",
                 entity->name, json);
        free(json);
        return json_data;
    }
}

LevelIndex FlipWorldRun::getCurrentLevelIndex() const
{
    if (!engine)
    {
        FURI_LOG_E("FlipWorldRun", "Engine is not initialized");
        return LevelUnknown;
    }
    auto currentLevel = engine->getGame()->current_level;
    if (!currentLevel)
    {
        FURI_LOG_E("FlipWorldRun", "Current level is not set");
        return LevelUnknown;
    }
    if (strcmp(currentLevel->name, "Home Woods") == 0)
    {
        return LevelHomeWoods;
    }
    if (strcmp(currentLevel->name, "Rock World") == 0)
    {
        return LevelRockWorld;
    }
    if (strcmp(currentLevel->name, "Forest World") == 0)
    {
        return LevelForestWorld;
    }
    FURI_LOG_E("FlipWorldRun", "Unknown level name: %s", currentLevel->name);
    return LevelUnknown;
}

IconSpec FlipWorldRun::getIconSpec(const char *name) const
{
    if (strcmp(name, "house") == 0)
        return (IconSpec){.id = ICON_ID_HOUSE, .icon = icon_house_48x32px, .pos = Vector(0, 0), .size = (Vector){48, 32}};
    if (strcmp(name, "plant") == 0)
        return (IconSpec){.id = ICON_ID_PLANT, .icon = icon_plant_16x16, .pos = Vector(0, 0), .size = (Vector){16, 16}};
    if (strcmp(name, "tree") == 0)
        return (IconSpec){.id = ICON_ID_TREE, .icon = icon_tree_16x16, .pos = Vector(0, 0), .size = (Vector){16, 16}};
    if (strcmp(name, "fence") == 0)
        return (IconSpec){.id = ICON_ID_FENCE, .icon = icon_fence_16x8px, .pos = Vector(0, 0), .size = (Vector){16, 8}};
    if (strcmp(name, "flower") == 0)
        return (IconSpec){.id = ICON_ID_FLOWER, .icon = icon_flower_16x16, .pos = Vector(0, 0), .size = (Vector){16, 16}};
    if (strcmp(name, "rock_large") == 0)
        return (IconSpec){.id = ICON_ID_ROCK_LARGE, .icon = icon_rock_large_18x19px, .pos = Vector(0, 0), .size = (Vector){18, 19}};
    if (strcmp(name, "rock_medium") == 0)
        return (IconSpec){.id = ICON_ID_ROCK_MEDIUM, .icon = icon_rock_medium_16x14px, .pos = Vector(0, 0), .size = (Vector){16, 14}};
    if (strcmp(name, "rock_small") == 0)
        return (IconSpec){.id = ICON_ID_ROCK_SMALL, .icon = icon_rock_small_10x8px, .pos = Vector(0, 0), .size = (Vector){10, 8}};

    return (IconSpec){.id = ICON_ID_INVALID, .icon = NULL, .pos = Vector(0, 0), .size = (Vector){0, 0}};
}

const char *FlipWorldRun::getLevelJson(LevelIndex index) const
{
    switch (index)
    {
    case LevelHomeWoods:
        return "{"
               "\"name\" : \"home_woods_v8\","
               "\"author\" : \"ChatGPT\","
               "\"json_data\" : ["
               "{\"i\" : \"rock_medium\", \"x\" : 100, \"y\" : 100, \"a\" : 10, \"h\" : true},"
               "{\"i\" : \"rock_medium\", \"x\" : 400, \"y\" : 300, \"a\" : 6, \"h\" : true},"
               "{\"i\" : \"rock_small\", \"x\" : 600, \"y\" : 200, \"a\" : 8, \"h\" : true},"
               "{\"i\" : \"fence\", \"x\" : 50, \"y\" : 50, \"a\" : 10, \"h\" : true},"
               "{\"i\" : \"fence\", \"x\" : 250, \"y\" : 150, \"a\" : 12, \"h\" : true},"
               "{\"i\" : \"fence\", \"x\" : 550, \"y\" : 350, \"a\" : 12, \"h\" : true},"
               "{\"i\" : \"rock_large\", \"x\" : 400, \"y\" : 70, \"a\" : 12, \"h\" : true},"
               "{\"i\" : \"rock_large\", \"x\" : 200, \"y\" : 200, \"a\" : 6, \"h\" : false},"
               "{\"i\" : \"tree\", \"x\" : 5, \"y\" : 5, \"a\" : 45, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 5, \"y\" : 5, \"a\" : 20, \"h\" : false},"
               "{\"i\" : \"tree\", \"x\" : 22, \"y\" : 22, \"a\" : 44, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 22, \"y\" : 22, \"a\" : 20, \"h\" : false},"
               "{\"i\" : \"tree\", \"x\" : 5, \"y\" : 347, \"a\" : 45, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 5, \"y\" : 364, \"a\" : 45, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 735, \"y\" : 37, \"a\" : 18, \"h\" : false},"
               "{\"i\" : \"tree\", \"x\" : 752, \"y\" : 37, \"a\" : 18, \"h\" : false}"
               "]"
               "}";
    case LevelRockWorld:
        return "{"
               "\"name\" : \"rock_world_v8\","
               "\"author\" : \"ChatGPT\","
               "\"json_data\" : ["
               "{\"i\" : \"house\", \"x\" : 100, \"y\" : 50, \"a\" : 1, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 650, \"y\" : 420, \"a\" : 5, \"h\" : true},"
               "{\"i\" : \"rock_large\", \"x\" : 150, \"y\" : 150, \"a\" : 2, \"h\" : true},"
               "{\"i\" : \"rock_medium\", \"x\" : 210, \"y\" : 80, \"a\" : 3, \"h\" : true},"
               "{\"i\" : \"rock_small\", \"x\" : 480, \"y\" : 110, \"a\" : 4, \"h\" : false},"
               "{\"i\" : \"flower\", \"x\" : 280, \"y\" : 140, \"a\" : 3, \"h\" : true},"
               "{\"i\" : \"plant\", \"x\" : 120, \"y\" : 130, \"a\" : 2, \"h\" : true},"
               "{\"i\" : \"rock_large\", \"x\" : 400, \"y\" : 200, \"a\" : 3, \"h\" : true},"
               "{\"i\" : \"rock_medium\", \"x\" : 600, \"y\" : 50, \"a\" : 5, \"h\" : false},"
               "{\"i\" : \"rock_small\", \"x\" : 500, \"y\" : 100, \"a\" : 6, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 650, \"y\" : 20, \"a\" : 4, \"h\" : true},"
               "{\"i\" : \"flower\", \"x\" : 550, \"y\" : 250, \"a\" : 8, \"h\" : true},"
               "{\"i\" : \"plant\", \"x\" : 300, \"y\" : 300, \"a\" : 5, \"h\" : true},"
               "{\"i\" : \"rock_large\", \"x\" : 700, \"y\" : 180, \"a\" : 2, \"h\" : true},"
               "{\"i\" : \"tree\", \"x\" : 50, \"y\" : 300, \"a\" : 10, \"h\" : true},"
               "{\"i\" : \"flower\", \"x\" : 350, \"y\" : 100, \"a\" : 7, \"h\" : true}"
               "]"
               "}";
    case LevelForestWorld:
        return "{"
               "\"name\": \"forest_world_v8\","
               "\"author\": \"ChatGPT\","
               "\"json_data\": ["
               "{\"i\": \"rock_small\", \"x\": 120, \"y\": 20, \"a\": 10, \"h\": false},"
               "{\"i\": \"tree\", \"x\": 50, \"y\": 50, \"a\": 10, \"h\": true},"
               "{\"i\": \"flower\", \"x\": 200, \"y\": 70, \"a\": 8, \"h\": false},"
               "{\"i\": \"rock_small\", \"x\": 250, \"y\": 100, \"a\": 8, \"h\": true},"
               "{\"i\": \"rock_medium\", \"x\": 300, \"y\": 140, \"a\": 2, \"h\": true},"
               "{\"i\": \"plant\", \"x\": 50, \"y\": 300, \"a\": 10, \"h\": true},"
               "{\"i\": \"rock_large\", \"x\": 650, \"y\": 250, \"a\": 3, \"h\": true},"
               "{\"i\": \"flower\", \"x\": 300, \"y\": 350, \"a\": 5, \"h\": true},"
               "{\"i\": \"tree\", \"x\": 20, \"y\": 150, \"a\": 10, \"h\": false},"
               "{\"i\": \"tree\", \"x\": 5, \"y\": 5, \"a\": 45, \"h\": true},"
               "{\"i\": \"tree\", \"x\": 5, \"y\": 5, \"a\": 20, \"h\": false},"
               "{\"i\": \"tree\", \"x\": 22, \"y\": 22, \"a\": 44, \"h\": true},"
               "{\"i\": \"tree\", \"x\": 22, \"y\": 22, \"a\": 20, \"h\": false},"
               "{\"i\": \"tree\", \"x\": 5, \"y\": 347, \"a\": 45, \"h\": true},"
               "{\"i\": \"tree\", \"x\": 5, \"y\": 364, \"a\": 45, \"h\": true},"
               "{\"i\": \"tree\", \"x\": 735, \"y\": 37, \"a\": 18, \"h\": false},"
               "{\"i\": \"tree\", \"x\": 752, \"y\": 37, \"a\": 18, \"h\": false}"
               "]"
               "}";
    default:
        FURI_LOG_E("FlipWorldRun", "Unknown level index: %d", index);
        return nullptr;
    }
}

std::unique_ptr<Level> FlipWorldRun::getLevel(LevelIndex index, Game *game) const
{
    std::unique_ptr<Level> level = std::make_unique<Level>(getLevelName(index), Vector(768, 384), game ? game : engine->getGame());
    if (!level)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to create Level object");
        return nullptr;
    }
    switch (index)
    {
    case LevelHomeWoods:
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(350, 210), Vector(390, 210), 2.0f, 30.0f, 0.4f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(200, 320), Vector(220, 320), 0.5f, 45.0f, 0.6f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(100, 80), Vector(180, 85), 2.2f, 55.0f, 0.5f, 30.0f, 300.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(400, 50), Vector(490, 50), 1.7f, 35.0f, 1.0f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Funny NPC", ENTITY_NPC, Vector(350, 180), Vector(350, 180), 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).release());
        break;
    case LevelRockWorld:
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(180, 80), Vector(160, 80), 1.0f, 32.0f, 0.5f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(220, 140), Vector(200, 140), 1.5f, 20.0f, 1.0f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(400, 200), Vector(450, 200), 2.0f, 15.0f, 1.2f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(600, 150), Vector(580, 150), 1.8f, 28.0f, 1.0f, 40.0f, 400.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(500, 250), Vector(480, 250), 1.2f, 30.0f, 0.6f, 10.0f, 100.0f).release());
        break;
    case LevelForestWorld:
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(50, 120), Vector(100, 120), 1.0f, 30.0f, 0.5f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(300, 60), Vector(250, 60), 1.5f, 20.0f, 0.8f, 30.0f, 300.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(400, 200), Vector(450, 200), 1.7f, 15.0f, 1.0f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(700, 150), Vector(650, 150), 1.2f, 25.0f, 0.6f, 10.0f, 100.0f).release());
        level->entity_add(std::make_unique<Sprite>("Cyclops", ENTITY_ENEMY, Vector(200, 300), Vector(250, 300), 2.0f, 18.0f, 0.9f, 20.0f, 200.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ogre", ENTITY_ENEMY, Vector(300, 300), Vector(350, 300), 1.5f, 15.0f, 1.2f, 50.0f, 500.0f).release());
        level->entity_add(std::make_unique<Sprite>("Ghost", ENTITY_ENEMY, Vector(500, 200), Vector(550, 200), 1.3f, 20.0f, 0.7f, 40.0f, 400.0f).release());
        break;
    default:
        break;
    };
    return level;
}

const char *FlipWorldRun::getLevelName(LevelIndex index) const
{
    switch (index)
    {
    case LevelHomeWoods:
        return "Home Woods";
    case LevelRockWorld:
        return "Rock World";
    case LevelForestWorld:
        return "Forest World";
    default:
        return "Unknown Level";
    }
}

size_t FlipWorldRun::getMemoryUsage() const
{
    size_t totalMemory = 0;

    // Count chunked message memory
    for (size_t i = 0; i < MAX_CHUNKED_MESSAGES; i++)
    {
        if (chunkedMessages[i].data)
        {
            totalMemory += chunkedMessages[i].dataCapacity;
        }
    }

    // Count queued message memory
    for (size_t i = 0; i < MAX_QUEUED_MESSAGES; i++)
    {
        if (messageQueue[i].message)
        {
            totalMemory += messageQueue[i].messageLen + 1; // +1 for null terminator
        }
    }

    // Count icon group memory if allocated
    if (currentIconGroup && currentIconGroup->icons)
    {
        totalMemory += currentIconGroup->count * sizeof(IconSpec);
    }

    return totalMemory;
}

bool FlipWorldRun::handleChunkedMessage(const char *message)
{
    if (!message || strlen(message) == 0)
    {
        return false;
    }

    // Look for "id": pattern
    const char *idPos = strstr(message, "\"id\":");
    if (!idPos)
    {
        return false; // Not a chunked message
    }

    // Extract id value (skip "id": and any whitespace)
    idPos += 5; // skip "id":
    while (*idPos == ' ' || *idPos == '\t')
        idPos++; // skip whitespace

    int messageId = 0;
    if (sscanf(idPos, "%d", &messageId) != 1)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to parse id value");
        return true;
    }

    // Look for "seq": pattern
    const char *seqPos = strstr(message, "\"seq\":");
    if (!seqPos)
    {
        FURI_LOG_E("FlipWorldRun", "No seq field found");
        return true;
    }

    seqPos += 6; // skip "seq":
    while (*seqPos == ' ' || *seqPos == '\t')
        seqPos++; // skip whitespace

    int seqVal = 0;
    if (sscanf(seqPos, "%d", &seqVal) != 1)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to parse seq value");
        return true;
    }

    // Look for "total": pattern
    const char *totalPos = strstr(message, "\"total\":");
    if (!totalPos)
    {
        FURI_LOG_E("FlipWorldRun", "No total field found");
        return true;
    }

    totalPos += 8; // skip "total":
    while (*totalPos == ' ' || *totalPos == '\t')
        totalPos++; // skip whitespace

    int totalChunks = 0;
    if (sscanf(totalPos, "%d", &totalChunks) != 1)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to parse total value");
        return true;
    }

    // Look for "data": pattern and extract the string value
    const char *dataPos = strstr(message, "\"data\":");
    if (!dataPos)
    {
        FURI_LOG_E("FlipWorldRun", "No data field found");
        return true;
    }

    dataPos += 7; // skip "data":
    while (*dataPos == ' ' || *dataPos == '\t')
        dataPos++; // skip whitespace

    if (*dataPos != '"')
    {
        FURI_LOG_E("FlipWorldRun", "Data field is not a string");
        return true;
    }

    dataPos++; // skip opening quote

    // The data field contains unescaped JSON, so we need to find the end carefully
    // Look for the last quote in the message, which should be the closing quote of the data field
    const char *messageEnd = message + strlen(message);
    const char *dataEnd = messageEnd - 1; // start from the end

    // Move backwards to find the last quote, skipping the final }
    while (dataEnd > dataPos && *dataEnd != '"')
    {
        dataEnd--;
    }

    if (dataEnd <= dataPos || *dataEnd != '"')
    {
        FURI_LOG_E("FlipWorldRun", "Could not find end quote for data field");
        return true;
    }

    size_t dataLen = dataEnd - dataPos;
    char *dataStr = (char *)malloc(dataLen + 1);
    if (!dataStr)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to allocate memory for data string");
        return true;
    }

    memcpy(dataStr, dataPos, dataLen);
    dataStr[dataLen] = '\0';

    // if we're already using too much memory, reject new chunked messages
    size_t currentMemoryUsage = getMemoryUsage();
    size_t freeHeap = memmgr_get_free_heap();
    const size_t MEMORY_PRESSURE_THRESHOLD = 12288;
    const size_t CRITICAL_HEAP_THRESHOLD = 8192;

    if (currentMemoryUsage > MEMORY_PRESSURE_THRESHOLD || freeHeap < CRITICAL_HEAP_THRESHOLD)
    {
        FURI_LOG_E("FlipWorldRun", "Memory pressure (used: %zu bytes, free heap: %zu bytes), rejecting chunked message",
                   currentMemoryUsage, freeHeap);
        free(dataStr);
        return true; // Consumed but rejected
    }

    // Validate chunk parameters
    const uint16_t MAX_REASONABLE_CHUNKS = 6;
    const size_t MAX_CHUNK_DATA_SIZE = 50;

    if (totalChunks == 0 || totalChunks > MAX_REASONABLE_CHUNKS)
    {
        FURI_LOG_E("FlipWorldRun", "Invalid totalChunks: %d (max: %d)", totalChunks, MAX_REASONABLE_CHUNKS);
        free(dataStr);
        return true;
    }

    // Clean up expired messages first
    cleanupExpiredChunkedMessages();

    // Find existing chunked message or create new one
    ChunkedMessage *chunkedMsg = nullptr;
    for (size_t i = 0; i < MAX_CHUNKED_MESSAGES; i++)
    {
        if (chunkedMessages[i].id == messageId && chunkedMessages[i].data)
        {
            chunkedMsg = &chunkedMessages[i];
            break;
        }
    }

    // If not found, create new one
    if (!chunkedMsg)
    {
        // Check total memory usage before creating new chunked message
        size_t totalMemoryUsed = 0;
        for (size_t j = 0; j < MAX_CHUNKED_MESSAGES; j++)
        {
            if (chunkedMessages[j].data)
            {
                totalMemoryUsed += chunkedMessages[j].dataCapacity;
            }
        }

        const size_t MAX_TOTAL_CHUNK_MEMORY = 6144;
        size_t estimatedNewSize = totalChunks * MAX_CHUNK_DATA_SIZE;

        if (totalMemoryUsed + estimatedNewSize > MAX_TOTAL_CHUNK_MEMORY)
        {
            FURI_LOG_E("FlipWorldRun", "Would exceed total chunk memory limit (%zu + %zu > %zu)",
                       totalMemoryUsed, estimatedNewSize, MAX_TOTAL_CHUNK_MEMORY);
            free(dataStr);
            return true;
        }

        for (size_t i = 0; i < MAX_CHUNKED_MESSAGES; i++)
        {
            if (!chunkedMessages[i].data)
            {
                chunkedMsg = &chunkedMessages[i];
                chunkedMsg->id = messageId;
                chunkedMsg->totalChunks = totalChunks;
                chunkedMsg->receivedChunks = 0;
                chunkedMsg->dataSize = 0;
                chunkedMsg->dataCapacity = (totalChunks * MAX_CHUNK_DATA_SIZE < 512) ? 512 : totalChunks * MAX_CHUNK_DATA_SIZE;
                chunkedMsg->data = (char *)malloc(chunkedMsg->dataCapacity);
                if (!chunkedMsg->data)
                {
                    FURI_LOG_E("FlipWorldRun", "Failed to allocate chunked message buffer");
                    free(dataStr);
                    return true;
                }
                chunkedMsg->data[0] = '\0';
                chunkedMessageCount++;
                break;
            }
        }
    }

    if (!chunkedMsg)
    {
        FURI_LOG_E("FlipWorldRun", "No available slots for chunked message");
        free(dataStr);
        return true;
    }

    // Update last update time
    chunkedMsg->lastUpdateTime = furi_get_tick();

    // Append data to the chunked message
    if (dataLen > 120)
    {
        FURI_LOG_E("FlipWorldRun", "Chunk data too large: %zu bytes (max: 120)", dataLen);
        free(dataStr);
        return true;
    }

    // ensure we have enough heap memory for the operation
    size_t chunkProcessHeap = memmgr_get_free_heap();
    if (chunkProcessHeap < 8192) // Need at least 8KB free heap for safe chunk processing
    {
        FURI_LOG_E("FlipWorldRun", "Insufficient heap memory for chunk processing: %zu bytes free", chunkProcessHeap);
        free(dataStr);
        return true;
    }

    // Check if buffer expansion is needed
    if (chunkedMsg->dataSize + dataLen >= chunkedMsg->dataCapacity)
    {
        const size_t MAX_MESSAGE_SIZE = 2048;
        size_t newCapacity = chunkedMsg->dataCapacity + 512;

        // Cap the maximum size to prevent runaway growth
        if (newCapacity > MAX_MESSAGE_SIZE)
        {
            FURI_LOG_E("FlipWorldRun", "Message too large, would exceed %zu bytes", MAX_MESSAGE_SIZE);
            free(dataStr);
            return true;
        }

        // Additional safety check before realloc
        size_t reallocHeap = memmgr_get_free_heap();
        if (reallocHeap < newCapacity + 4096) // Need buffer capacity + 4KB safety margin
        {
            FURI_LOG_E("FlipWorldRun", "Insufficient heap for buffer expansion: need %zu, have %zu",
                       newCapacity + 4096, reallocHeap);
            free(dataStr);
            return true;
        }

        char *newData = (char *)realloc(chunkedMsg->data, newCapacity);
        if (!newData)
        {
            FURI_LOG_E("FlipWorldRun", "Failed to expand chunked message buffer to %zu bytes", newCapacity);
            free(dataStr);
            return true;
        }
        chunkedMsg->data = newData;
        chunkedMsg->dataCapacity = newCapacity;
    }

    // Additional bounds checking before memcpy
    if (chunkedMsg->dataSize + dataLen >= chunkedMsg->dataCapacity)
    {
        FURI_LOG_E("FlipWorldRun", "Buffer overflow prevention: dataSize %zu + dataLen %zu >= capacity %zu",
                   chunkedMsg->dataSize, dataLen, chunkedMsg->dataCapacity);
        free(dataStr);
        return true;
    }

    // Append the chunk data safely
    memcpy(chunkedMsg->data + chunkedMsg->dataSize, dataStr, dataLen);
    chunkedMsg->dataSize += dataLen;

    // Ensure we don't write past buffer end
    if (chunkedMsg->dataSize < chunkedMsg->dataCapacity)
    {
        chunkedMsg->data[chunkedMsg->dataSize] = '\0'; // Null terminate
    }
    else
    {
        FURI_LOG_E("FlipWorldRun", "Cannot null-terminate: dataSize %zu >= capacity %zu",
                   chunkedMsg->dataSize, chunkedMsg->dataCapacity);
        free(dataStr);
        return true;
    }

    chunkedMsg->receivedChunks++;

    // Free the dataStr since we've copied it to the chunked message buffer
    free(dataStr);

    // Check if we have all chunks
    if (chunkedMsg->receivedChunks >= chunkedMsg->totalChunks)
    {
        // Validate the complete message before processing
        if (chunkedMsg->data && chunkedMsg->dataSize > 0)
        {
            // Ensure null termination
            chunkedMsg->data[chunkedMsg->dataSize] = '\0';

            // Basic JSON validation - must start with { and end with }
            if (chunkedMsg->data[0] == '{' && chunkedMsg->data[chunkedMsg->dataSize - 1] == '}')
            {
                // Check memory before processing to prevent crashes
                size_t freeHeap = memmgr_get_free_heap();
                if (freeHeap > 8192) // Only process if we have at least 8KB free heap
                {
                    processCompleteMultiplayerMessage(chunkedMsg->data);
                }
                else
                {
                    FURI_LOG_W("FlipWorldRun", "Skipping chunked message processing due to low memory: %zu bytes free", freeHeap);
                }
            }
        }

        // Clean up this chunked message
        free(chunkedMsg->data);
        chunkedMsg->data = nullptr;
        chunkedMsg->id = 0;
        chunkedMsg->totalChunks = 0;
        chunkedMsg->receivedChunks = 0;
        chunkedMsg->dataSize = 0;
        chunkedMsg->dataCapacity = 0;
        chunkedMsg->lastUpdateTime = 0;
        if (chunkedMessageCount > 0)
        {
            chunkedMessageCount--;
        }
    }

    return true;
}

void FlipWorldRun::handleIncomingMultiplayerData(const char *message)
{
    // Only handle in PvE mode
    if (!isPvEMode || !message)
    {
        return;
    }

    // Check if message is empty or too short
    if (strlen(message) < 10)
    {
        FURI_LOG_E("FlipWorldRun", "Received very short message: '%s'", message);
        return;
    }

    // First check if this is a chunked message
    if (handleChunkedMessage(message))
    {
        return; // Message was handled as a chunk (or completed chunk assembly)
    }

    // If not a chunk, process as complete message
    processCompleteMultiplayerMessage(message);
}

void FlipWorldRun::inputManager()
{
    static int inputHeldCounter = 0;

    // Track input held state
    if (lastInput != InputKeyMAX)
    {
        inputHeldCounter++;
        if (inputHeldCounter > 10)
        {
            this->inputHeld = true;
        }
    }
    else
    {
        inputHeldCounter = 0;
        this->inputHeld = false;
    }

    if (shouldDebounce)
    {
        debounceInput();
    }

    // Pass input to player for processing
    if (player)
    {
        player->setInputKey(lastInput);
        player->processInput();
    }
}

bool FlipWorldRun::parseEntityDataFromJson(Entity *entity, const char *jsonData)
{
    if (!entity || !jsonData || strlen(jsonData) == 0)
    {
        return false;
    }

    // Basic JSON validation first
    if (jsonData[0] != '{' || jsonData[strlen(jsonData) - 1] != '}')
    {
        FURI_LOG_E("FlipWorldRun", "Invalid JSON format for entity data");
        return false;
    }

    // Memory safety check before parsing
    size_t freeHeap = memmgr_get_free_heap();
    if (freeHeap < 4096) // Need at least 4KB free for JSON parsing
    {
        FURI_LOG_W("FlipWorldRun", "Skipping entity JSON parsing due to low memory: %zu bytes free", freeHeap);
        return false;
    }

    // Parse username and verify it matches
    char *u = get_json_value("u", jsonData);
    if (!u)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to get username from JSON");
        return false;
    }

    // Check if the username matches
    if (strcmp(u, entity->name) != 0)
    {
        FURI_LOG_E("FlipWorldRun", "Username mismatch: expected %s, got %s", entity->name, u);
        free(u);
        return false;
    }
    free(u);

    // Parse entity data
    char *h = get_json_value("h", jsonData);
    char *eat = get_json_value("eat", jsonData);
    char *d = get_json_value("d", jsonData);
    char *xp = get_json_value("xp", jsonData);
    char *sp = get_json_value("sp", jsonData);
    char *x = get_json_value("x", sp);
    char *y = get_json_value("y", sp);

    if (!h || !eat || !d || !sp || !x || !y || !xp)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to parse entity data fields");
        if (h)
            free(h);
        if (eat)
            free(eat);
        if (d)
            free(d);
        if (sp)
            free(sp);
        if (x)
            free(x);
        if (y)
            free(y);
        if (xp)
            free(xp);
        return false;
    }

    // Update entity with parsed data
    entity->health = (float)atoi(h);
    if (entity->health <= 0)
    {
        entity->health = 0;
        entity->state = ENTITY_DEAD;
        entity->position_set((Vector){-100, -100});
        free(h);
        free(eat);
        free(d);
        free(sp);
        free(x);
        free(y);
        free(xp);
        return true;
    }

    entity->elapsed_attack_timer = atof_(eat);

    switch (atoi(d))
    {
    case 0:
        entity->direction = ENTITY_LEFT;
        break;
    case 1:
        entity->direction = ENTITY_RIGHT;
        break;
    case 2:
        entity->direction = ENTITY_UP;
        break;
    case 3:
        entity->direction = ENTITY_DOWN;
        break;
    default:
        entity->direction = ENTITY_RIGHT;
        break;
    }

    entity->xp = (atoi)(xp);
    entity->level = 1;
    uint32_t xp_required = 100;
    while (entity->level < 100 && entity->xp >= xp_required)
    {
        entity->level++;
        xp_required = (uint32_t)(xp_required * 1.5);
    }

    // Set position
    entity->position_set(Vector(atof_(x), atof_(y)));

    // Free the strings
    free(h);
    free(eat);
    free(d);
    free(sp);
    free(x);
    free(y);
    free(xp);
    return true;
}

void FlipWorldRun::processCompleteMultiplayerMessage(const char *message)
{
    if (!engine || !engine->getGame() || !engine->getGame()->current_level)
    {
        return;
    }

    // ensure message looks like valid JSON
    if (!message || strlen(message) < 10 || message[0] != '{' || message[strlen(message) - 1] != '}')
    {
        FURI_LOG_E("FlipWorldRun", "Invalid message format: not proper JSON structure");
        return;
    }

    // check for malformed JSON fragments
    if (strstr(message, "\"seq\"") != NULL && strstr(message, "\"total\"") != NULL && strstr(message, "\"data\"") != NULL)
    {
        FURI_LOG_E("FlipWorldRun", "Attempted to process chunked message fragment as complete message");
        return;
    }

    // Reject messages that contain chunked message fragments or malformed data
    if (strstr(message, "Rend") != NULL || strstr(message, "total\":") != NULL ||
        strstr(message, "seq\":") != NULL || strstr(message, "},\"") == message ||
        strncmp(message, "\"", 1) == 0)
    {
        FURI_LOG_E("FlipWorldRun", "Rejecting malformed message fragment: %.100s...", message);
        return;
    }

    auto currentLevel = engine->getGame()->current_level;

    // Parse message type (we already validated it exists in processMultiplayerUpdate)
    char *messageType = get_json_value("type", message);
    if (!messageType)
    {
        FURI_LOG_E("FlipWorldRun", "No 'type' field in message - may be malformed JSON");
        FURI_LOG_E("FlipWorldRun", "Message content: %.200s%s", message, strlen(message) > 200 ? "..." : "");
        return;
    }

    // Additional validation for message type
    if (strlen(messageType) == 0 || strlen(messageType) > 20 ||
        strstr(messageType, "\"") != NULL || strstr(messageType, "}") != NULL)
    {
        FURI_LOG_E("FlipWorldRun", "Invalid message type: '%s'", messageType);
        free(messageType);
        return;
    }

    if (strcmp(messageType, "player") == 0)
    {
        // Handle player update
        char *playerData = get_json_value("data", message);
        if (playerData)
        {

            // Find the player entity by username
            char *username = get_json_value("u", playerData);
            if (username)
            {

                // Validate username before processing - reject invalid/malformed usernames
                if (strlen(username) == 0 || strlen(username) > 50 ||
                    strstr(username, "\"") != NULL || strstr(username, "}") != NULL ||
                    strstr(username, "{") != NULL || strstr(username, "total") != NULL ||
                    strstr(username, "data") != NULL || strstr(username, "seq") != NULL ||
                    strstr(username, "Rend") != NULL || strstr(username, ",") != NULL ||
                    strstr(username, ":") != NULL || strstr(username, "[") != NULL ||
                    strstr(username, "]") != NULL || username[0] == ' ' ||
                    username[strlen(username) - 1] == ' ' || isdigit(username[0]) ||
                    strlen(username) < 3 || // Require at least 3 characters for valid username
                    strstr(username, "type") != NULL || strstr(username, "player") != NULL ||
                    strstr(username, "chunk") != NULL || strstr(username, "id") != NULL)
                {
                    FURI_LOG_W("FlipWorldRun", "Rejecting invalid username: '%s' (len: %zu)", username, strlen(username));
                    free(username);
                    free(playerData);
                    free(messageType);
                    return;
                }

                // Don't update self
                if (strcmp(username, player->name) == 0 && strcmp(username, "Player") != 0)
                {
                    free(username);
                    free(playerData);
                    free(messageType);
                    return;
                }

                Entity *playerEntity = nullptr;
                for (int i = 0; i < currentLevel->getEntityCount(); i++)
                {
                    Entity *entity = currentLevel->getEntity(i);
                    if (entity && entity->type == ENTITY_PLAYER &&
                        strcmp(entity->name, username) == 0)
                    {
                        playerEntity = entity;
                        break;
                    }
                }

                // If player doesn't exist, add them as a remote player
                if (!playerEntity)
                {
                    if (addRemotePlayer(username))
                    {
                        // Try to find the newly added player
                        for (int i = 0; i < currentLevel->getEntityCount(); i++)
                        {
                            Entity *entity = currentLevel->getEntity(i);
                            if (entity && entity->type == ENTITY_PLAYER &&
                                strcmp(entity->name, username) == 0)
                            {
                                playerEntity = entity;
                                break;
                            }
                        }
                    }
                }

                // Update the player entity with received data
                if (playerEntity)
                {
                    parseEntityDataFromJson(playerEntity, playerData);
                }

                free(username);
            }
            free(playerData);
        }
    }
    else if (strcmp(messageType, "enemy") == 0 && !isLobbyHost)
    {
        // Followers receive enemy updates from host
        char *enemyData = get_json_value("data", message);
        if (enemyData)
        {
            char *enemyName = get_json_value("u", enemyData);
            if (enemyName)
            {
                for (int i = 0; i < currentLevel->getEntityCount(); i++)
                {
                    Entity *entity = currentLevel->getEntity(i);
                    if (entity && entity->type == ENTITY_ENEMY &&
                        strcmp(entity->name, enemyName) == 0)
                    {
                        // Update the enemy entity with received data
                        parseEntityDataFromJson(entity, enemyData);
                        break;
                    }
                }
                free(enemyName);
            }
            free(enemyData);
        }
    }
    else if (strcmp(messageType, "level") == 0 && !isLobbyHost)
    {
        // Followers receive level change commands from host
        char *levelIndex = get_json_value("level_index", message);
        if (levelIndex)
        {
            int newLevelIndex = atoi(levelIndex);
            if (newLevelIndex >= 0 && newLevelIndex < 3 && getCurrentLevelIndex() != newLevelIndex) // Valid level indices
            {
                if (engine && engine->getGame())
                {
                    engine->getGame()->level_switch(newLevelIndex);
                    setIconGroup(static_cast<LevelIndex>(newLevelIndex));
                }
            }
            free(levelIndex);
        }
    }

    free(messageType);
}

void FlipWorldRun::processMultiplayerUpdate()
{
    // Always process the websocket message queue first
    processWebsocketMessageQueue();

    // Only process multiplayer logic in PvE mode
    if (!isPvEMode)
    {
        return;
    }

    static uint32_t lastCleanupTime = 0;
    static uint32_t lastMemoryMaintenance = 0;
    uint32_t currentTime = furi_get_tick();

    // cleanup every 500ms
    if (currentTime - lastCleanupTime > 500)
    {
        cleanupExpiredChunkedMessages();
        lastCleanupTime = currentTime;

        // Log memory usage periodically for debugging
        size_t memUsage = getMemoryUsage();
        size_t availableHeap = memmgr_get_free_heap();

        // detailed memory tracking
        static uint32_t lastMemoryLog = 0;
        if (memUsage > 10240 && (currentTime - lastMemoryLog > 2000)) // Log at most every 2 seconds
        {
            FURI_LOG_W("FlipWorldRun", "High memory usage: %zu bytes (chunks: %zu, queue: %zu), free heap: %zu",
                       memUsage, chunkedMessageCount, queueSize, availableHeap);
            lastMemoryLog = currentTime;
        }

        // Force garbage collection if available heap is getting dangerously low
        if (availableHeap < 8192) // If less than 8KB free heap remaining
        {
            FURI_LOG_W("FlipWorldRun", "Critical memory situation: only %zu bytes free heap", availableHeap);

            // Emergency cleanup
            // Clear ALL chunked messages immediately
            for (size_t i = 0; i < MAX_CHUNKED_MESSAGES; i++)
            {
                if (chunkedMessages[i].data)
                {
                    FURI_LOG_W("FlipWorldRun", "Emergency clearing chunked message ID %d (%zu bytes)",
                               chunkedMessages[i].id, chunkedMessages[i].dataCapacity);
                    free(chunkedMessages[i].data);
                    chunkedMessages[i].data = nullptr;
                    chunkedMessages[i].id = 0;
                    chunkedMessages[i].totalChunks = 0;
                    chunkedMessages[i].receivedChunks = 0;
                    chunkedMessages[i].dataSize = 0;
                    chunkedMessages[i].dataCapacity = 0;
                    chunkedMessages[i].lastUpdateTime = 0;
                }
            }
            chunkedMessageCount = 0;

            // Clear most of the message queue, keep only the most recent 5 messages
            while (queueSize > 5)
            {
                QueuedMessage *oldMsg = &messageQueue[queueHead];
                if (oldMsg->message)
                {
                    free(oldMsg->message);
                    oldMsg->message = nullptr;
                    oldMsg->messageLen = 0;
                }
                queueHead = (queueHead + 1) % MAX_QUEUED_MESSAGES;
                queueSize--;
            }

            FURI_LOG_I("FlipWorldRun", "Emergency cleanup complete, free heap now: %zu", memmgr_get_free_heap());
        }
        else if (memUsage > 12288) // If using more than 12KB, force aggressive cleanup
        {
            FURI_LOG_W("FlipWorldRun", "Emergency memory cleanup triggered at %zu bytes", memUsage);

            // Force cleanup of all but the most recent chunked messages
            for (size_t i = 0; i < MAX_CHUNKED_MESSAGES; i++)
            {
                if (chunkedMessages[i].data && chunkedMessageCount > 2) // Keep only 2 most recent
                {
                    // Find oldest message to clean
                    uint32_t oldestTime = currentTime;
                    size_t oldestIndex = SIZE_MAX;

                    for (size_t j = 0; j < MAX_CHUNKED_MESSAGES; j++)
                    {
                        if (chunkedMessages[j].data && chunkedMessages[j].lastUpdateTime < oldestTime)
                        {
                            oldestTime = chunkedMessages[j].lastUpdateTime;
                            oldestIndex = j;
                        }
                    }

                    if (oldestIndex != SIZE_MAX)
                    {
                        FURI_LOG_W("FlipWorldRun", "Emergency cleanup of chunked message ID %d",
                                   chunkedMessages[oldestIndex].id);
                        free(chunkedMessages[oldestIndex].data);
                        chunkedMessages[oldestIndex].data = nullptr;
                        chunkedMessages[oldestIndex].id = 0;
                        chunkedMessages[oldestIndex].totalChunks = 0;
                        chunkedMessages[oldestIndex].receivedChunks = 0;
                        chunkedMessages[oldestIndex].dataSize = 0;
                        chunkedMessages[oldestIndex].dataCapacity = 0;
                        chunkedMessages[oldestIndex].lastUpdateTime = 0;
                        if (chunkedMessageCount > 0)
                        {
                            chunkedMessageCount--;
                        }
                    }
                }
            }

            // trim the queue if it's large
            while (queueSize > MAX_QUEUED_MESSAGES * 0.7)
            {
                QueuedMessage *oldMsg = &messageQueue[queueHead];
                if (oldMsg->message)
                {
                    free(oldMsg->message);
                    oldMsg->message = nullptr;
                    oldMsg->messageLen = 0;
                }
                queueHead = (queueHead + 1) % MAX_QUEUED_MESSAGES;
                queueSize--;
            }
        }
    }

    //  periodic memory maintenance every 2 seconds
    if (currentTime - lastMemoryMaintenance > 2000)
    {
        lastMemoryMaintenance = currentTime;
        size_t freeHeap = memmgr_get_free_heap();

        // If free heap is getting low, clean it up gang
        if (freeHeap < 15360)
        {
            FURI_LOG_W("FlipWorldRun", "Preventive memory cleanup: free heap %zu bytes", freeHeap);

            // Trim queue if it's getting large
            while (queueSize > MAX_QUEUED_MESSAGES * 0.5) // Keep queue at 50% max
            {
                QueuedMessage *oldMsg = &messageQueue[queueHead];
                if (oldMsg->message)
                {
                    free(oldMsg->message);
                    oldMsg->message = nullptr;
                    oldMsg->messageLen = 0;
                }
                queueHead = (queueHead + 1) % MAX_QUEUED_MESSAGES;
                queueSize--;
            }

            // Force cleanup of any chunked messages older than 2 seconds
            for (size_t i = 0; i < MAX_CHUNKED_MESSAGES; i++)
            {
                if (chunkedMessages[i].data &&
                    (currentTime - chunkedMessages[i].lastUpdateTime) > 2000)
                {
                    FURI_LOG_W("FlipWorldRun", "Preventive cleanup of chunked message ID %d", chunkedMessages[i].id);
                    free(chunkedMessages[i].data);
                    chunkedMessages[i].data = nullptr;
                    chunkedMessages[i].id = 0;
                    chunkedMessages[i].totalChunks = 0;
                    chunkedMessages[i].receivedChunks = 0;
                    chunkedMessages[i].dataSize = 0;
                    chunkedMessages[i].dataCapacity = 0;
                    chunkedMessages[i].lastUpdateTime = 0;
                    if (chunkedMessageCount > 0)
                    {
                        chunkedMessageCount--;
                    }
                }
            }
        }
    }

    // Send our state to other players
    static uint32_t lastSyncAttempt = 0;
    size_t currentMemUsage = getMemoryUsage();
    size_t freeHeap = memmgr_get_free_heap();

    // Adaptive sync frequency based on memory situation
    uint32_t adaptiveSyncInterval = syncInterval;
    if (freeHeap < 12288) // If less than 12KB free heap
    {
        adaptiveSyncInterval = syncInterval * 4; //  reduce sync frequency
    }
    else if (currentMemUsage > 12288 || queueSize > MAX_QUEUED_MESSAGES * 0.6) // If high memory usage or large queue
    {
        adaptiveSyncInterval = syncInterval * 2; // Double the sync interval
    }

    if (currentTime - lastSyncAttempt >= adaptiveSyncInterval)
    {
        syncMultiplayerState();
        lastSyncAttempt = currentTime;
    }

    // Check for incoming messages (with reduced frequency under memory pressure)
    FlipWorldApp *app = static_cast<FlipWorldApp *>(appContext);
    if (app)
    {
        const char *incomingMessage = app->getLastResponse();
        if (incomingMessage && strlen(incomingMessage) > 0)
        {
            // Skip processing if we're under severe memory pressure
            size_t freeHeap = memmgr_get_free_heap();
            if (freeHeap < 6144)
            {
                FURI_LOG_W("FlipWorldRun", "Skipping message processing due to low memory: %zu bytes free", freeHeap);
                app->clearLastResponse(); // Still clear to prevent buildup
                return;
            }

            // Only process messages that look like websocket messages (have "type" field or chunk metadata)
            bool isWebsocketMessage = false;

            //  check for proper JSON structure
            if (incomingMessage[0] == '{' && incomingMessage[strlen(incomingMessage) - 1] == '}')
            {
                // Check for chunked message first (has id, seq, total at top level)
                if (strstr(incomingMessage, "\"id\"") != NULL &&
                    strstr(incomingMessage, "\"seq\"") != NULL &&
                    strstr(incomingMessage, "\"total\"") != NULL)
                {
                    isWebsocketMessage = true;
                }
                // Check for complete message with top-level "type" field
                // Look for "type" near the beginning to avoid matching "type" in nested data
                else if (strstr(incomingMessage, "\"type\"") != NULL)
                {
                    // ensure "type" appears early in the message (within first 50 chars)
                    // to avoid matching "type" that appears in chunked data payload
                    const char *typePos = strstr(incomingMessage, "\"type\"");
                    if (typePos && (typePos - incomingMessage) < 50)
                    {
                        isWebsocketMessage = true;
                    }
                }
            }

            if (isWebsocketMessage)
            {
                handleIncomingMultiplayerData(incomingMessage);
            }

            app->clearLastResponse(); // Clear after processing
        }
    }
}

void FlipWorldRun::processWebsocketQueue(FlipWorldApp *app)
{
    if (!app || queueSize == 0)
    {
        return;
    }

    uint32_t currentTime = furi_get_tick();

    // throttling based on memory pressure
    size_t memUsage = getMemoryUsage();
    uint32_t throttleDelay = 300; // Base delay of 300ms

    if (memUsage > 12288) // If using more than 12KB
    {
        throttleDelay = 500; // Slow down to 500ms when memory pressure detected
    }
    if (queueSize > 35) // If queue is very large
    {
        throttleDelay = 400; // Moderate slowdown for large queue
    }

    // Check if enough time has passed since last send
    if (lastWebsocketSendTime != 0 && (currentTime - lastWebsocketSendTime < throttleDelay))
    {
        return; // Not enough time has passed
    }

    // Send the next message in the queue
    QueuedMessage *msg = &messageQueue[queueHead];
    if (msg->message)
    {
        app->websocketSend(msg->message);
        lastWebsocketSendTime = currentTime;

        // Clean up and advance queue
        free(msg->message);
        msg->message = nullptr;
        msg->messageLen = 0;
        queueHead = (queueHead + 1) % MAX_QUEUED_MESSAGES;
        queueSize--;
    }
}

void FlipWorldRun::processWebsocketMessageQueue()
{
    FlipWorldApp *app = static_cast<FlipWorldApp *>(appContext);
    if (app)
    {
        processWebsocketQueue(app);
    }
    else if (queueSize > 0)
    {
        // clear everything if app fails (very very unlikely)
        while (queueSize > 0)
        {
            QueuedMessage *msg = &messageQueue[queueHead];
            if (msg->message)
            {
                free(msg->message);
                msg->message = nullptr;
                msg->messageLen = 0;
            }
            queueHead = (queueHead + 1) % MAX_QUEUED_MESSAGES;
            queueSize--;
        }
    }
}

void FlipWorldRun::pveRender(Entity *entity, Draw *canvas, Game *game)
{
    // Safety check for entity and name
    if (!entity || !entity->name || strlen(entity->name) == 0)
    {
        return;
    }

    // Calculate screen position after applying camera offset
    float screen_x = entity->position.x - game->pos.x;
    float screen_y = entity->position.y - game->pos.y;

    // Check if the username would be visible on the 128x64 screen
    float text_width = strlen(entity->name) * 4 + 1; // Approximate text width
    if (screen_x - text_width / 2 < 0 || screen_x + text_width / 2 > 128 ||
        screen_y - 10 < 0 || screen_y > 64)
    {
        return;
    }

    // draw box around the name
    canvas->fillRect(Vector(screen_x - (strlen(entity->name) * 2) - 1, screen_y - 7), Vector(strlen(entity->name) * 4 + 1, 8), ColorWhite);

    // draw name over player's head
    canvas->text(Vector(screen_x - (strlen(entity->name) * 2), screen_y - 2), entity->name, ColorBlack);
}

bool FlipWorldRun::queueWebsocketMessage(const char *message)
{
    if (!message)
    {
        return false;
    }

    // If queue is getting very full, drop oldest messages to make room
    if (queueSize >= MAX_QUEUED_MESSAGES * 0.85)
    {
        // Drop messages until we're back to 70% to prevent oscillation
        while (queueSize > MAX_QUEUED_MESSAGES * 0.7)
        {
            FURI_LOG_W("FlipWorldRun", "Queue very full (%zu/%zu), dropping oldest message to prevent overflow",
                       queueSize, MAX_QUEUED_MESSAGES);

            // Drop the oldest message
            QueuedMessage *oldMsg = &messageQueue[queueHead];
            if (oldMsg->message)
            {
                free(oldMsg->message);
                oldMsg->message = nullptr;
                oldMsg->messageLen = 0;
            }
            queueHead = (queueHead + 1) % MAX_QUEUED_MESSAGES;
            queueSize--;
        }
    }

    if (queueSize >= MAX_QUEUED_MESSAGES)
    {
        FURI_LOG_W("FlipWorldRun", "Message queue full (%zu/%zu), dropping message to prevent memory leak: %.50s...",
                   queueSize, MAX_QUEUED_MESSAGES, message);
        return false;
    }

    // Check total memory usage before allocating
    size_t currentMemoryUsage = getMemoryUsage();
    size_t messageLen = strlen(message);
    const size_t MAX_TOTAL_MEMORY = 12288;

    if (currentMemoryUsage + messageLen + 1 > MAX_TOTAL_MEMORY)
    {
        FURI_LOG_E("FlipWorldRun", "Would exceed total memory limit (%zu + %zu > %zu), dropping message",
                   currentMemoryUsage, messageLen + 1, MAX_TOTAL_MEMORY);
        return false;
    }

    // Allocate and copy the message
    char *messageCopy = (char *)malloc(messageLen + 1);
    if (!messageCopy)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to allocate memory for queued message");
        return false;
    }
    strcpy(messageCopy, message);

    // Add to queue
    messageQueue[queueTail].message = messageCopy;
    messageQueue[queueTail].messageLen = messageLen;
    queueTail = (queueTail + 1) % MAX_QUEUED_MESSAGES;
    queueSize++;

    // Log warning if queue is getting large
    if (queueSize > MAX_QUEUED_MESSAGES * 0.6 && queueSize % 5 == 0) // Warn at 60% and only every 5th message
    {
        FURI_LOG_W("FlipWorldRun", "Queue getting large (%zu/%zu), potential memory pressure",
                   queueSize, MAX_QUEUED_MESSAGES);
    }

    return true;
}

bool FlipWorldRun::removeRemotePlayer(const char *username)
{
    // Only remove remote players in PvE mode
    if (!isPvEMode || !username)
    {
        return false;
    }

    if (!engine || !engine->getGame() || !engine->getGame()->current_level)
    {
        return false;
    }

    auto currentLevel = engine->getGame()->current_level;

    // Find and remove the player entity
    for (int i = 0; i < currentLevel->getEntityCount(); i++)
    {
        Entity *entity = currentLevel->getEntity(i);
        if (entity && entity->type == ENTITY_PLAYER &&
            entity->name && strcmp(entity->name, username) == 0)
        {
            // Don't remove our own player
            if (entity == player.get())
            {
                continue;
            }

            // Free the allocated username memory before removing the entity
            if (entity->name)
            {
                free((char *)entity->name);
                entity->name = nullptr;
            }

            // Remove the remote player
            currentLevel->entity_remove(entity);
            return true;
        }
    }

    return false;
}

bool FlipWorldRun::safeWebsocketSend(FlipWorldApp *app, const char *message)
{
    if (!app || !message)
    {
        return false;
    }

    // queue it bro
    return queueWebsocketMessage(message);
}

void FlipWorldRun::sendMessageWithChunking(FlipWorldApp *app, const char *message)
{
    if (!app || !message)
    {
        return;
    }

    // if this doesnt work... we definitely need another approach lol
    // i think instead we should just fix FlipperHTTP handle large websocket messages
    // and then we can just send the message as-is without chunking
    // but we made it this far sooo.. let's use it until then

    const size_t MAX_WEBSOCKET_SIZE = 80;
    size_t messageLen = strlen(message);

    // If message fits within limit, send as-is with throttling
    if (messageLen <= MAX_WEBSOCKET_SIZE)
    {
        safeWebsocketSend(app, message);
        return;
    }

    // Generate unique message ID
    static uint16_t messageId = 0;
    messageId++;

    // Calculate metadata overhead dynamically for better accuracy
    char testHeader[64];
    int maxHeaderLen = snprintf(testHeader, sizeof(testHeader), "{\"id\":%d,\"seq\":999,\"total\":999,\"data\":\"", messageId);
    const size_t METADATA_OVERHEAD = maxHeaderLen + 2; // +2 for closing "}
    const size_t MAX_DATA_SIZE = MAX_WEBSOCKET_SIZE - METADATA_OVERHEAD;

    size_t messagePos = 0;

    // First pass: determine actual chunk boundaries that respect JSON structure
    struct ChunkBoundary
    {
        size_t start;
        size_t length;
    };
    ChunkBoundary boundaries[6];
    size_t boundaryCount = 0;

    while (messagePos < messageLen && boundaryCount < 6)
    {
        size_t chunkStart = messagePos;
        size_t maxChunkSize = (messageLen - messagePos > MAX_DATA_SIZE) ? MAX_DATA_SIZE : (messageLen - messagePos);

        // Find a good break point that doesn't split JSON tokens
        size_t actualChunkSize = maxChunkSize;

        // If we're not at the end of the message, try to find a safe break point
        if (messagePos + maxChunkSize < messageLen)
        {
            // Look backwards from the max position to find a safe break point
            size_t searchPos = messagePos + maxChunkSize;
            bool foundBreak = false;

            // Search backwards up to 10 characters for a safe break point
            for (int backtrack = 0; backtrack < 10 && searchPos > messagePos; backtrack++, searchPos--)
            {
                char c = message[searchPos];
                char prevC = (searchPos > 0) ? message[searchPos - 1] : '\0';

                // Good break points: after comma, after colon, after closing brace/bracket
                if ((c == ',' || c == ':' || c == '}' || c == ']') && prevC != '\\')
                {
                    actualChunkSize = searchPos - messagePos + 1;
                    foundBreak = true;
                    break;
                }
                // Also good: before opening quote of a new field
                else if (c == '"' && prevC == ',' && searchPos + 1 < messageLen)
                {
                    actualChunkSize = searchPos - messagePos;
                    foundBreak = true;
                    break;
                }
            }

            // If we couldn't find a good break point, use the maximum size
            if (!foundBreak)
            {
                actualChunkSize = maxChunkSize;
            }
        }

        boundaries[boundaryCount].start = chunkStart;
        boundaries[boundaryCount].length = actualChunkSize;
        boundaryCount++;

        messagePos += actualChunkSize;
    }

    // Second pass: send the chunks (all or nothing approach)
    // First, check if we have enough queue space for all chunks
    if (queueSize + boundaryCount > MAX_QUEUED_MESSAGES)
    {
        FURI_LOG_E("FlipWorldRun", "Not enough queue space for %zu chunks (need %zu, have %zu free), skipping entire message",
                   boundaryCount, boundaryCount, MAX_QUEUED_MESSAGES - queueSize);
        return; // Skip this entire message to avoid partial sends
    }

    // Check if memory pressure would prevent queueing all chunks
    size_t currentMemoryUsage = getMemoryUsage();
    size_t estimatedChunkMemory = boundaryCount * 60;
    const size_t MAX_TOTAL_MEMORY = 12288;

    if (currentMemoryUsage + estimatedChunkMemory > MAX_TOTAL_MEMORY)
    {
        FURI_LOG_E("FlipWorldRun", "Would exceed memory limit with %zu chunks (%zu + %zu > %zu), skipping message",
                   boundaryCount, currentMemoryUsage, estimatedChunkMemory, MAX_TOTAL_MEMORY);
        return; // Skip this entire message to avoid partial sends
    }

    // We have enough space, send all chunks
    for (size_t i = 0; i < boundaryCount; i++)
    {
        size_t chunkStart = boundaries[i].start;
        size_t chunkLength = boundaries[i].length;

        // Build chunk header with actual total count
        char chunkHeader[64];
        int headerLen = snprintf(chunkHeader, sizeof(chunkHeader), "{\"id\":%d,\"seq\":%zu,\"total\":%zu,\"data\":\"",
                                 messageId, i + 1, boundaryCount);

        // Build the complete chunk
        char chunk[MAX_WEBSOCKET_SIZE + 1];
        strcpy(chunk, chunkHeader);
        int pos = headerLen;

        // Add the chunk data
        for (size_t j = 0; j < chunkLength && pos < (int)sizeof(chunk) - 3; j++)
        {
            chunk[pos++] = message[chunkStart + j];
        }

        // Close JSON
        chunk[pos++] = '"';
        chunk[pos++] = '}';
        chunk[pos] = '\0';

        if (!safeWebsocketSend(app, chunk))
        {
            FURI_LOG_E("FlipWorldRun", "Unexpected failure to send/queue chunk %zu", i + 1);
            // Continue anyway since we pre-checked queue space
        }
    }
}

bool FlipWorldRun::setAppContext(void *context)
{
    if (!context)
    {
        FURI_LOG_E("Game", "Context is NULL");
        return false;
    }
    appContext = context;
    return true;
}

bool FlipWorldRun::setIconGroup(LevelIndex index)
{
    const char *json_data = getLevelJson(index);
    if (!json_data)
    {
        FURI_LOG_E("Game", "JSON data is NULL");
        return false;
    }

    // Ensure currentIconGroup is allocated
    if (!currentIconGroup)
    {
        currentIconGroup = (IconGroupContext *)malloc(sizeof(IconGroupContext));
        if (!currentIconGroup)
        {
            FURI_LOG_E("Game", "Failed to allocate currentIconGroup");
            return false;
        }
        currentIconGroup->count = 0;
        currentIconGroup->icons = nullptr;
    }

    // Free any existing icons before reallocating
    if (currentIconGroup->icons)
    {
        free(currentIconGroup->icons);
        currentIconGroup->icons = nullptr;
        currentIconGroup->count = 0;
    }

    // Pass 1: Count the total number of icons.
    int total_icons = 0;
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        char *data = get_json_array_value("json_data", i, json_data);
        if (!data)
            break;
        char *amount = get_json_value("a", data);
        if (amount)
        {
            int count = atoi(amount);
            if (count < 1)
                count = 1;
            total_icons += count;
            free(amount);
        }
        free(data);
    }

    currentIconGroup->count = total_icons;
    currentIconGroup->icons = (IconSpec *)malloc(total_icons * sizeof(IconSpec));
    if (!currentIconGroup->icons)
    {
        FURI_LOG_E("Game", "Failed to allocate icon group array for %d icons", total_icons);
        return false;
    }

    // Pass 2: Parse the JSON to fill the icon specs.
    int spec_index = 0;
    for (int i = 0; i < MAX_WORLD_OBJECTS; i++)
    {
        char *data = get_json_array_value("json_data", i, json_data);
        if (!data)
            break;

        /*
        i - icon name
        x - x position
        y - y position
        a - amount
        h - horizontal (true/false)
        */

        char *icon_str = get_json_value("i", data);
        char *x_str = get_json_value("x", data);
        char *y_str = get_json_value("y", data);
        char *amount_str = get_json_value("a", data);
        char *horizontal_str = get_json_value("h", data);

        if (!icon_str || !x_str || !y_str || !amount_str || !horizontal_str)
        {
            FURI_LOG_E("Game", "Incomplete icon data");
            if (icon_str)
                free(icon_str);
            if (x_str)
                free(x_str);
            if (y_str)
                free(y_str);
            if (amount_str)
                free(amount_str);
            if (horizontal_str)
                free(horizontal_str);
            free(data);
            continue;
        }

        int count = atoi(amount_str);
        if (count < 1)
            count = 1;
        float base_x = atof_(x_str);
        float base_y = atof_(y_str);
        bool is_horizontal = (strcmp(horizontal_str, "true") == 0);
        int spacing = 17;

        for (int j = 0; j < count; j++)
        {
            IconSpec spec = getIconSpec(icon_str);
            if (!spec.icon)
            {
                FURI_LOG_E("Game", "Icon name not recognized");
                continue;
            }
            if (is_horizontal)
            {
                spec.pos.x = base_x + (j * spacing);
                spec.pos.y = base_y;
            }
            else
            {
                spec.pos.x = base_x;
                spec.pos.y = base_y + (j * spacing);
            }
            currentIconGroup->icons[spec_index++] = spec;
        }

        free(icon_str);
        free(x_str);
        free(y_str);
        free(amount_str);
        free(horizontal_str);
        free(data);
    }

    return true;
}

bool FlipWorldRun::shouldProcessEnemyAI() const
{
    // In PvE mode, only the host processes enemy AI
    if (isPvEMode)
    {
        return isLobbyHost;
    }
    return true;
}

bool FlipWorldRun::shouldUpdateEntity(Entity *entity) const
{
    if (!entity)
    {
        return false;
    }

    // In PvE mode, only the host processes enemy AI
    if (isPvEMode && entity->type == ENTITY_ENEMY)
    {
        return isLobbyHost;
    }

    return true;
}

bool FlipWorldRun::startGame()
{
    draw->fillScreen(ColorWhite);
    draw->text(Vector(0, 10), "Initializing game...", ColorBlack);

    if (isGameRunning || engine)
    {
        FURI_LOG_W("FlipWorldRun", "Game already running, skipping start");
        return true;
    }

    // Create the player instance if it doesn't exist
    if (!player)
    {
        player = std::make_unique<Player>();
        if (!player)
        {
            FURI_LOG_E("FlipWorldRun", "Failed to create Player object");
            return false;
        }
    }

    // Create the game instance with 3rd person perspective
    auto game = std::make_unique<Game>("FlipWorld", Vector(768, 384), draw.get());
    if (!game)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to create Game object");
        return false;
    }

    draw->fillScreen(ColorWhite);
    draw->text(Vector(0, 10), "Adding levels and player...", ColorBlack);

    // add levels and player to the game
    std::unique_ptr<Level> level1 = getLevel(LevelHomeWoods, game.get());
    std::unique_ptr<Level> level2 = getLevel(LevelRockWorld, game.get());
    std::unique_ptr<Level> level3 = getLevel(LevelForestWorld, game.get());

    setIconGroup(LevelHomeWoods); // once we switch levels, we need to set the icon group again

    level1->entity_add(player.get());
    level2->entity_add(player.get());
    level3->entity_add(player.get());

    // Add all levels to the game engine
    game->level_add(level1.release());
    game->level_add(level2.release());
    game->level_add(level3.release());

    // Start with the first level
    game->level_switch(0); // Switch to LevelHomeWoods (index 0)

    // set game position to center of player
    game->pos = Vector(384, 192);
    game->old_pos = game->pos;

    this->engine = std::make_unique<GameEngine>(game.release(), 60);
    if (!this->engine)
    {
        FURI_LOG_E("FlipWorldRun", "Failed to create GameEngine");
        return false;
    }

    draw->fillScreen(ColorWhite);
    draw->setFontCustom(FONT_SIZE_SMALL);
    if (isPvEMode)
    {
        draw->text(Vector(0, 10), "Starting multiplayer game...", ColorBlack);
    }
    else
    {
        draw->text(Vector(0, 10), "Starting single player game...", ColorBlack);
    }

    isGameRunning = true; // Set the flag to indicate game is running
    return true;
}

void FlipWorldRun::syncMultiplayerState()
{
    // Only sync in PvE mode
    if (!isPvEMode)
    {
        return;
    }

    FlipWorldApp *app = static_cast<FlipWorldApp *>(appContext);
    if (!app)
    {
        return;
    }

    // Check if enough time has passed since last sync
    uint32_t currentTime = furi_get_tick();

    // If queue is getting full, increase sync interval to prevent overflow
    uint32_t effectiveSyncInterval = syncInterval;
    if (queueSize > MAX_QUEUED_MESSAGES * 0.7) // If queue is 70% full
    {
        effectiveSyncInterval = syncInterval * 2; // Double the interval
    }

    if (currentTime - lastSyncTime < effectiveSyncInterval)
    {
        return;
    }
    lastSyncTime = currentTime;

    if (!engine || !engine->getGame() || !engine->getGame()->current_level)
    {
        return;
    }

    auto currentLevel = engine->getGame()->current_level;

    if (isLobbyHost)
    {
        // Host sends all entity states (player + enemies)
        // Limit number of entities per sync if queue is getting full
        int maxEntitiesToSync = currentLevel->getEntityCount();
        if (queueSize > MAX_QUEUED_MESSAGES * 0.5) // If queue is 50% full
        {
            maxEntitiesToSync = 2; // Only sync 2 entities at a time
        }

        int entitiesSynced = 0;
        for (int i = 0; i < currentLevel->getEntityCount() && entitiesSynced < maxEntitiesToSync; i++)
        {
            Entity *entity = currentLevel->getEntity(i);
            if (!entity)
                continue;

            const char *entityJson = entityToJson(entity, true); // websocket format
            if (entityJson)
            {
                // Calculate required buffer size and allocate dynamically
                size_t entityJsonLen = strlen(entityJson);
                size_t messageSize = entityJsonLen + 64; // Extra space for wrapper JSON
                char *message = (char *)malloc(messageSize);

                if (!message)
                {
                    FURI_LOG_E("FlipWorldRun", "Failed to allocate message buffer");
                    free((void *)entityJson);
                    continue;
                }

                // Create message with type and data
                if (entity->type == ENTITY_PLAYER)
                {
                    snprintf(message, messageSize, "{\"type\":\"player\",\"data\":%s}", entityJson);
                }
                else if (entity->type == ENTITY_ENEMY)
                {
                    snprintf(message, messageSize, "{\"type\":\"enemy\",\"data\":%s}", entityJson);
                }
                else
                {
                    free((void *)entityJson); // Free allocated memory
                    free(message);
                    continue; // Skip NPCs and other entity types
                }

                // Send message with chunking support
                sendMessageWithChunking(app, message);
                entitiesSynced++;

                free((void *)entityJson); // Free allocated memory after use
                free(message);            // Free message buffer
            }
        }

        // Send current level info
        char levelMessage[128];
        snprintf(levelMessage, sizeof(levelMessage),
                 "{\"type\":\"level\",\"level_index\":%d}",
                 getCurrentLevelIndex());
        safeWebsocketSend(app, levelMessage);
    }
    else
    {
        // Follower sends only their player state
        if (player)
        {
            // Find the player entity in the current level
            Entity *playerEntity = nullptr;
            for (int i = 0; i < currentLevel->getEntityCount(); i++)
            {
                Entity *entity = currentLevel->getEntity(i);
                if (entity && entity->type == ENTITY_PLAYER)
                {
                    playerEntity = entity;
                    break;
                }
            }

            if (playerEntity)
            {
                const char *entityJson = entityToJson(playerEntity, true); // websocket format
                if (entityJson)
                {
                    // Calculate required buffer size and allocate dynamically
                    size_t entityJsonLen = strlen(entityJson);
                    size_t messageSize = entityJsonLen + 64; // Extra space for wrapper JSON
                    char *message = (char *)malloc(messageSize);

                    if (!message)
                    {
                        FURI_LOG_E("FlipWorldRun", "Failed to allocate message buffer");
                        free((void *)entityJson);
                        return;
                    }

                    snprintf(message, messageSize, "{\"type\":\"player\",\"data\":%s}", entityJson);

                    // Send message with chunking support
                    sendMessageWithChunking(app, message);

                    free((void *)entityJson); // Free allocated memory after use
                    free(message);            // Free message buffer
                }
            }
        }
    }
}

void FlipWorldRun::updateDraw(Canvas *canvas)
{
    // set Draw instance
    if (!draw)
    {
        draw = std::make_unique<Draw>(canvas);
    }

    // Initialize player if not already done
    if (!player)
    {
        player = std::make_unique<Player>();
        if (player)
        {
            player->setFlipWorldRun(this);
        }
    }

    // Process multiplayer updates if in PvE mode
    if (isPvEMode && isGameRunning)
    {
        processMultiplayerUpdate();
    }

    // Let the player handle all drawing
    if (player)
    {
        if (player->shouldLeaveGame())
        {
            this->endGame(); // End the game if the player wants to leave
        }
        else
        {
            player->drawCurrentView(draw.get());
        }
    }
}

void FlipWorldRun::updateInput(InputEvent *event)
{
    if (!event)
    {
        FURI_LOG_E("FlipWorldRun", "updateInput: no event available");
        return;
    }

    this->lastInput = event->key;

    // Only run inputManager when not in an active game to avoid input conflicts
    if (!(player && this->isGameRunning))
    {
        this->inputManager();
    }
}
