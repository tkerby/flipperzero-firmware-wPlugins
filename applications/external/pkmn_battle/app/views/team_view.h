#pragma once

#include <gui/view.h>
#include <gui/view_dispatcher.h>
#include <stdint.h>

#include <pkb/team.h>

typedef struct PkbTeamView PkbTeamView;

/* 56x56 XBM, as stored in files/all_sprites.fxbm */
#define PKB_SPRITE_SIZE  56
#define PKB_SPRITE_BYTES (PKB_SPRITE_SIZE / 8 * PKB_SPRITE_SIZE)

/* One team mon per page */
typedef struct {
    uint8_t page, count;
    char name[11];
    uint8_t level;
    uint16_t max_hp;
    char item[13]; /* held item, "" if none */
    char moves[PKB_NUM_MOVES][13];
    bool can_reroll; /* random teams: OK draws a new one */
    bool has_sprite;
    uint8_t sprite[PKB_SPRITE_BYTES];
} PkbTeamViewModel;

/* Custom events the view sends: Left, Right, OK */
#define PKB_TEAM_VIEW_EVENT_PREV   0x1100
#define PKB_TEAM_VIEW_EVENT_NEXT   0x1101
#define PKB_TEAM_VIEW_EVENT_REROLL 0x1102

PkbTeamView* pkb_team_view_alloc(ViewDispatcher* dispatcher);
void pkb_team_view_free(PkbTeamView* view);
View* pkb_team_view_get_view(PkbTeamView* view);
/* Copies *model into the view and redraws. */
void pkb_team_view_update(PkbTeamView* view, const PkbTeamViewModel* model);
