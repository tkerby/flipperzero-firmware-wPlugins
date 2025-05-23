/*
 * Copyright 2025 Ivan Barsukov
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "delayed_sprite.h"

#include "src/engine/game_manager.h"

#include "src/game.h"

Entity* delayed_sprite_add_to_level(
    Level* level,
    GameManager* manager,
    Vector pos,
    float delay,
    const char* sprite_name) {
    Entity* entity = level_add_entity(level, &delayed_sprite_description);
    DelayedSpriteContext* entity_context = entity_context_get(entity);
    entity_pos_set(entity, pos);
    entity_context->delay = delay;
    entity_context->time = 0;
    entity_context->sprite = game_manager_sprite_load(manager, sprite_name);
    return entity;
}

static void delayed_sprite_update(Entity* self, GameManager* manager, void* _entity_context) {
    UNUSED(self);
    UNUSED(manager);
    DelayedSpriteContext* entity_context = _entity_context;

    if(entity_context->time < entity_context->delay) {
        entity_context->time += 1.0f;
    }
}

static void delayed_sprite_render(
    Entity* self,
    GameManager* manager,
    Canvas* canvas,
    void* _entity_context) {
    UNUSED(manager);
    DelayedSpriteContext* entity_context = _entity_context;

    if(entity_context->time >= entity_context->delay) {
        Vector pos = entity_pos_get(self);
        canvas_draw_sprite(canvas, entity_context->sprite, pos.x, pos.y);
    }
}

static void delayed_sprite_event(
    Entity* self,
    GameManager* manager,
    EntityEvent event,
    void* _entity_context) {
    UNUSED(self);
    UNUSED(manager);

    DelayedSpriteContext* entity_context = _entity_context;
    if(event.type == GameEventSkipAnimation) {
        entity_context->time = entity_context->delay;
    }
}

const EntityDescription delayed_sprite_description = {
    .start = NULL,
    .stop = NULL,
    .update = delayed_sprite_update,
    .render = delayed_sprite_render,
    .collision = NULL,
    .event = delayed_sprite_event,
    .context_size = sizeof(DelayedSpriteContext),
};
