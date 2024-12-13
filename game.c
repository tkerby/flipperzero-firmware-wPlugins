#include "game.h"
#include "flip_world.h"
#include "flip_world_icons.h"

Wall walls[] = {
    WALL(true, 12, 0, 3),
    WALL(false, 3, 3, 17),
    WALL(false, 23, 3, 6),
    WALL(true, 3, 4, 57),
    WALL(true, 28, 4, 56),
    WALL(false, 4, 7, 5),
    WALL(false, 12, 7, 13),
    WALL(true, 8, 8, 34),
    WALL(true, 12, 8, 42),
    WALL(true, 24, 8, 8),
    WALL(true, 16, 11, 8),
    WALL(false, 17, 11, 4),
    WALL(true, 20, 12, 22),
    WALL(false, 6, 17, 2),
    WALL(true, 24, 19, 15),
    WALL(true, 16, 22, 16),
    WALL(false, 4, 24, 1),
    WALL(false, 21, 28, 2),
    WALL(false, 6, 33, 2),
    WALL(false, 13, 34, 3),
    WALL(false, 17, 37, 11),
    WALL(true, 16, 41, 14),
    WALL(false, 20, 41, 5),
    WALL(true, 20, 45, 12),
    WALL(true, 24, 45, 12),
    WALL(false, 4, 46, 2),
    WALL(false, 9, 46, 3),
    WALL(false, 6, 50, 3),
    WALL(true, 12, 53, 7),
    WALL(true, 8, 54, 6),
    WALL(false, 4, 60, 19),
    WALL(false, 26, 60, 6),
};

// Global variables to store camera position
static int camera_x = 0;
static int camera_y = 0;

// Background rendering function
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
    canvas_draw_frame(canvas, -camera_x, -camera_y, WORLD_WIDTH, WORLD_HEIGHT);

    // Draw other elements adjusted by camera offset
    // Static Dot at (72, 40)
    canvas_draw_dot(canvas, 72 - camera_x, 40 - camera_y);

    // Static Circle at (16, 16) with radius 4
    canvas_draw_circle(canvas, 16 - camera_x, 16 - camera_y, 4);

    // Static 8x8 Rectangle Frame at (96, 48)
    canvas_draw_frame(canvas, 96 - camera_x, 48 - camera_y, 8, 8);

    // Static earth icon at (112, 56)
    canvas_draw_icon(canvas, 112 - camera_x, 56 - camera_y, &I_icon_earth);

    // static home icon at (128, 24)
    canvas_draw_icon(canvas, 128 - camera_x, 24 - camera_y, &I_icon_home);

    // static menu icon at (144, 24)
    canvas_draw_icon(canvas, 144 - camera_x, 24 - camera_y, &I_icon_info);

    // static man icon at (160, 56)
    canvas_draw_icon(canvas, 160 - camera_x, 56 - camera_y, &I_icon_man);

    // static plant icon at (176, 32)
    canvas_draw_icon(canvas, 176 - camera_x, 32 - camera_y, &I_icon_plant);

    // static tree icon at (104, 40)
    canvas_draw_icon(canvas, 192 - camera_x, 40 - camera_y, &I_icon_tree);

    // static woman icon at (208, 32)
    canvas_draw_icon(canvas, 208 - camera_x, 32 - camera_y, &I_icon_woman);
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
    entity_pos_set(player, (Vector){64, 32});

    // Add collision box to player entity
    // Box is centered in player x and y, and it's size is 10x10
    entity_collider_add_rect(player, 10, 10);

    // Get player context
    PlayerContext *player_context = entity_context_get(player);

    // Load player sprite
    player_context->sprite = game_manager_sprite_load(manager, "player.fxbm");
}

static void player_update(Entity *self, GameManager *manager, void *context)
{
    UNUSED(context);

    // Get game input
    InputState input = game_manager_input_get(manager);

    // Get player position
    Vector pos = entity_pos_get(self);

    // Control player movement
    if (input.held & GameKeyUp)
        pos.y -= 2;
    if (input.held & GameKeyDown)
        pos.y += 2;
    if (input.held & GameKeyLeft)
        pos.x -= 2;
    if (input.held & GameKeyRight)
        pos.x += 2;

    // Clamp player position to screen bounds, considering player sprite size (10x10)
    pos.x = CLAMP(pos.x, WORLD_WIDTH - 5, 5);
    pos.y = CLAMP(pos.y, WORLD_HEIGHT - 5, 5);

    // Set new player position
    entity_pos_set(self, pos);

    // Control game exit
    if (input.pressed & GameKeyBack)
    {
        game_manager_game_stop(manager);
    }
}

static void player_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    // Get player context
    PlayerContext *player = context;

    // Get player position
    Vector pos = entity_pos_get(self);

    // Draw background (updates camera_x and camera_y)
    background_render(canvas, pos);

    // Draw player sprite relative to camera
    canvas_draw_sprite(canvas, player->sprite, pos.x - camera_x - 5, pos.y - camera_y - 5);

    // Get game context
    GameContext *game_context = game_manager_game_context_get(manager);

    // Draw score (optional)
    UNUSED(game_context);
    // canvas_printf(canvas, 0, 7, "Score: %lu", game_context->score);
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

/****** Entities: Target ******/

static Vector random_pos(void)
{
    return (Vector){rand() % (SCREEN_WIDTH - 8) + 4, rand() % (SCREEN_HEIGHT - 8) + 4};
}

