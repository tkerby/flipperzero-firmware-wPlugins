#include "blinking_sprite.h"

#include "../../engine/game_manager.h"

#include "../../game.h"

void
blinking_sprite_init(Entity* entity,
                     GameManager* manager,
                     Vector pos,
                     float delay,
                     float show_duration,
                     float hide_duration,
                     const char* sprite_name)
{
    BlinkingSpriteContext* entity_context = entity_context_get(entity);
    entity_pos_set(entity, pos);
    entity_context->delay = delay;
    entity_context->show_duration = show_duration;
    entity_context->hide_duration = hide_duration;
    entity_context->time = 0;
    entity_context->sprite = game_manager_sprite_load(manager, sprite_name);
}

static void
blinking_sprite_update(Entity* self,
                       GameManager* manager,
                       void* _entity_context)
{
    UNUSED(self);
    UNUSED(manager);

    BlinkingSpriteContext* entity_context = _entity_context;

    entity_context->time += 1.0f;

    if (entity_context->delay + entity_context->show_duration +
          entity_context->hide_duration <
        entity_context->time) {
        entity_context->time = entity_context->delay;
    }
}

static void
blinking_sprite_render(Entity* self,
                       GameManager* manager,
                       Canvas* canvas,
                       void* _entity_context)
{
    UNUSED(manager);
    BlinkingSpriteContext* entity_context = _entity_context;

    if (entity_context->sprite &&
        entity_context->delay <= entity_context->time &&
        entity_context->time <
          entity_context->delay + entity_context->show_duration) {
        Vector pos = entity_pos_get(self);
        canvas_draw_sprite(canvas, entity_context->sprite, pos.x, pos.y);
    }
}

static void
blinking_sprite_event(Entity* self,
                      GameManager* manager,
                      EntityEvent event,
                      void* _entity_context)
{
    UNUSED(self);
    UNUSED(manager);

    BlinkingSpriteContext* entity_context = _entity_context;
    if (event.type == GameEventStopAnimation) {
        entity_context->time = entity_context->delay;
    }
}

const EntityDescription blinking_sprite_description = {
    .start = NULL,
    .stop = NULL,
    .update = blinking_sprite_update,
    .render = blinking_sprite_render,
    .collision = NULL,
    .event = blinking_sprite_event,
    .context_size = sizeof(BlinkingSpriteContext),
};
