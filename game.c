#include "game.h"

/****** Entities: Player ******/
static const LevelBehaviour level;
static bool GameOver = false;

static const int player_size = 6;
static const int player_radius = player_size / 2;
static const int player_x_max = 127 - player_radius;
static const int player_x_min = player_radius + 1;

static const int ghost_size = 6;
static const int ghost_radius = ghost_size / 2;
static const int ghost_x_max = 128 - ghost_radius;
static const int ghost_x_min = ghost_radius;

static const int target_buffer_max = 12;
typedef struct {
    Sprite* sprite;
    float speed;
} PlayerContext;

typedef enum {
    NORMAL,
    EDIBLE,
    EATEN,
} GhostState;

typedef struct {
    Sprite* sprite;
    int num_sprites;
    float speed;
    GhostState state;
    int frames;
} Ghost;

typedef enum {
    StopGame,
} GameEvent;

static bool direction = true;

// Forward declaration of player_desc, because it's used in player_spawn function.
static const EntityDescription player_desc;

//Initial Player location upon game start:
static Vector last_player_position = {34, 32};
static Vector default_ghost_position = {122, 32};

static Vector gen_target_pos(int index) {
    return (Vector){
        .x = (120 * (1 - ((double)index / target_buffer_max))) + 4,
        .y = 32
    };
}

static const EntityDescription target_desc;
static const EntityDescription food_desc;
static bool is_level_empty(Level* level) {
    int count = level_entity_count(level, &target_desc) + level_entity_count(level, &food_desc);

    FURI_LOG_I("target_collision", "Total food + target count is %d", count);
    return count == 0;
}

// Forward declaration, so we can reset when all targets and food have been eaten
static void target_and_food_spawn(Level* level);

static void player_spawn(Level* level, GameManager* manager) {
    Entity* player = level_add_entity(level, &player_desc);

    // Set player position.
    // Depends on your game logic, it can be done in start entity function, but also can be done here.
    entity_pos_set(player, last_player_position);

    // Add collision box to player entity
    entity_collider_add_rect(player, player_size, player_size);

    // Get player context
    PlayerContext* player_context = entity_context_get(player);

    // Load player sprite
    player_context->sprite = game_manager_sprite_load(manager, "player.fxbm");
    player_context->speed = 4.0;
}

static void player_update(Entity* self, GameManager* manager, void* context) {
    UNUSED(context);

    // Get game input
    InputState input = game_manager_input_get(manager);

    // Get player position
    Vector pos = entity_pos_get(self);
	
    // Get player context
    PlayerContext* player_context = (PlayerContext*)context;
	// Get game context
	GameContext* game_context = game_manager_game_context_get(manager);

	char left_path[50] = "player_left_ .fxbm";
	int left_path_index= 12;
	char right_path[50] = "player_right_ .fxbm";
	int right_path_index= 13;
	
	if (input.pressed & GameKeyOk && GameOver) {
		GameOver = false;
		pos.x = 34;
		pos.y = 32;
		player_context->speed = 4.0;
		game_context->score = 0;
		last_player_position = pos;
		Level* nextlevel = game_manager_add_level(manager, &level);
		game_manager_next_level_set(manager, nextlevel);
		FURI_LOG_I("player_update", "Game Over is %d", GameOver);
		FURI_LOG_I("player_update", "Player position is %d, %d", (int)pos.x, (int)pos.y);
	}
	if (input.pressed & GameKeyOk && !GameOver) {
		direction = !direction;
	}
	if (input.held & GameKeyLeft) {
		direction = false;
	}
	if (input.held & GameKeyRight) {
		direction = true;
	}
    if(!direction) {
		pos.x -= player_context->speed; //offset by one so that sprite can cycle through animation
		int frame = (int)pos.x % 3 + 1;//This references sprite files - out of range causes crashes
		left_path[left_path_index] = '0' + frame; 
		FURI_LOG_I("left_path", "left_path is %s", left_path);
		player_context->sprite = game_manager_sprite_load(manager, left_path);
	}
    if(direction) 
	{
		pos.x += player_context->speed;
		int frame = (int)(pos.x) % 3 + 1;
		right_path[right_path_index] = '0' + frame;
		FURI_LOG_I("right_path", "right path is %s", right_path);
		player_context->sprite = game_manager_sprite_load(manager, right_path);
	}

    // Set wraparound for player
	if (pos.x < player_x_min) {
		pos.x = player_x_max;
	}
	if (pos.x > player_x_max) {
		pos.x = player_x_min;
	}

    entity_pos_set(self, pos);

    // Control game exit
    if(input.pressed & GameKeyBack) {
        game_manager_game_stop(manager);
    }
}

