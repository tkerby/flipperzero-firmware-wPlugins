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
