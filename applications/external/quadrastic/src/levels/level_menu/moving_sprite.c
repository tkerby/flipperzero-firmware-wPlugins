#include "moving_sprite.h"

#include "../../engine/entity.h"
#include "../../engine/game_manager.h"

#include "../../game.h"

void
moving_sprite_init(Entity* entity,
                   GameManager* manager,
                   Vector pos_start,
                   Vector pos_end,
                   float duration,
                   const char* sprite_name)
{
    MovingSpriteContext* entity_context = entity_context_get(entity);
    entity_context->pos_start = pos_start;
    entity_context->pos_end = pos_end;
    entity_context->duration = duration;
    entity_context->time = 0;
    entity_context->sprite = game_manager_sprite_load(manager, sprite_name);
}

static void
moving_sprite_update(Entity* self, GameManager* manager, void* _entity_context)
{
    UNUSED(manager);
    MovingSpriteContext* entity_context = _entity_context;

    // lerp position between start and end for duration
    if (entity_context->time < entity_context->duration) {
        Vector dir =
          vector_sub(entity_context->pos_end, entity_context->pos_start);
        Vector len =
          vector_mulf(dir, entity_context->time / entity_context->duration);
        Vector pos = vector_add(entity_context->pos_start, len);

        entity_pos_set(self, pos);
        entity_context->time += 1.0f;
    } else {
        entity_pos_set(self, entity_context->pos_end);
    }
}

static void
moving_sprite_render(Entity* self,
                     GameManager* manager,
                     Canvas* canvas,
                     void* _entity_context)
{
    UNUSED(manager);
    MovingSpriteContext* entity_context = _entity_context;

    if (entity_context->sprite) {
        Vector pos = entity_pos_get(self);
        canvas_draw_sprite(canvas, entity_context->sprite, pos.x, pos.y);
    }
}

static void
moving_sprite_event(Entity* self,
                    GameManager* manager,
                    EntityEvent event,
                    void* _entity_context)
{
    UNUSED(self);
    UNUSED(manager);
    UNUSED(event);

    MovingSpriteContext* entity_context = _entity_context;
    if (event.type == GameEventStopAnimation) {
        entity_context->time = entity_context->duration;
    }
}

const EntityDescription moving_sprite_description = {
    .start = NULL,
    .stop = NULL,
    .update = moving_sprite_update,
    .render = moving_sprite_render,
    .collision = NULL,
    .event = moving_sprite_event,
    .context_size = sizeof(MovingSpriteContext),
};
