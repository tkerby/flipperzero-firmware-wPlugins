#include <game/level.h>

/****** Level ******/

static void level_start(Level *level, GameManager *manager, void *context)
{
    UNUSED(manager);
    LevelContext *level_context = context;
    // check if the world exists
    // if (!world_exists(level_context->id))
    // {
    //     FURI_LOG_E("Game", "World does not exist");
    //     easy_flipper_dialog("[WORLD ERROR]", "No world data installed.\n\n\nSettings -> Game ->\nInstall Official World Pack");
    //     draw_example_world(level);
    //     return;
    // }
    // draw the world
    // FuriString *world_data = load_furi_world(level_context->id);
    // if (!world_data)
    // {
    //     FURI_LOG_E("Game", "Failed to load world data");
    //     draw_example_world(level);
    //     return;
    // }
    // if (!draw_json_world_furi(level, load_furi_world(level_context->id)))
    // {
    //     FURI_LOG_E("Game", "World exists but failed to draw.");
    //     draw_example_world(level);
    // }
    // furi_string_free(world_data);
    if (level_context->index == 0)
        draw_tree_world(level);
    else
        draw_example_world(level);
}

static void level_alloc_tree_world(Level *level, GameManager *manager, void *context)
{
    UNUSED(level);
    LevelContext *level_context = context;
    snprintf(level_context->id, sizeof(level_context->id), "tree_world");
    level_context->index = 0;
    // Add player entity to the level
    player_spawn(level, manager);
}
static void level_alloc_example_world(Level *level, GameManager *manager, void *context)
{
    UNUSED(level);
    LevelContext *level_context = context;
    snprintf(level_context->id, sizeof(level_context->id), "example_world");
    level_context->index = 1;
    // Add player entity to the level
    player_spawn(level, manager);
}

Level *level_tree;
Level *level_example;

const LevelBehaviour tree_level = {
    .alloc = level_alloc_tree_world,      // called once, when level allocated
    .free = NULL,                         // called once, when level freed
    .start = level_start,                 // called when level is changed to this level
    .stop = NULL,                         // called when level is changed from this level
    .context_size = sizeof(LevelContext), // size of level context, will be automatically allocated and freed
};
const LevelBehaviour example_level = {
    .alloc = level_alloc_example_world,
    .free = NULL,
    .start = level_start,
    .stop = NULL,
    .context_size = sizeof(LevelContext),
};

void level_alloc_world(Level *level, GameManager *manager, void *context)
{
    UNUSED(level);
    LevelContext *level_context = context;
    snprintf(level_context->id, sizeof(level_context->id), "%s", level_contexts[level_context->index].id);
    level_context->index = level_contexts[level_context->index].index;
    // Add player entity to the level
    player_spawn(level, manager);
}

bool level_load_all(GameManager *game_manager)
{
    char file_path[128];
    snprintf(
        file_path,
        sizeof(file_path),
        STORAGE_EXT_PATH_PREFIX "/apps_data/flip_world/worlds/world_list.json");

    FuriString *world_list = flipper_http_load_from_file(file_path);
    if (!world_list)
    {
        FURI_LOG_E(TAG, "Failed to load world list");
        return false;
    }
    for (int i = 0; i < 10; i++)
    {
        FuriString *world_id = get_json_array_value_furi("names", i, world_list);
        if (!world_id)
        {
            FURI_LOG_E(TAG, "Failed to get world id");
            break;
        }
        snprintf(level_contexts[i].id, sizeof(level_contexts[i].id), "%s", furi_string_get_cstr(world_id));
        level_contexts[i].index = i;
        LevelBehaviour *new_behavior = malloc(sizeof(LevelBehaviour));
        if (!new_behavior)
        {
            FURI_LOG_E(TAG, "Failed to allocate memory for level behavior");
            furi_string_free(world_id);
            furi_string_free(world_list);
            return false;
        }
        *new_behavior = (LevelBehaviour){
            .alloc = level_alloc_world,
            .free = NULL,
            .start = level_start,
            .stop = NULL,
            .context_size = sizeof(LevelContext),
        };
        level_count++;
        game_manager_add_level(game_manager, new_behavior);
        furi_string_free(world_id);
    }
    furi_string_free(world_list);
    return true;
}

LevelContext level_contexts[] = {0};
Level *levels[] = {0};
int level_count = 0;