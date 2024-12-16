#include "game.h"
#include "flip_world.h"
#include "flip_world_icons.h"

// Background rendering function
// TODO: each object needs a collision box so we can detect collisions and prevent movement through walls.
static void background_render(Canvas *canvas, Vector pos)
{
    // Clear the canvas
    canvas_clear(canvas);

    // Calculate camera offset to center the player
    camera_x = pos.x - (SCREEN_WIDTH / 2);
    camera_y = pos.y - (SCREEN_HEIGHT / 2);

    // Clamp camera position to prevent showing areas outside the world
    camera_x = CLAMP(camera_x, WORLD_WIDTH - SCREEN_WIDTH, 0);
    camera_y = CLAMP(camera_y, WORLD_HEIGHT - SCREEN_HEIGHT, 0);

    // Draw the outer bounds adjusted by camera offset
    draw_bounds(canvas);
}

/****** Entities: Player ******/

typedef struct
{
    Vector trajectory; // Direction player would like to move.
    float radius;      // collision radius
    int8_t dx;         // x direction
    int8_t dy;         // y direction
    Sprite *sprite;    // player sprite
} PlayerContext;

// Forward declaration of player_desc, because it's used in player_spawn function.
static const EntityDescription player_desc;

static void player_spawn(Level *level, GameManager *manager)
{
    Entity *player = level_add_entity(level, &player_desc);

    // Set player position.
    // Depends on your game logic, it can be done in start entity function, but also can be done here.
    entity_pos_set(player, (Vector){WORLD_WIDTH / 2, WORLD_HEIGHT / 2});

    // Add collision box to player entity
    // Box is centered in player x and y, and it's size is 10x10
    entity_collider_add_rect(player, 10, 10);

    // Get player context
    PlayerContext *player_context = entity_context_get(player);

    // Load player sprite
    player_context->sprite = game_manager_sprite_load(manager, "player.fxbm");
}

// Modify player_update to track direction
static void player_update(Entity *self, GameManager *manager, void *context)
{
    PlayerContext *player = (PlayerContext *)context;
    InputState input = game_manager_input_get(manager);
    Vector pos = entity_pos_get(self);

    // Reset direction each frame
    player->dx = 0;
    player->dy = 0;

    if (input.held & GameKeyUp)
    {
        pos.y -= 2;
        player->dy = -1;
    }
    if (input.held & GameKeyDown)
    {
        pos.y += 2;
        player->dy = 1;
    }
    if (input.held & GameKeyLeft)
    {
        pos.x -= 2;
        player->dx = -1;
    }
    if (input.held & GameKeyRight)
    {
        pos.x += 2;
        player->dx = 1;
    }

    pos.x = CLAMP(pos.x, WORLD_WIDTH - 5, 5);
    pos.y = CLAMP(pos.y, WORLD_HEIGHT - 5, 5);

    entity_pos_set(self, pos);

    if (input.pressed & GameKeyBack)
    {
        game_manager_game_stop(manager);
    }
}

static void player_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    // Get player context
    UNUSED(manager);
    PlayerContext *player = context;

    // Get player position
    Vector pos = entity_pos_get(self);

    // Draw background (updates camera_x and camera_y)
    background_render(canvas, pos);

    // Draw player sprite relative to camera
    canvas_draw_sprite(canvas, player->sprite, pos.x - camera_x - 5, pos.y - camera_y - 5);
}

static const EntityDescription player_desc = {
    .start = NULL,                         // called when entity is added to the level
    .stop = NULL,                          // called when entity is removed from the level
    .update = player_update,               // called every frame
    .render = player_render,               // called every frame, after update
    .collision = NULL,                     // called when entity collides with another entity
    .event = NULL,                         // called when entity receives an event
    .context_size = sizeof(PlayerContext), // size of entity context, will be automatically allocated and freed
};

/****** Level ******/

