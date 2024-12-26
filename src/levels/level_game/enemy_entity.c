#include "enemy_entity.h"

#include "../../engine/game_manager.h"
#include "../../game.h"

#include "level_game.h"
#include "player_entity.h"

#define ENEMY_SIZE 6

static Vector
random_pos(void)
{
    return (Vector){ rand() % 120 + 4, rand() % 58 + 4 };
}

void
spawn_enemy(GameManager* manager)
{
    GameContext* game_context = game_manager_game_context_get(manager);
    Level* level = game_manager_current_level_get(manager);
    Entity* enemy = level_add_entity(level, &enemy_description);

    // Add to enemies list
    GameLevelContext* level_context = level_context_get(level);
    EntityList_push_back(level_context->enemies, enemy);

    // Set enemy position
    entity_pos_set(enemy, random_pos());

    // Add collision rect to enemy entity
    entity_collider_add_rect(enemy, ENEMY_SIZE, ENEMY_SIZE);

    // Load target sprite
    EnemyContext* enemy_context = entity_context_get(enemy);
    enemy_context->sprite = game_manager_sprite_load(manager, "enemy.fxbm");
    enemy_context->direction = (EnemyDirection)(rand() % 4);

    float speed;
    switch (game_context->difficulty) {
        case DifficultyEasy:
            speed = 0.25f;
            break;
        case DifficultyHard:
            speed = 1.0f;
            break;
        case DifficultyInsane:
            speed = 1.5f;
            break;
        default:
            speed = 0.5f;
            break;
    }
    enemy_context->speed = speed;
}

static void
enemy_update(Entity* self, GameManager* manager, void* context)
{
    Level* level = game_manager_current_level_get(manager);
    GameLevelContext* level_context = level_context_get(level);
    if (level_context->is_paused) {
        return;
    }

    EnemyContext* enemy_context = context;
    Vector pos = entity_pos_get(self);

    // Control player movement
    if (enemy_context->direction == EnemyDirectionUp)
        pos.y -= enemy_context->speed;
    if (enemy_context->direction == EnemyDirectionDown)
        pos.y += enemy_context->speed;
    if (enemy_context->direction == EnemyDirectionLeft)
        pos.x -= enemy_context->speed;
    if (enemy_context->direction == EnemyDirectionRight)
        pos.x += enemy_context->speed;

    // Clamp player position to screen bounds, and set it
    pos.x = CLAMP(pos.x, 125, 3);
    pos.y = CLAMP(pos.y, 61, 3);
    entity_pos_set(self, pos);

    if (enemy_context->direction == EnemyDirectionUp && pos.y <= 3.0f)
        enemy_context->direction = EnemyDirectionDown;
    else if (enemy_context->direction == EnemyDirectionDown && pos.y >= 61.0f)
        enemy_context->direction = EnemyDirectionUp;
    else if (enemy_context->direction == EnemyDirectionLeft && pos.x <= 3.0f)
        enemy_context->direction = EnemyDirectionRight;
    else if (enemy_context->direction == EnemyDirectionRight && pos.x >= 125.0f)
        enemy_context->direction = EnemyDirectionLeft;
}

static void
enemy_render(Entity* self, GameManager* manager, Canvas* canvas, void* context)
{
    UNUSED(context);
    UNUSED(manager);

    // Get enemy position
    Vector pos = entity_pos_get(self);

    // Draw enemy
    EnemyContext* enemy_context = entity_context_get(self);
    canvas_draw_sprite(canvas, enemy_context->sprite, pos.x - 3, pos.y - 3);
}

static void
enemy_collision(Entity* self,
                Entity* other,
                GameManager* manager,
                void* context)
{
    UNUSED(self);
    UNUSED(context);

    if (entity_description_get(other) != &player_description) {
        return;
    }

    // Game over
    GameContext* game_context = game_manager_game_context_get(manager);
    game_manager_next_level_set(manager, game_context->levels.game_over);
}

const EntityDescription enemy_description = {
    .start = NULL,
    .stop = NULL,
    .update = enemy_update,
    .render = enemy_render,
    .collision = enemy_collision,
    .event = NULL,
    .context_size = sizeof(EnemyContext),
};
