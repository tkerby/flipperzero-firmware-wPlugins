#include "level_about.h"

#include <stddef.h>

#include "../../game.h"

#include "about_entity.h"

static void level_about_alloc(Level* level, GameManager* manager, void* context) {
    UNUSED(manager);
    UNUSED(context);

    Entity* entity = level_add_entity(level, &about_description);
    AboutContext* entity_context = entity_context_get(entity);
    entity_context->logo_sprite = game_manager_sprite_load(manager, "icon.fxbm");

    FURI_LOG_D(GAME_NAME, "About level allocated");
}

static void level_about_start(Level* level, GameManager* manager, void* context) {
    UNUSED(level);
    UNUSED(manager);
    UNUSED(context);

    FURI_LOG_D(GAME_NAME, "About level started");
}

const LevelBehaviour level_about = {
    .alloc = level_about_alloc,
    .free = NULL,
    .start = level_about_start,
    .stop = NULL,
    .context_size = 0,
};
