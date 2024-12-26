#include <game/level.h>
#include <flip_storage/storage.h>
static void level_start(Level *level, GameManager *manager, void *context)
{
    UNUSED(manager);
    if (!level || !context)
    {
        FURI_LOG_E("Game", "Level or context is NULL");
        return;
    }

    level_clear(level);
    player_spawn(level, manager);
    LevelContext *level_context = context;

    // check if the world exists
    if (!world_exists(level_context->id))
    {
        FURI_LOG_E("Game", "World does not exist.. downloading now");
        FuriString *world_data = fetch_world(level_context->id);
        if (!world_data)
        {
            FURI_LOG_E("Game", "Failed to fetch world data");
            draw_tree_world(level);
            return;
        }

        if (!draw_json_world(level, furi_string_get_cstr(world_data)))
        {
            FURI_LOG_E("Game", "Failed to draw world");
            draw_tree_world(level);
        }

        // world_data is guaranteed non-NULL here
        furi_string_free(world_data);
        return;
    }

    // get the world data
    FuriString *world_data = load_furi_world(level_context->id);
    if (!world_data)
    {
        FURI_LOG_E("Game", "Failed to load world data");
        draw_tree_world(level);
        return;
    }

    // draw the world
    if (!draw_json_world(level, furi_string_get_cstr(world_data)))
    {
        FURI_LOG_E("Game", "World exists but failed to draw.");
        draw_tree_world(level);
    }

    // world_data is guaranteed non-NULL here
    furi_string_free(world_data);
}

static LevelContext *level_context_generic;

static LevelContext *level_generic_alloc(const char *id, int index)
{
    if (level_context_generic == NULL)
    {
        level_context_generic = malloc(sizeof(LevelContext));
    }
    snprintf(level_context_generic->id, sizeof(level_context_generic->id), "%s", id);
    level_context_generic->index = index;
    return level_context_generic;
}

static void level_generic_free()
{
    if (level_context_generic != NULL)
    {
        free(level_context_generic);
        level_context_generic = NULL;
    }
}

static void level_free(Level *level, GameManager *manager, void *context)
{
    UNUSED(level);
    UNUSED(manager);
    UNUSED(context);
    level_generic_free();
}

static void level_alloc_generic_world(Level *level, GameManager *manager, void *context)
{
    UNUSED(manager);
    UNUSED(level);
    if (!level_context_generic)
    {
        FURI_LOG_E("Game", "Generic level context not set");
        return;
    }
    if (!context)
    {
        FURI_LOG_E("Game", "Context is NULL");
        return;
    }
    LevelContext *level_context = context;
    snprintf(level_context->id, sizeof(level_context->id), "%s", level_context_generic->id);
    level_context->index = level_context_generic->index;
}

const LevelBehaviour _generic_level = {
    .alloc = level_alloc_generic_world,
    .free = level_free,
    .start = level_start,
    .stop = NULL,
    .context_size = sizeof(LevelContext),
};

const LevelBehaviour *generic_level(const char *id, int index)
{
    // free any old context before allocating a new one
    level_generic_free();
    level_context_generic = level_generic_alloc(id, index);
    return &_generic_level;
}