static void level_alloc(Level *level, GameManager *manager, void *context)
{
    UNUSED(manager);
    UNUSED(context);

    // Add player entity to the level
    player_spawn(level, manager);

    draw_tree_world(level);
    // draw_example_world(level);
}

static const LevelBehaviour level = {
    .alloc = level_alloc, // called once, when level allocated
    .free = NULL,         // called once, when level freed
    .start = NULL,        // called when level is changed to this level
    .stop = NULL,         // called when level is changed from this level
    .context_size = 0,    // size of level context, will be automatically allocated and freed
};

typedef struct
{
    const Icon *icon;
} IconContext;

// Forward declaration of icon_desc
static const EntityDescription icon_desc;

static void icon_collision(Entity *self, Entity *other, GameManager *manager, void *context)
{
    UNUSED(manager);
    UNUSED(self);
    IconContext *icon = (IconContext *)context;
    UNUSED(icon);
    if (entity_description_get(other) == &player_desc)
    {
        PlayerContext *player = (PlayerContext *)entity_context_get(other);
        if (player)
        {
            Vector pos = entity_pos_get(other);
            // Bounce the player back by 3 units opposite their last movement direction
            pos.x -= player->dx * 3;
            pos.y -= player->dy * 3;
            entity_pos_set(other, pos);
        }
    }
}

static void icon_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    UNUSED(manager);
    IconContext *icon_ctx = (IconContext *)context;
    Vector pos = entity_pos_get(self);
    canvas_draw_icon(canvas, pos.x - camera_x - 8, pos.y - camera_y - 8, icon_ctx->icon);
}

static void icon_start(Entity *self, GameManager *manager, void *context)
{
    UNUSED(manager);
    UNUSED(context);
    // Just add the collision rectangle for 16x16 icon
    entity_collider_add_rect(self, 16, 16);
}

static const EntityDescription icon_desc = {
    .start = icon_start,
    .stop = NULL,
    .update = NULL,
    .render = icon_render,
    .collision = icon_collision,
    .event = NULL,
    .context_size = sizeof(IconContext),
};

// Helper function to spawn an icon entity at a given position
void spawn_icon(Level *level, const Icon *icon, float x, float y)
{
    Entity *e = level_add_entity(level, &icon_desc);
    IconContext *icon_ctx = entity_context_get(e);
    icon_ctx->icon = icon;
    // Set the entity position to the center of the icon
    entity_pos_set(e, (Vector){x + 8, y + 8});
}

/****** Game ******/

/*
    Write here the start code for your game, for example: creating a level and so on.
    Game context is allocated (game.context_size) and passed to this function, you can use it to store your game data.
*/
static void game_start(GameManager *game_manager, void *ctx)
{
    UNUSED(game_manager);

    // Do some initialization here, for example you can load score from storage.
    // For simplicity, we will just set it to 0.
    GameContext *game_context = ctx;
    game_context->score = 0;

    // Add level to the game
    game_manager_add_level(game_manager, &level);
}

/*
    Write here the stop code for your game, for example, freeing memory, if it was allocated.
    You don't need to free level, sprites or entities, it will be done automatically.
    Also, you don't need to free game_context, it will be done automatically, after this function.
*/
static void game_stop(void *ctx)
{
    UNUSED(ctx);
    // GameContext *game_context = ctx;
    //  Do some deinitialization here, for example you can save score to storage.
    //  For simplicity, we will just print it.
    // FURI_LOG_I("Game", "Your score: %lu", game_context->score);
}

/*
    Your game configuration, do not rename this variable, but you can change its content here.
*/
const Game game = {
    .target_fps = 30,                    // target fps, game will try to keep this value
    .show_fps = false,                   // show fps counter on the screen
    .always_backlight = true,            // keep display backlight always on
    .start = game_start,                 // will be called once, when game starts
    .stop = game_stop,                   // will be called once, when game stops
    .context_size = sizeof(GameContext), // size of game context
};
