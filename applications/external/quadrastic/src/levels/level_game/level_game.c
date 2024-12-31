#include "level_game.h"

#include <stddef.h>

#include <m-list.h>

#include <dolphin/dolphin.h>
#include <notification/notification_messages.h>

#include "../../engine/vector.h"

#include "../../game.h"
#include "../../game_notifications.h"

#include "context_menu.h"

#include "enemy_entity.h"
#include "player_entity.h"

#define TARGET_ANIMATION_DURATION 30.0f
#define TARGET_SIZE               1
#define TARGET_RADIUS             1
#define TARGET_ANIMATION_RADIUS   10

static Vector random_pos(void) {
    return (Vector){rand() % 120 + 4, rand() % 58 + 4};
}

/****** Entities: Target ******/

typedef struct {
    Sprite* sprite;
    float time;
} TargetContext;

static const EntityDescription target_description;

static Entity* create_target(Level* level, GameManager* manager) {
    Entity* target = level_add_entity(level, &target_description);

    // Set target position
    entity_pos_set(target, random_pos());

    // Add collision circle to target entity
    entity_collider_add_circle(target, TARGET_RADIUS);

    // Load target sprite
    TargetContext* target_context = entity_context_get(target);
    target_context->sprite = game_manager_sprite_load(manager, "target.fxbm");

    return target;
}

static void reset_target(Entity* target) {
    // Set player position.
    entity_pos_set(target, random_pos());

    // Reset animation
    TargetContext* target_context = entity_context_get(target);
    target_context->time = 0.0f;
}

static void target_update(Entity* self, GameManager* manager, void* context) {
    UNUSED(self);
    UNUSED(manager);

    // Start level animation
    TargetContext* target = context;
    if(target->time >= TARGET_ANIMATION_DURATION) {
        return;
    }
    target->time += 1.0f;
}

static void target_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(context);
    UNUSED(manager);

    // Get target position
    Vector pos = entity_pos_get(self);

    TargetContext* target = context;
    if(target->time < TARGET_ANIMATION_DURATION) {
        float step = TARGET_ANIMATION_RADIUS / TARGET_ANIMATION_DURATION;
        float radius = CLAMP(
            TARGET_ANIMATION_RADIUS - target->time * step, TARGET_ANIMATION_RADIUS, TARGET_RADIUS);
        canvas_draw_circle(canvas, pos.x, pos.y, radius);
        canvas_draw_dot(canvas, pos.x, pos.y);
        return;
    }

    // Draw target
    TargetContext* target_context = entity_context_get(self);
    canvas_draw_sprite(canvas, target_context->sprite, pos.x - 1, pos.y - 1);
}

static void target_collision(Entity* self, Entity* other, GameManager* manager, void* context) {
    UNUSED(context);

    if(entity_description_get(other) != &player_description) {
        return;
    }

    // Increase score
    GameContext* game_context = game_manager_game_context_get(manager);
    ++game_context->score;

    // Move target to new random position
    reset_target(self);
    spawn_enemy(manager);

    // Notify
    game_notify(game_context, &sequence_earn_point);
}

static const EntityDescription target_description = {
    .start = NULL,
    .stop = NULL,
    .update = target_update,
    .render = target_render,
    .collision = target_collision,
    .event = NULL,
    .context_size = sizeof(TargetContext),
};

/***** Level *****/

static void level_game_alloc(Level* level, GameManager* manager, void* context) {
    GameLevelContext* level_context = context;

    // Add entities to the level
    level_context->player = player_spawn(level, manager);
    level_context->target = create_target(level, manager);
    EntityList_init(level_context->enemies);
    level_context->is_paused = false;
    level_context->pause_menu = NULL;
}

static void level_game_start(Level* level, GameManager* manager, void* context) {
    UNUSED(level);

    dolphin_deed(DolphinDeedPluginGameStart);

    GameContext* game_context = game_manager_game_context_get(manager);
    game_context->score = 0;

    GameLevelContext* level_context = context;
    player_respawn(level_context->player);
    reset_target(level_context->target);

    FURI_LOG_D(GAME_NAME, "Game level started");
}

static void level_game_stop(Level* level, GameManager* manager, void* context) {
    UNUSED(manager);

    GameLevelContext* level_context = context;
    FOREACH(item, level_context->enemies) {
        level_remove_entity(level, *item);
    }
    EntityList_clear(level_context->enemies);

    resume_game(level);
}

void pause_game(Level* level) {
    GameLevelContext* level_context = level_context_get(level);
    if(level_context->is_paused) {
        return;
    }

    level_context->is_paused = true;
    level_context->pause_menu = level_add_entity(level, &context_menu_description);
    context_menu_back_callback_set(
        level_context->pause_menu, (ContextMenuBackCallback)resume_game, level);
}

void resume_game(Level* level) {
    GameLevelContext* level_context = level_context_get(level);
    if(!level_context->is_paused) {
        return;
    }

    level_context->is_paused = false;
    level_remove_entity(level, level_context->pause_menu);
    level_context->pause_menu = NULL;
}

const LevelBehaviour level_game = {
    .alloc = level_game_alloc,
    .free = NULL,
    .start = level_game_start,
    .stop = level_game_stop,
    .context_size = sizeof(GameLevelContext),
};
