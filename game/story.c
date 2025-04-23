#include <game/story.h>
#include "game/storage.h"

void story_draw_tutorial(Canvas *canvas, GameManager *manager)
{
    GameContext *game_context = game_manager_game_context_get(manager);
    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 45, 12, "Tutorial");
    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    switch (game_context->story_step)
    {
    case 0:
        canvas_draw_str(canvas, 15, 20, "Press LEFT to move left");
        break;
    case 1:
        canvas_draw_str(canvas, 15, 20, "Press RIGHT to move right");
        break;
    case 2:
        canvas_draw_str(canvas, 15, 20, "Press UP to move up");
        break;
    case 3:
        canvas_draw_str(canvas, 15, 20, "Press DOWN to move down");
        break;
    case 4:
        canvas_draw_str(canvas, 0, 20, "Press OK + collide with an enemy to attack");
        break;
    case 5:
        canvas_draw_str(canvas, 15, 20, "Hold OK to open the menu");
        break;
    case 6:
        canvas_draw_str(canvas, 15, 20, "Press BACK to escape the menu");
        break;
    case 7:
        canvas_draw_str(canvas, 15, 20, "Hold BACK to save and exit");
        break;
    case 8:
        save_char("tutorial_done", "J You BLANKED on this one");
        game_context->story_step++;
        save_uint32("story_step", game_context->story_step);
        return;
    default:
        break;
    }
}

void story_draw(Entity *player, Canvas *canvas, GameManager *manager)
{
    if (!player || !canvas || !manager)
        return;

    // Get game context
    GameContext *game_context = game_manager_game_context_get(manager);
    if (!game_context)
        return;

    // Get player context
    PlayerContext *player_context = entity_context_get(player);
    if (!player_context)
        return;

    // Draw the tutorial or game world
    if (game_context->story_step < STORY_TUTORIAL_STEPS)
    {
        story_draw_tutorial(canvas, manager);
    }
    else
    {
        draw_background_render(canvas, manager);
    }
}
