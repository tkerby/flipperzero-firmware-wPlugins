#include "game_over_entity.h"

#include "../../game.h"

static void
game_over_update(Entity* self, GameManager* manager, void* _entity_context)
{
    UNUSED(self);
    UNUSED(_entity_context);

    GameContext* game_context = game_manager_game_context_get(manager);

    InputState input = game_manager_input_get(manager);
    if (input.pressed & GameKeyOk || input.pressed & GameKeyBack) {
        game_manager_next_level_set(manager, game_context->levels.menu);
    }
}

static void
game_over_render(Entity* self,
                 GameManager* manager,
                 Canvas* canvas,
                 void* _entity_context)
{
    UNUSED(self);
    UNUSED(manager);

    GameOverEntityContext* entity_context = _entity_context;

    furi_check(entity_context->logo_sprite);

    const uint32_t screen_height = 64;
    const uint32_t screen_width = 128;
    const uint32_t text_height = 7;
    uint32_t x =
      screen_width / 2.0 - sprite_get_width(entity_context->logo_sprite) / 2.0;
    uint32_t y = (screen_height - text_height) / 2.0 -
                 sprite_get_height(entity_context->logo_sprite) / 2.0;

    canvas_draw_sprite(canvas, entity_context->logo_sprite, x, y);

    char score_str[12] = {};
    snprintf(score_str, sizeof(score_str), "SCORE: %ld", entity_context->score);

    char best_score_str[10] = {};
    snprintf(best_score_str,
             sizeof(best_score_str),
             "BEST: %ld",
             entity_context->max_score);

    canvas_set_color(canvas, ColorBlack);
    canvas_set_font(canvas, FontSecondary);
    canvas_draw_str_aligned(canvas, 0, 64, AlignLeft, AlignBottom, score_str);
    canvas_draw_str_aligned(
      canvas, 128, 64, AlignRight, AlignBottom, best_score_str);
}

const EntityDescription game_over_description = {
    .start = NULL,
    .stop = NULL,
    .update = game_over_update,
    .render = game_over_render,
    .collision = NULL,
    .event = NULL,
    .context_size = sizeof(GameOverEntityContext),
};
