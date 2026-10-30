#pragma once

#include <furi.h>
#include <gui/gui.h>
#include <gui/view_dispatcher.h>
#include <gui/scene_manager.h>
#include <gui/modules/submenu.h>
#include <gui/modules/widget.h>
#include <gui/modules/variable_item_list.h>
#include <storage/storage.h>

#include <pkb/team.h>
#include <pkb/policy_rng.h>

#include "engine_glue.h"
#include "link_glue.h"
#include "views/battle_view.h"
#include "views/team_view.h"

typedef enum {
    PkbViewSubmenu,
    PkbViewWidget,
    PkbViewVariableItemList,
    PkbViewBattle,
    PkbViewTeam,
} PkbView;

/* The opponent the Flipper plays (Opponent menu), kept in settings.bin */
typedef enum {
    PkbOpponentRandom,
    PkbOpponentBoss,
} PkbOpponentKind;

typedef struct {
    uint8_t kind; /* PkbOpponentKind */
    uint8_t level_lo, level_hi; /* random teams */
    uint8_t boss; /* pb_bosses index */
    uint8_t boss_ace; /* 0 = the game's levels, else the ace's level */
    uint8_t music_test; /* 0 = follow the opponent, else a battle song id */
    uint8_t picture; /* 0 = default, 0xFF = link picture, else a trainer class */
} PkbSettings;

#define PKB_RANDOM_LEVEL_MIN 5 /* lowest level the Opponent menu offers */

typedef struct {
    uint32_t battles;
    bool active; /* between the team exchange and the end of battle */
    bool error;
    PkbEngineGlue engine;
    PkbBattleViewModel model;
} PkbBattleState;

typedef struct {
    Gui* gui;
    Storage* storage;
    ViewDispatcher* view_dispatcher;
    SceneManager* scene_manager;

    Submenu* submenu;
    Widget* widget;
    VariableItemList* variable_item_list;
    PkbBattleView* battle_view;
    PkbTeamView* team_view;

    /* flipper-gblink handle; always external clock (the GB is the master) */
    void* gblink;

    /* Policy RNG for team generation and move choices (never the battle RNG) */
    PkbPolicyRng rng;
    PkbTeam team;
    bool have_team;
    PkbSettings settings;

    PkbLink link;
    PkbBattleState battle;
    PkbTeamViewModel team_page; /* View Team's current page */

    /* Backing store for widget text (and the error scene's detail) */
    char text[1024];
    char error_title[24];
} PkbApp;

/* Custom event a scene sends itself to open the error scene outside on_enter */
#define PKB_EVENT_ERROR 0x2000

/* Build the selected opponent's team into app->team (a fresh one for random
 * battles). On failure, sets the error (see pkb_app_set_error) and returns
 * false. */
bool pkb_app_new_team(PkbApp* app);
/* The opponent as the menus show it, e.g. "Random 75-80", "Gym: Falkner" */
void pkb_app_opponent_label(const PkbApp* app, char* out, size_t size);
/* settings.bin in the app's data folder; defaults if missing or stale */
void pkb_settings_load(PkbApp* app);
void pkb_settings_save(PkbApp* app);
/* Opponent, music, and picture back to a fresh install, and drop the cached team. */
void pkb_settings_reset(PkbApp* app);
/* Sets what the error scene shows: a title and a detail with line breaks */
void pkb_app_set_error(PkbApp* app, const char* title, const char* detail);
