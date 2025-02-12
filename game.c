#include "game.h"

/****** Entities: Player ******/
static const LevelBehaviour level;

// static int target_buffer = 7;
static const int target_buffer_max = 12;
typedef struct {
    Sprite* sprite;
} PlayerContext;

static bool direction = true;

// Forward declaration of player_desc, because it's used in player_spawn function.
static const EntityDescription player_desc;

//Initial Player location upon game start:
static Vector last_player_position = {64, 32}; // Defa

static Vector gen_target_pos(int index) {
    return (Vector){
        .x = (120 * (1 - ((double)index / target_buffer_max))) + 4,
        .y = 30
    };
}

static const EntityDescription target_desc;
static const EntityDescription food_desc;
static bool is_level_empty(Level* level) {
    int count = level_entity_count(level, &target_desc) + level_entity_count(level, &food_desc);

    FURI_LOG_I("target_collision", "Total food + target count is %d", count);
    return count == 0;
}

static void player_spawn(Level* level, GameManager* manager) {
    Entity* player = level_add_entity(level, &player_desc);

    // Set player position.
    // Depends on your game logic, it can be done in start entity function, but also can be done here.
    entity_pos_set(player, last_player_position);

    // Add collision box to player entity
    // Box is centered in player x and y, and it's size is 3x3 (because its eating dots)
    entity_collider_add_rect(player, 3, 3);

    // Get player context
    PlayerContext* player_context = entity_context_get(player);

    // Load player sprite
    player_context->sprite = game_manager_sprite_load(manager, "player.fxbm");
}

static void player_update(Entity* self, GameManager* manager, void* context) {
    UNUSED(context);

    // Get game input
    InputState input = game_manager_input_get(manager);

    // Get player position
    Vector pos = entity_pos_get(self);
	
    // Get player context
    PlayerContext* player_context = (PlayerContext*)context;

	char left_path[50] = "player_left_ .fxbm";
	int left_path_index= 12;
	char right_path[50] = "player_right_ .fxbm";
	int right_path_index= 13;
    // Control player movement
	//Removing player up and down movement
    //if(input.held & GameKeyUp) pos.y -= 2;
    //if(input.held & GameKeyDown) pos.y += 2;
	
	if (input.pressed & GameKeyOk) {
		direction = !direction;
	}
	if (input.held & GameKeyLeft) {
		direction = false;
	}
	if (input.held & GameKeyRight) {
		direction = true;
	}
    if(!direction) {
		pos.x -= 4; //offset by one so that sprite can cycle through animation
		int frame = (int)pos.x % 3 + 1;//This references sprite files - out of range causes crashes
		left_path[left_path_index] = '0' + frame; 
		FURI_LOG_I("left_path", "left_path is %s", left_path);
		player_context->sprite = game_manager_sprite_load(manager, left_path);
	}
    if(direction) 
	{
		pos.x += 4;
		int frame = (int)(pos.x) % 3 + 1;
		right_path[right_path_index] = '0' + frame;
		FURI_LOG_I("right_path", "right path is %s", right_path);
		player_context->sprite = game_manager_sprite_load(manager, right_path);
	}

    // Clamp player position to screen bounds, considering player sprite size (10x10)
    pos.x = CLAMP(pos.x, 125, 3);
    pos.y = CLAMP(pos.y, 59, 5);

    // Set wraparound for player
	if (pos.x < 5) {
		pos.x=123;
	}
	if (pos.x > 123) {
		pos.x = 5;
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
    canvas_draw_sprite(canvas, player->sprite, pos.x - 5, pos.y - 5);

    // Get game context
    GameContext* game_context = game_manager_game_context_get(manager);

    // Draw score
    canvas_printf(canvas, 0, 7, "Score: %lu", game_context->score);
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
			Level* nextlevel = game_manager_add_level(manager, &level);
			game_manager_next_level_set(manager, nextlevel);
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

    // FIXME: remove
    UNUSED(self);

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

		if (is_level_empty(current_level)) {
			Level* nextlevel = game_manager_add_level(manager, &level);
			game_manager_next_level_set(manager, nextlevel);
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

static void level_alloc(Level* level, GameManager* manager, void* context) {
    UNUSED(manager);
    UNUSED(context);

    // Add player entity to the level
    player_spawn(level, manager);

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
