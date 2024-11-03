#include "game_level.h"

bool started = false;
int welcomeTicks = 0;
int enemiesSpawned = 0;

void game_level_player_update(Entity* self, GameManager* manager, void* context, Vector* pos) {
    if(!game_menu_game_selected) return;
    PlayerContext* playerCtx = context;
    UNUSED(playerCtx);
    UNUSED(manager);
    UNUSED(pos);
    UNUSED(self);

    static bool hasProgressed = false;
    if(kills == 1 && !hasProgressed) {
        hasProgressed = true;
        enemy_spawn(gameLevel, manager, (Vector){120, 0}, 3000, false);
        furi_delay_ms(200);
        enemy_spawn(gameLevel, manager, (Vector){10, 0}, 3000, true);
    }
}

void game_level_player_render(GameManager* manager, Canvas* canvas, void* context) {
    if(!game_menu_game_selected) return;
    UNUSED(manager);
    PlayerContext* playerCtx = context;
    UNUSED(playerCtx);

    if(!started) {
        started = true;
        welcomeTicks = furi_get_tick();
        enemy_spawn(gameLevel, manager, (Vector){120, 0}, 1000, false);
        enemiesSpawned++;
    }

    if(started && furi_get_tick() - welcomeTicks < 6000 &&
       ((furi_get_tick() - welcomeTicks) / 1000) % 2 == 0) {
        canvas_printf(canvas, 15, 30, "Welcome to DEADZONE!");
    }
}

void game_level_enemy_render(GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(manager);
    UNUSED(canvas);
    UNUSED(context);
}
