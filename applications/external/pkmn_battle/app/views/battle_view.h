#pragma once

#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <stdint.h>

typedef struct PkbBattleView PkbBattleView;

/* One side's active mon */
typedef struct {
    char name[11];
    char status[4]; /* "PSN"..., "" if none */
    uint16_t hp, max_hp;
    uint8_t count; /* party size */
    uint8_t alive; /* bit i: party mon i can still fight */
} PkbBattleViewSide;

typedef struct {
    char phase[12];
    /* before the first battle: what to do next (two lines) */
    char status[40];
    char hint[32];
    /* from the first battle on: both active mons and an event line (the
     * Flipper's last move, or the result) */
    bool show_mons;
    PkbBattleViewSide me, gb;
    char event[32];
    const char* desync; /* why the GB may desync (a static string), NULL if not */
    bool battle_running; /* the quit prompt warns that the GB will hang */
    bool rematch; /* between battles: the console starts the next one */
    uint32_t bytes;
    uint32_t turns;
    uint32_t battles;
    /* owned by the view: the quit prompt is open */
    bool confirm_quit;
} PkbBattleViewModel;

/* Custom event sent when the user confirms quitting the session. */
#define PKB_BATTLE_VIEW_EVENT_ABORT 0x1000

PkbBattleView* pkb_battle_view_alloc(ViewDispatcher* dispatcher);
void pkb_battle_view_free(PkbBattleView* view);
View* pkb_battle_view_get_view(PkbBattleView* view);
/* Clears the model, including the quit prompt (on entering the scene). */
void pkb_battle_view_reset(PkbBattleView* view);
/* Copies *model into the view (keeping the quit prompt state) and redraws. */
void pkb_battle_view_update(PkbBattleView* view, const PkbBattleViewModel* model);
