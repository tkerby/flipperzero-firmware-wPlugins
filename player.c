#include "player.h"

bool isGrounded;
bool isJumping;

void player_spawn(Level* level, GameManager* manager) {
    Entity* player = level_add_entity(level, &player_desc);

    // Set player position.
    // Depends on your game logic, it can be done in start entity function, but also can be done here.
    entity_pos_set(player, (Vector){15, 24});

    // Add collision box to player entity
    // Box is centered in player x and y, and it's size is 10x10
    entity_collider_add_rect(player, 15, 24);

    // Get player context
    PlayerContext* player_context = entity_context_get(player);

    // Load player sprite
    player_context->sprite = game_manager_sprite_load(manager, "other/player.fxbm");
}

void player_update(Entity* self, GameManager* manager, void* context) {
    PlayerContext* playerContext = (PlayerContext*)context;
    GameContext* game_context = game_manager_game_context_get(manager);
    InputState input = game_manager_input_get(manager);

    Vector pos = entity_pos_get(self);

    game_context->score = pos.y;

    // isGrounded check
    if((pos.y + 12) >= game_context->ground_hight && playerContext->Yvelocity >= 0) {
        pos.y = game_context->ground_hight - 12;
        playerContext->Yvelocity = 0;
        isGrounded = true;
    } else {
        isGrounded = false;
    }

    // gravity
    if(!isGrounded) {
        playerContext->Yvelocity += 0.5;
        pos.y += playerContext->Yvelocity;
    }

    if(input.held & GameKeyLeft) pos.x -= 2;
    if(input.held & GameKeyRight) pos.x += 2;

    // jump
    if(input.pressed & GameKeyUp && isGrounded) {
        playerContext->Yvelocity = -3.5;
    }

    // Clamp player position to screen bounds, considering player sprite size (10x10)
    pos.x = CLAMP(pos.x, 123, 5);
    pos.y = CLAMP(pos.y, 59, 5);

    // Set new player position
    entity_pos_set(self, pos);

    // Control game exit
    if(input.pressed & GameKeyBack) {
        game_manager_game_stop(manager);
    }
}

void player_collision(Entity* self, Entity* other, GameManager* manager, void* context) {
    UNUSED(context);
    UNUSED(self);
    UNUSED(manager);
    UNUSED(other);
    /*
    if(entity_description_get(other) == &ground_desc) {
        return;
    }
    */
}

void player_render(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    // Get player context
    PlayerContext* player = context;

    // Get player position
    Vector pos = entity_pos_get(self);

    // Draw player sprite
    canvas_draw_sprite(canvas, player->sprite, pos.x - 38, pos.y - 40);

    // Get game context
    GameContext* game_context = game_manager_game_context_get(manager);

    // Draw score
    canvas_printf(canvas, 0, 7, "Score: %lu", game_context->score);
}

void Animations(Entity* self, GameManager* manager, Canvas* canvas, void* context) {
    PlayerContext* playerContext = (PlayerContext*)context;
    UNUSED(self);
    UNUSED(canvas);
    UNUSED(playerContext);
    UNUSED(manager);
}

const EntityDescription player_desc = {
    .start = NULL, // called when entity is added to the level
    .stop = NULL, // called when entity is //*d from the level
    .update = player_update, // called every frame
    .render = player_render, // called every frame, after update
    .collision = player_collision, // called when entity collides with another entity
    .event = NULL, // called when entity receives an event
    .context_size =
        sizeof(PlayerContext), // size of entity context, will be automatically allocated and freed
};