static void target_start(Entity *self, GameManager *manager, void *context)
{
    UNUSED(context);
    UNUSED(manager);
    // Set target position
    entity_pos_set(self, random_pos());
    // Add collision circle to target entity
    // Circle is centered in target x and y, and it's radius is 3
    entity_collider_add_circle(self, 3);
}

static void target_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    UNUSED(context);
    UNUSED(manager);

    // Get target position
    Vector pos = entity_pos_get(self);

    // Draw target relative to the camera
    canvas_draw_disc(canvas, pos.x - camera_x, pos.y - camera_y, 3);
}

static void target_collision(Entity *self, Entity *other, GameManager *manager, void *context)
{
    UNUSED(context);
    // Check if target collided with player
    if (entity_description_get(other) == &player_desc)
    {
        // Increase score
        GameContext *game_context = game_manager_game_context_get(manager);
        game_context->score++;

        // Move target to new random position
        entity_pos_set(self, random_pos());
    }
}

static const EntityDescription target_desc = {
    .start = target_start,         // called when entity is added to the level
    .stop = NULL,                  // called when entity is removed from the level
    .update = NULL,                // called every frame
    .render = target_render,       // called every frame, after update
    .collision = target_collision, // called when entity collides with another entity
    .event = NULL,                 // called when entity receives an event
    .context_size = 0,             // size of entity context, will be automatically allocated and freed
};

/****** Entities: Wall ******/

static uint8_t wall_index;

static void wall_start(Entity *self, GameManager *manager, void *context);

typedef struct
{
    float width;
    float height;
} WallContext;

static void wall_render(Entity *self, GameManager *manager, Canvas *canvas, void *context)
{
    UNUSED(manager);
    UNUSED(self);
    UNUSED(canvas);
    UNUSED(context);

    // WallContext *wall = context;

    // Vector pos = entity_pos_get(self);

    // Draw the wall relative to the camera
    // canvas_draw_box(
    //     canvas,
    //     pos.x - camera_x - (wall->width / 2),
    //     pos.y - camera_y - (wall->height / 2),
    //     wall->width,
    //     wall->height);
}

static void wall_collision(Entity *self, Entity *other, GameManager *manager, void *context)
{
    WallContext *wall = context;

    // Check if wall collided with player
    if (entity_description_get(other) == &player_desc)
    {
        // Increase score
        GameContext *game_context = game_manager_game_context_get(manager);
        game_context->score++;

        PlayerContext *player = (PlayerContext *)entity_context_get(other);
        if (player)
        {
            if (player->dx || player->dy)
            {
                Vector pos = entity_pos_get(other);

                // TODO: Based on where we collided, we should still slide across/down the wall.
                UNUSED(wall);

                if (player->dx)
                {
                    FURI_LOG_D(
                        "Player",
                        "Player collided with wall, dx: %d.  center:%f,%f",
                        player->dx,
                        (double)pos.x,
                        (double)pos.y);
                    pos.x -= player->dx;
                    player->dx = 0;
                }
                if (player->dy)
                {
                    FURI_LOG_D(
                        "Player",
                        "Player collided with wall, dy: %d.  center:%f,%f",
                        player->dy,
                        (double)pos.x,
                        (double)pos.y);
                    pos.y -= player->dy;
                    player->dy = 0;
                }
                entity_pos_set(other, pos);
                FURI_LOG_D("Player", "Set to center:%f,%f", (double)pos.x, (double)pos.y);
            }
        }
        else
        {
            FURI_LOG_D("Player", "Player collided with wall, but context null.");
        }
    }
    else
    {
        // HACK: Wall touching other items destroys each other (to help find collider issues)
        Level *level = game_manager_current_level_get(manager);
        level_remove_entity(level, self);
        level_remove_entity(level, other);
    }
}

static const EntityDescription wall_desc = {
    .start = wall_start,         // called when entity is added to the level
    .stop = NULL,                // called when entity is removed from the level
    .update = NULL,              // called every frame
    .render = wall_render,       // called every frame, after update
    .collision = wall_collision, // called when entity collides with another entity
    .event = NULL,               // called when entity receives an event
    .context_size =
        sizeof(WallContext), // size of entity context, will be automatically allocated and freed
};

static void wall_start(Entity *self, GameManager *manager, void *context)
{
    UNUSED(manager);

    WallContext *wall = context;

    // TODO: We can get the current number of items from the level (instead of wall_index).

    if (wall_index < COUNT_OF(walls))
    {
        if (walls[wall_index].horizontal)
        {
            wall->width = walls[wall_index].length * 2;
            wall->height = 1 * 2;
        }
        else
        {
            wall->width = 1 * 2;
            wall->height = walls[wall_index].length * 2;
        }

        entity_pos_set(
            self,
            (Vector){
                walls[wall_index].x + wall->width / 2, walls[wall_index].y + wall->height / 2});

        entity_collider_add_rect(self, wall->width, wall->height);

        wall_index++;
    }
}

/****** Level ******/

static void level_alloc(Level *level, GameManager *manager, void *context)
{
    UNUSED(manager);
    UNUSED(context);

    // Add player entity to the level
    player_spawn(level, manager);

    // Add first target entity to the level
    level_add_entity(level, &target_desc);

    // Add wall entities to the level
    wall_index = 0;
    for (size_t i = 0; i < COUNT_OF(walls); i++)
    {
        level_add_entity(level, &wall_desc);
    }
}

static const LevelBehaviour level = {
    .alloc = level_alloc, // called once, when level allocated
    .free = NULL,         // called once, when level freed
    .start = NULL,        // called when level is changed to this level
    .stop = NULL,         // called when level is changed from this level
    .context_size = 0,    // size of level context, will be automatically allocated and freed
};

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
