// enemy.c
#include <game/enemy.h>

// Allocation function
static EnemyContext *enemy_generic_alloc(const char *id, int index, float x, float y, float width, float height)
{
    EnemyContext *context = malloc(sizeof(EnemyContext));
    if (!context)
    {
        FURI_LOG_E("Game", "Failed to allocate EnemyContext");
        return NULL;
    }
    snprintf(context->id, sizeof(context->id), "%s", id);
    context->index = index;
    context->x = x;
    context->y = y;
    context->width = width;
    context->height = height;
    // Initialize other fields as needed
    context->trajectory = (Vector){0, 0}; // Default trajectory
    context->sprite_right = NULL;         // Assign appropriate sprite
    context->sprite_left = NULL;          // Assign appropriate sprite
    context->is_looking_left = false;
    context->radius = 3.0f; // Default collision radius
    return context;
}

// Free function
static void enemy_generic_free(void *context)
{
    if (context != NULL)
    {
        EnemyContext *enemy_context = (EnemyContext *)context;

        // Free sprites if they were dynamically loaded
        if (enemy_context->sprite_right)
        {
            sprite_free(enemy_context->sprite_right);
        }
        if (enemy_context->sprite_left)
        {
            sprite_free(enemy_context->sprite_left);
        }

        free(context);
    }
}

// Enemy start function
static void enemy_start(Entity *self, GameManager *manager, void *context)
{
    UNUSED(manager);
    if (!self || !context)
        return;

    EnemyContext *enemy_context = (EnemyContext *)context;

    // Set enemy's initial position based on context
    entity_pos_set(self, (Vector){enemy_context->x, enemy_context->y});

    // Add collision circle based on the enemy's radius
    entity_collider_add_circle(self, enemy_context->radius);
}

// Enemy render function
static void enemy_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    UNUSED(manager);
    if (!self || !context || !canvas)
        return;

    EnemyContext *enemy_context = (EnemyContext *)context;

    // Get the position of the enemy
    Vector pos = entity_pos_get(self);

    // Draw the enemy sprite based on their direction
    if (enemy_context->is_looking_left && enemy_context->sprite_left)
    {
        canvas_draw_sprite(canvas, enemy_context->sprite_left, pos.x, pos.y);
    }
    else if (enemy_context->sprite_right)
    {
        canvas_draw_sprite(canvas, enemy_context->sprite_right, pos.x, pos.y);
    }
    else
    {
        // Fallback if no sprite is set
        canvas_draw_disc(canvas, pos.x, pos.y, enemy_context->radius);
    }
}

// Enemy collision function
static void enemy_collision(Entity *self, Entity *other, GameManager *manager, void *context)
{
    UNUSED(self);
    UNUSED(context);

    // Check if the enemy collided with the player
    if (entity_description_get(other) == &player_desc)
    {
        // Increase score
        GameContext *game_context = game_manager_game_context_get(manager);
        if (game_context)
        {
            game_context->xp++;
        }

        // Move enemy to original position or handle respawn logic
        EnemyContext *enemy_context = (EnemyContext *)context;
        if (enemy_context)
        {
            entity_pos_set(self, (Vector){enemy_context->x, enemy_context->y});
        }
    }
}

// Enemy update function (optional)
static void enemy_update(Entity *self, GameManager *manager, void *context)
{
    if (!self || !context)
        return;
    UNUSED(manager);
    EnemyContext *enemy_context = (EnemyContext *)context;

    // Update position based on trajectory
    Vector pos = entity_pos_get(self);
    pos.x += enemy_context->trajectory.x;
    pos.y += enemy_context->trajectory.y;
    entity_pos_set(self, pos);

    // Example: Change direction if hitting boundaries
    if (pos.x < 0 || pos.x > WORLD_WIDTH)
    {
        enemy_context->trajectory.x *= -1;
        enemy_context->is_looking_left = !enemy_context->is_looking_left;
    }

    if (pos.y < 0 || pos.y > WORLD_HEIGHT)
    {
        enemy_context->trajectory.y *= -1;
    }
}

// Free function for the entity
static void enemy_free(Entity *self, GameManager *manager, void *context)
{
    UNUSED(self);
    UNUSED(manager);
    enemy_generic_free(context);
}

// Enemy behavior structure
static const EntityDescription _generic_enemy = {
    .start = enemy_start,
    .stop = enemy_free,
    .update = enemy_update,
    .render = enemy_render,
    .collision = enemy_collision,
    .event = NULL,
    .context_size = sizeof(EnemyContext),
};

// Enemy function to return the entity description
const EntityDescription *enemy(GameManager *manager, const char *id, int index, float x, float y, float width, float height, bool moving_left)
{
    // Allocate a new EnemyContext with provided parameters
    EnemyContext *context = enemy_generic_alloc(id, index, x, y, width, height);
    if (!context)
    {
        return NULL;
    }
    char right_edited[64];
    char left_edited[64];
    snprintf(right_edited, sizeof(right_edited), "%s_right.fxbm", id);
    snprintf(left_edited, sizeof(left_edited), "%s_left.fxbm", id);

    context->sprite_right = game_manager_sprite_load(manager, right_edited);
    context->sprite_left = game_manager_sprite_load(manager, left_edited);

    // Set initial direction
    context->is_looking_left = moving_left; // Default direction

    // set trajectory
    context->trajectory = !moving_left ? (Vector){1.0f, 0.0f} : (Vector){-1.0f, 0.0f}; // Default trajectory

    return &_generic_enemy;
}
