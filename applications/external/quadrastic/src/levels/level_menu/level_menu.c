#include "level_menu.h"

#include <stddef.h>

#include "../../engine/vector.h"

#include "../../game.h"

#include "blinking_sprite.h"
#include "delayed_sprite.h"
#include "menu_entity.h"
#include "moving_sprite.h"

typedef struct {
    Entity* quadrastic_logo;
    Entity* press_ok;
    Entity* left_button;
    Entity* right_button;

} LevelMenuContext;

static void level_menu_alloc(Level* level, GameManager* manager, void* context) {
    LevelMenuContext* menu_context = context;

    static const float initial_amimation_duration = 45.0f;

    // Quadrastic logo
    menu_context->quadrastic_logo = level_add_entity(level, &moving_sprite_description);
    moving_sprite_init(
        menu_context->quadrastic_logo,
        manager,
        (Vector){.x = 9, .y = 64},
        (Vector){.x = 9, .y = 2},
        initial_amimation_duration,
        "quadrastic.fxbm");

    // Press ok logo
    menu_context->press_ok = level_add_entity(level, &blinking_sprite_description);
    blinking_sprite_init(
        menu_context->press_ok,
        manager,
        (Vector){.x = 31, .y = 33},
        initial_amimation_duration,
        15.0f,
        7.0f,
        "press_ok.fxbm");

    // Settings button
    menu_context->left_button = level_add_entity(level, &delayed_sprite_description);
    delayed_sprite_init(
        menu_context->left_button,
        manager,
        (Vector){.x = 0, .y = 57},
        initial_amimation_duration,
        "left_button.fxbm");

    // About button
    menu_context->right_button = level_add_entity(level, &delayed_sprite_description);
    delayed_sprite_init(
        menu_context->right_button,
        manager,
        (Vector){.x = 115, .y = 57},
        initial_amimation_duration,
        "right_button.fxbm");

    // Menu
    level_add_entity(level, &menu_description);
}

static void level_menu_start(Level* level, GameManager* manager, void* context) {
    UNUSED(level);
    UNUSED(manager);
    UNUSED(context);

    FURI_LOG_D(GAME_NAME, "Menu level started");
}

const LevelBehaviour level_menu = {
    .alloc = level_menu_alloc,
    .free = NULL,
    .start = level_menu_start,
    .stop = NULL,
    .context_size = sizeof(LevelMenuContext),
};
