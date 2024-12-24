// enemy.c
#include <game/enemy.h>

static EnemyContext *enemy_context_generic;

// Allocation function
static EnemyContext *enemy_generic_alloc(const char *id, int index, float x, float y, float width, float height)
{
    if (!enemy_context_generic)
    {
        enemy_context_generic = malloc(sizeof(EnemyContext));
    }
    if (!enemy_context_generic)
    {
        FURI_LOG_E("Game", "Failed to allocate EnemyContext");
        return NULL;
    }
    snprintf(enemy_context_generic->id, sizeof(enemy_context_generic->id), "%s", id);
    enemy_context_generic->index = index;
    enemy_context_generic->x = x;
    enemy_context_generic->y = y;
    enemy_context_generic->width = width;
    enemy_context_generic->height = height;
    // Initialize other fields as needed
    enemy_context_generic->trajectory = (Vector){0, 0}; // Default trajectory
    enemy_context_generic->sprite_right = NULL;         // Assign appropriate sprite
    enemy_context_generic->sprite_left = NULL;          // Assign appropriate sprite
    enemy_context_generic->is_looking_left = false;
    enemy_context_generic->radius = 3.0f; // Default collision radius
    return enemy_context_generic;
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
    {
        FURI_LOG_E("Game", "Enemy start: Invalid parameters");
    }
    if (!enemy_context_generic)
    {
        FURI_LOG_E("Game", "Enemy start: Enemy context not set");
    }

    EnemyContext *enemy_context = (EnemyContext *)context;
    snprintf(enemy_context->id, sizeof(enemy_context->id), "%s", enemy_context_generic->id);
    enemy_context->index = enemy_context_generic->index;
    enemy_context->x = enemy_context_generic->x;
    enemy_context->y = enemy_context_generic->y;
    enemy_context->width = enemy_context_generic->width;
    enemy_context->height = enemy_context_generic->height;
    enemy_context->trajectory = enemy_context_generic->trajectory;
    enemy_context->sprite_right = enemy_context_generic->sprite_right;
    enemy_context->sprite_left = enemy_context_generic->sprite_left;
    enemy_context->is_looking_left = enemy_context_generic->is_looking_left;
    enemy_context->radius = enemy_context_generic->radius;

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

    // Draw enemy sprite relative to camera, centered on the enemy's position
    canvas_draw_sprite(
        canvas,
        enemy_context->is_looking_left ? enemy_context->sprite_left : enemy_context->sprite_right,
        pos.x - camera_x - 5, // Center the sprite horizontally
        pos.y - camera_y - 5  // Center the sprite vertically
    );
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
    enemy_context_generic = enemy_generic_alloc(id, index, x, y, width, height);
    if (!enemy_context_generic)
    {
        FURI_LOG_E("Game", "Failed to allocate EnemyContext");
        return NULL;
    }
    char right_edited[64];
    char left_edited[64];
    snprintf(right_edited, sizeof(right_edited), "%s_right.fxbm", id);
    snprintf(left_edited, sizeof(left_edited), "%s_left.fxbm", id);

    enemy_context_generic->sprite_right = game_manager_sprite_load(manager, right_edited);
    enemy_context_generic->sprite_left = game_manager_sprite_load(manager, left_edited);

    // Set initial direction
    enemy_context_generic->is_looking_left = moving_left; // Default direction

    // set trajectory
    enemy_context_generic->trajectory = !moving_left ? (Vector){1.0f, 0.0f} : (Vector){-1.0f, 0.0f}; // Default trajectory

    return &_generic_enemy;
}
