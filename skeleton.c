#include "skeleton.h"
#include "player.h"

void skeleton_spawn(Level *level, GameManager *manager){
    Entity* skeleton = level_add_entity(level, &skel_desc);
    // TEMP//
    entity_pos_set(skeleton, (Vector){30, 30});

    entity_collider_add_rect(skeleton, 15, 31);

    SkeletonContext* skeleton_context = entity_context_get(skeleton);

    skeleton_context->sprite = game_manager_sprite_load(manager, "enemies/skeleton/walking_0.fxbm");
}

void skel_update(Entity* self, GameManager* manager, void* context) {
    Vector pos = entity_pos_get(self);
    Vector player_pos = entity_pos_get(player);
    
    if(player != NULL){
        if(player_pos.x > pos.x) pos.x += 0.5;
        if(player_pos.x < pos.x) pos.x -= 0.5;
    }

    entity_pos_set(self, pos);
    UNUSED(manager);
    UNUSED(context);
}

void skel_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    SkeletonContext* skel = context;
    Vector pos = entity_pos_get(self);

    canvas_draw_sprite(canvas, skel->sprite, pos.x - 10, pos.y);

    UNUSED(manager);
}

const EntityDescription skel_desc = {
    .start = NULL,
    .stop = NULL,
    .update = skel_update,
    .render = skel_render,
    .collision = NULL,
    .event = NULL,
    .context_size =
        sizeof(SkeletonContext),
};