static void player_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    // Get player context
    PlayerContext* player = context;

    // Get player position
    Vector pos = entity_pos_get(self);

    // Draw player sprite
    // We subtract 5 from x and y, because collision box is 10x10, and we want to center sprite in it.
    canvas_draw_sprite(canvas, player->sprite, pos.x - player_radius, pos.y - player_radius);

    // Get game context
    GameContext* game_context = game_manager_game_context_get(manager);

    // Draw score
    canvas_printf(canvas, 0, 7, "Score: %lu", game_context->score);

    if (player->speed == 0) {
        canvas_printf(canvas, 0, 16, "Game Over");
		GameOver = true;
    }
}

static const EntityDescription player_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = player_update, // called every frame
    .render = player_render, // called every frame, after update
    .collision = NULL, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size =
        sizeof(PlayerContext), // size of entity context, will be automatically allocated and freed
};


/****** Entities: Ghost ******/

static const EntityDescription ghost_desc;

static void ghost_reset(Ghost* ghost, GameManager* manager) {
    ghost->sprite = game_manager_sprite_load(manager, "ghost_left_1.fxbm");
    ghost->state = NORMAL;
    ghost->num_sprites = 2;
    ghost->speed = 4.2;
    ghost->frames = -1;
}

static void ghost_edible_set(Ghost* ghost) {
    ghost->state = EDIBLE;
    ghost->speed = 1.0;
    ghost->frames = game.target_fps * 2;
}

static void ghost_eaten_set(Ghost* ghost) {
    ghost->state = EATEN;
    ghost->speed = 6.0;
}

static void ghost_spawn(Level* level, GameManager* manager) {
    Entity* entity = level_add_entity(level, &ghost_desc);

    entity_pos_set(entity, default_ghost_position);

    // Add collision box to ghost entity
    entity_collider_add_rect(entity, ghost_size - 1, ghost_size - 1);

    // Get ghost context
    Ghost* ghost = entity_context_get(entity);

    // Load ghost sprite and set speed to 0
    ghost_reset(ghost, manager);
}

static void ghost_update(Entity* self, GameManager* manager, void* context) {
    UNUSED(manager);

    Ghost* ghost = context;

    // Get player entity
    Level* level = game_manager_current_level_get(manager);
    Entity* player = level_entity_get(level, &player_desc, 0);
    if (player == NULL) {
        return;
    }
    Vector player_pos = entity_pos_get(player);
    Vector ghost_pos = entity_pos_get(self);

    // Ghost mode handling
    if (ghost->state == EDIBLE) {
        if (ghost->frames == 0) {
            // Timer is up
            ghost_reset(ghost, manager);
        } else {
            // Ghost is stil edible
            ghost->frames--;
        }
    } else if (ghost->state == EATEN) {
        ghost->sprite = game_manager_sprite_load(manager, "ghost_eaten.fxbm");
    }

    // Set position based on ghost mode
    if (ghost->state == EDIBLE) {
        if (ghost_pos.x < player_pos.x) {
            ghost_pos.x -= ghost->speed;
        } else {
            ghost_pos.x += ghost->speed;
        }
    } else if (ghost->state == NORMAL) {
        if (ghost_pos.x < player_pos.x) {
            ghost_pos.x += ghost->speed;
        } else {
            ghost_pos.x -= ghost->speed;
        }
    } else {
        ghost_pos.x -= ghost->speed;
    }

    // Ensure new ghost position is within bounds
    ghost_pos.x = CLAMP(ghost_pos.x, ghost_x_max, ghost_x_min);

    if (ghost->state == EATEN && ghost_pos.x == 3) {
        ghost_reset(ghost, manager);
    }

    entity_pos_set(self, ghost_pos);
}

