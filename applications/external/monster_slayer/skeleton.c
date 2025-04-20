#include "skeleton.h"
#include "player.h"

bool isSkeletonGrounded;
int health = 3;
int previous_health;
bool is_dead;
bool is_hurt;

int hurt_frames_to_wait = 30;
int hurt_timer = 0;

void skeleton_spawn(Level* level, GameManager* manager) {
    Entity* skeleton = level_add_entity(level, &skel_desc);
    entity_pos_set(skeleton, (Vector){30, 30});
    entity_collider_add_rect(skeleton, 15, 31);

    SkeletonContext* skeleton_context = entity_context_get(skeleton);
    skeleton_context->sprite =
        game_manager_sprite_load(manager, "enemies/skeleton/walking_0.fxbm");

    previous_health = health;
}

void skel_update(Entity* self, GameManager* manager, void* context) {
    Vector pos = entity_pos_get(self);
    SkeletonContext* skeleton_context = entity_context_get(self);
    GameContext* game_context = game_manager_game_context_get(manager);
    Level* level = game_manager_current_level_get(manager);

    if((pos.y + 31) >= game_context->ground_hight && skeleton_context->Yvelocity >= 0) {
        pos.y = game_context->ground_hight - 31;
        skeleton_context->Yvelocity = 0;
        isSkeletonGrounded = true;
    } else {
        isSkeletonGrounded = false;
    }

    if(!isSkeletonGrounded) {
        skeleton_context->Yvelocity += 0.5;
        pos.y += skeleton_context->Yvelocity;
    }

    if(pos.y < 5) pos.y = 5;
    if(pos.y > 59) pos.y = 59;

    if(is_hurt) {
        if(hurt_timer > 0) {
            hurt_timer--;
        } else {
            is_hurt = false;
        }
    }

    if(!is_hurt && player != NULL) {
        Vector player_pos = entity_pos_get(player);
        PlayerContext* player_context = entity_context_get(player);

        float distance_to_player = pos.x - player_pos.x;

        if(player_context->is_hitting) {
            if(is_player_facing_right && distance_to_player > 0 &&
               distance_to_player <= player_sword_reach) {
                health -= player_context->weapon_damage;
            }
            if(!is_player_facing_right && distance_to_player < 0 &&
               -distance_to_player <= player_sword_reach) {
                health -= player_context->weapon_damage;
            }

            if(health < previous_health) {
                is_hurt = true;
                hurt_timer = hurt_frames_to_wait;
                previous_health = health;
            }
        }
    }

    if(!is_hurt && player != NULL) {
        Vector player_pos = entity_pos_get(player);
        if(player_pos.x > pos.x) pos.x += 0.5;
        if(player_pos.x < pos.x) pos.x -= 0.5;
    }

    entity_pos_set(self, pos);

    if(health <= 0) {
        level_remove_entity(level, self);
    }

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
    .context_size = sizeof(SkeletonContext),
};
