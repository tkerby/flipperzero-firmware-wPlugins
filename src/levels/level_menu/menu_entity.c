#include "menu_entity.h"

#include "src/engine/entity.h"
#include "src/engine/game_manager.h"
#include "src/engine/level.h"

#include "src/game.h"

static void
menu_update(Entity* self, GameManager* manager, void* _entity_context)
{
    UNUSED(self);
    UNUSED(_entity_context);

    GameContext* game_context = game_manager_game_context_get(manager);
    Level* level = game_manager_current_level_get(manager);

    InputState input = game_manager_input_get(manager);
    if (input.pressed & GameKeyBack) {
        game_manager_game_stop(manager);
    } else if (input.pressed & GameKeyOk) {
        game_manager_next_level_set(manager, game_context->levels.game);
        level_send_event(
          level, self, NULL, GameEventSkipAnimation, (EntityEventValue){ 0 });
    } else if (input.pressed & GameKeyLeft) {
        game_manager_next_level_set(manager, game_context->levels.settings);
        level_send_event(
          level, self, NULL, GameEventSkipAnimation, (EntityEventValue){ 0 });
    } else if (input.pressed & GameKeyRight) {
        game_manager_next_level_set(manager, game_context->levels.about);
        level_send_event(
          level, self, NULL, GameEventSkipAnimation, (EntityEventValue){ 0 });
    }
}

const EntityDescription menu_description = {
    .start = NULL,
    .stop = NULL,
    .update = menu_update,
    .render = NULL,
    .collision = NULL,
    .event = NULL,
    .context_size = 0,
};