static void ghost_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(manager);

    // Get ghost context
    Ghost* ghost = context;

    // Get ghost position
    Vector pos = entity_pos_get(self);

    // Draw ghost sprite
    canvas_draw_sprite(canvas, ghost->sprite, pos.x - ghost_radius, pos.y - ghost_radius);
}

static void ghost_collision(Entity* self, Entity* other, GameManager* manager, void* context) {
    UNUSED(self);
    UNUSED(context);
    UNUSED(manager);

    if(entity_description_get(other) == &player_desc) {
        Ghost* ghost = context;
        if (ghost->state == EDIBLE || ghost->state == EATEN) {
            ghost_eaten_set(ghost);
        } else {
            PlayerContext* player_context = entity_context_get(other);
            player_context->speed = 0;
            ghost->speed = 0;
        }
    }
}

static const EntityDescription ghost_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = ghost_update, // called every frame
    .render = ghost_render, // called every frame, after update
    .collision = ghost_collision, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size =
        sizeof(Ghost), // size of entity context, will be automatically allocated and freed
};

/****** Entities: Target ******/

/*
static Vector random_pos(void) {
    //return (Vector){rand() % 120 + 4, rand() % 58 + 4};
	//having target return a random x position in line with sprite
	//magic number 32 found by trial and error based on the number provided in the original function
    return (Vector){rand() % target_buffer_max * 120 / target_buffer_max + 4, 31};
}
*/

static void target_start(Entity* self, GameManager* manager, void* context) {
    UNUSED(context);
    UNUSED(manager);
    //entity_pos_set(self, random_pos());
    // Add collision circle to target entity
    // Circle is centered in target x and y, and it's radius is 3
    entity_collider_add_circle(self, 3);
}

static void target_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(context);
    UNUSED(manager);

    // Get target position
    Vector pos = entity_pos_get(self);

    // Draw target
    canvas_draw_disc(canvas, pos.x, pos.y, 1);
}

static void target_collision(Entity* self, Entity* other, GameManager* manager, void* context) {
    UNUSED(context);
    // Check if target collided with player
    if(entity_description_get(other) == &player_desc) {
        // Increase score
        GameContext* game_context = game_manager_game_context_get(manager);
        game_context->score++;

		last_player_position = entity_pos_get(other);

        // Move target to new random position
		Level* current_level = game_manager_current_level_get(manager);
		Vector pos = entity_pos_get(self);
		FURI_LOG_I("target_collision", "Removing target at %d, %d",(int)pos.x, (int)pos.y);
		level_remove_entity(current_level, self);

		if (is_level_empty(current_level)) {
            target_and_food_spawn(current_level);
		}
    }
}

static const EntityDescription target_desc = {
    .start = target_start, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = NULL, // called every frame
    .render = target_render, // called every frame, after update
    .collision = target_collision, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size = 0, // size of entity context, will be automatically allocated and freed
};

static void target_spawn(Level* level, Vector pos) {
	Entity* target = level_add_entity(level, &target_desc);
	FURI_LOG_I("before pos_set", "Spawning target at %d, %d",(int)pos.x, (int)pos.y);
    entity_pos_set(target, pos);
}

/****** Entities: Big Dot ******/

static void food_start();
static void food_render();
static void food_collision();

static void food_spawn(Level* level, Vector pos) {
	Entity* food = level_add_entity(level, &food_desc);
	FURI_LOG_I("food_spawn", "Spawning food at %d, %d",(int)pos.x, (int)pos.y);
    entity_pos_set(food, pos);
}

static void food_start(Entity* self, GameManager* manager, void* context) {
    UNUSED(context);
    UNUSED(manager);

    entity_collider_add_circle(self, 3);
}

static void food_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(context);
    UNUSED(manager);

    // Get target position
    Vector pos = entity_pos_get(self);

    // Draw target
    canvas_draw_disc(canvas, pos.x, pos.y, 2);
}

