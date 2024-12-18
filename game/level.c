#include <game/level.h>

static void level_start(Level *level, GameManager *manager, void *context)
{
    UNUSED(manager);
    LevelContext *level_context = context;
    // check if the world exists
    if (!world_exists(level_context->id))
    {
        FURI_LOG_E("Game", "World does not exist.. downloading now");
        if (!level_context->app)
        {
            level_context->app = (FlipWorldApp *)malloc(sizeof(FlipWorldApp));
            if (!level_context->app)
            {
                FURI_LOG_E("Game", "Failed to allocate FlipWorldApp");
                return;
            }
        }
        FuriString *world_data = fetch_world(level_context->id, level_context->app);
        if (!world_data)
        {
            FURI_LOG_E("Game", "Failed to fetch world data");
            draw_town_world(level);
            free(level_context->app);
            level_context->app = NULL;
            return;
        }
        if (!draw_json_world(level, furi_string_get_cstr(world_data)))
        {
            FURI_LOG_E("Game", "Failed to draw world");
            draw_town_world(level);
        }
        free(level_context->app);
        level_context->app = NULL;
        furi_string_free(world_data);
        return;
    }
    // get the world data
    FuriString *world_data = load_furi_world(level_context->id);
    if (!world_data)
    {
        FURI_LOG_E("Game", "Failed to load world data");
        draw_town_world(level);
        return;
    }
    // draw the world
    if (!draw_json_world(level, furi_string_get_cstr(world_data)))
    {
        FURI_LOG_E("Game", "World exists but failed to draw.");
        draw_town_world(level);
    }
    furi_string_free(world_data);
}

static LevelContext *level_context_generic;
static LevelContext *level_generic_alloc(const char *id, int index)
{
    if (!level_context_generic)
    {
        level_context_generic = malloc(sizeof(LevelContext));
    }
    snprintf(level_context_generic->id, sizeof(level_context_generic->id), "%s", id);
    level_context_generic->index = index;
    level_context_generic->app = NULL;
    return level_context_generic;
}
static void level_generic_free()
{
    if (level_context_generic)
    {
        free(level_context_generic);
        level_context_generic = NULL;
    }
}

static void level_alloc_generic_world(Level *level, GameManager *manager, void *context)
{
    if (!level_context_generic)
    {
        FURI_LOG_E("Game", "Generic level context not set");
        return;
    }
    LevelContext *level_context = context;
    snprintf(level_context->id, sizeof(level_context->id), "%s", level_context_generic->id);
    level_context->index = level_context_generic->index;
    level_context->app = level_context_generic->app;
    player_spawn(level, manager);
}

// Do NOT touch (this is for dynamic level creation)
const LevelBehaviour _generic_level = {
    .alloc = level_alloc_generic_world,   // called once, when level allocated
    .free = NULL,                         // called once, when level freed
    .start = level_start,                 // called when level is changed to this level
    .stop = NULL,                         // called when level is changed from this level
    .context_size = sizeof(LevelContext), // size of level context, will be automatically allocated and freed
};

const LevelBehaviour *generic_level(const char *id, int index)
{
    level_generic_free();
    level_context_generic = level_generic_alloc(id, index);
    return &_generic_level;
}