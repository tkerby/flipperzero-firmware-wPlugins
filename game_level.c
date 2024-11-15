#include "game_level.h"

bool started = false;
int welcomeTicks = 0;
int enemiesSpawned = 0;
#define MAX_DOORS 5
struct game_door doors[MAX_DOORS];

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

    static bool completedFirstTask = false;
    if(kills == 3) {
        if(!completedFirstTask) {
            completedFirstTask = true;
            doors[0] = (struct game_door){58, 24, 16, 32, true, 0, "Salutations!"};
        }
    }
    //Door processing
    InputState input = game_manager_input_get(manager);
    for(int i = 0; i < MAX_DOORS; i++) {
        struct game_door* door = &doors[i];
        if(!door->visible) continue;
        if(input.pressed & GameKeyUp && fabsf(pos->x - door->x) < 16 &&
           fabsf(pos->y - door->y) < 32) {
            door->visible = false;
            door->transitionTicks = furi_get_tick();
            //Went in door!
        }
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

    for(int i = 0; i < MAX_DOORS; i++) {
        struct game_door door = doors[i];
        if(!door.visible) {
            if((furi_get_tick() - door.transitionTicks) < 3000 && door.transitionText != NULL) {
                canvas_draw_str_aligned(
                    canvas, door.x, door.y, AlignCenter, AlignCenter, door.transitionText);
            }
            continue;
        };
        canvas_draw_frame(canvas, door.x, door.y, door.width, door.height);
    }
}

void game_level_enemy_render(GameManager* manager, Canvas* canvas, void* context) {
    UNUSED(manager);
    UNUSED(canvas);
    UNUSED(context);
}