static void food_collision(Entity* self, Entity* other, GameManager* manager, void* context) {
    UNUSED(context);

    // Check if target collided with player
    if(entity_description_get(other) == &player_desc) {
        // Increase score
        GameContext* game_context = game_manager_game_context_get(manager);
        game_context->score++;

		last_player_position = entity_pos_get(other);

		Level* current_level = game_manager_current_level_get(manager);
		Vector pos = entity_pos_get(self);
		FURI_LOG_I("food_collision", "Removing target at %d, %d",(int)pos.x, (int)pos.y);
		level_remove_entity(current_level, self);

        Entity* ghost_entity = level_entity_get(current_level, &ghost_desc, 0);
        Ghost* ghost = entity_context_get(ghost_entity);
        ghost_edible_set(ghost);

		if (is_level_empty(current_level)) {
            target_and_food_spawn(current_level);
		}
    }
}

static const EntityDescription food_desc = {
    .start = food_start, // called when entity is added to the level
    .stop = NULL, // called when entity is removed from the level
    .update = NULL, // called every frame
    .render = food_render, // called every frame, after update
    .collision = food_collision, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size = 0, // size of entity context, will be automatically allocated and freed
};

/****** Level ******/

static void target_and_food_spawn(Level* level) {
    int food_index = rand() % target_buffer_max;

	for (int i = target_buffer_max; i > 0; i--)
	{
        Vector pos = gen_target_pos(i);
        if (i == food_index) {
            food_spawn(level, pos);
        } else {
            target_spawn(level, pos);
        }
	}
}

static void level_alloc(Level* level, GameManager* manager, void* context) {
    UNUSED(manager);
    UNUSED(context);

    // Add player entity to the level
    player_spawn(level, manager);
    // Add ghost entity to the level
    ghost_spawn(level, manager);

    // Add target entity to the level
    // srand(time(0));
    int food_index = rand() % target_buffer_max;

	for (int i = target_buffer_max; i > 0; i--)
	{
        Vector pos = gen_target_pos(i);
        if (i == food_index) {
            food_spawn(level, pos);
        } else {
            target_spawn(level, pos);
        }
	}
}

/*
    Alloc/free is called once, when level is added/removed from the game. 
    It useful if you have small amount of levels and entities, that can be allocated at once.

    Start/stop is called when level is changed to/from this level.
    It will save memory, because you can allocate entities only for current level.
*/
static const LevelBehaviour level = {
    .alloc = level_alloc, // called once, when level allocated
    .free = NULL, // called once, when level freed
    .start = NULL, // called when level is changed to this level
    .stop = NULL, // called when level is changed from this level
    .context_size = 0, // size of level context, will be automatically allocated and freed
};

/****** Game ******/

/* 
    Write here the start code for your game, for example: creating a level and so on.
    Game context is allocated (game.context_size) and passed to this function, you can use it to store your game data.
*/
static void game_start(GameManager* game_manager, void* ctx) {
    UNUSED(game_manager);

    // Do some initialization here, for example you can load score from storage.
    // For simplicity, we will just set it to 0.
    GameContext* game_context = ctx;
    game_context->score = 0;

    // Add level to the game
    game_manager_add_level(game_manager, &level);
}

/* 
    Write here the stop code for your game, for example: freeing memory, if it was allocated.
    You don't need to free level, sprites or entities, it will be done automatically.
    Also, you don't need to free game_context, it will be done automatically, after this function.
*/
static void game_stop(void* ctx) {
    GameContext* game_context = ctx;
    // Do some deinitialization here, for example you can save score to storage.
    // For simplicity, we will just print it.
    FURI_LOG_I("Game", "Your score: %lu", game_context->score);
}

/*
    Yor game configuration, do not rename this variable, but you can change it's content here.  
*/
const Game game = {
    .target_fps = 10, // target fps, game will try to keep this value
    .show_fps = true, // show fps counter on the screen
    .always_backlight = true, // keep display backlight always on
    .start = game_start, // will be called once, when game starts
    .stop = game_stop, // will be called once, when game stops
    .context_size = sizeof(GameContext), // size of game context
};
