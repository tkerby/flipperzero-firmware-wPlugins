/*
 * Start Battle: run the link session with the battle engine behind it.
 * The first battle uses the team View Team shows; random teams are redrawn
 * before each rematch, boss teams stay the same.
 *
 * The engine (app/engine_glue.c) runs DoBattle on its own thread as soon as
 * the teams are exchanged; the session answers the Game Boy from the ISR.
 */
#include <furi_hal_random.h>
#include <pkb/engine.h>
#include <pkb/music_test.h>
#include <pkb/picture.h>
#include <pkb/party_block.h>

#include "../pkb_app.h"
#include "pkb_scene.h"

static uint8_t tx_rn[PKB_RN_BLOCK_LEN];
static uint8_t tx_party[PKB_PARTY_BLOCK_LEN];
static uint8_t tx_patch[PKB_PATCH_BLOCK_LEN];

/* Stop here: the error scene opens on the next event (never from on_enter
 * directly). */
static void battle_fail(PkbApp* app, const char* title, const char* detail) {
    if(app->battle.error) return;
    if(title) pkb_app_set_error(app, title, detail);
    app->battle.error = true;
    view_dispatcher_send_custom_event(app->view_dispatcher, PKB_EVENT_ERROR);
}

static bool battle_prepare_team(PkbApp* app, bool fresh) {
    uint8_t rns[PKB_RNS_LEN];

    if((fresh || !app->have_team) && !pkb_app_new_team(app)) {
        battle_fail(app, NULL, NULL); /* the error is set */
        return false;
    }
    furi_hal_random_fill_buf(rns, sizeof(rns));
    pkb_rn_block_build(rns, tx_rn);
    if(!pkb_party_block_build(&app->team, app->team.name, tx_party, tx_patch)) {
        battle_fail(app, "Internal error", "The team does not fit\nthe patch list.");
        return false;
    }
    bool boss = app->settings.kind == PkbOpponentBoss;
    uint8_t picture =
        pkb_picture_resolve(app->settings.picture, boss, app->settings.boss, &app->rng);
    uint8_t song = pkb_music_resolve(app->settings.music_test, boss, app->settings.boss);
    /* Default music follows the picture. A chosen song still wins. */
    if(app->settings.music_test == 0 && picture) song = pkb_music_for_class(picture);
    if(!pkb_link_overlay_apply(tx_party, picture, song)) {
        battle_fail(app, "Internal error", "The picture or music\ndid not fit the party.");
        return false;
    }
    pbl_set_tx(&app->link.session, tx_rn, tx_party, tx_patch);
    return true;
}

/* The two lines shown before the first battle */
static void waiting_text(const PkbApp* app, PblPhase phase, PkbBattleViewModel* m) {
    const char* status = "";
    const char* hint = "";
    switch(phase) {
    case PblPhaseConn: {
        /* who the Game Boy will face */
        int n = snprintf(m->status, sizeof(m->status), "Opponent: ");
        pkb_app_opponent_label(app, m->status + n, sizeof(m->status) - n);
        snprintf(m->hint, sizeof(m->hint), "Talk to Cable Club desk");
        return;
    }
    case PblPhaseRecept:
        status = "Linked!";
        hint = "Save, then enter the room";
        break;
    case PblPhaseRoom:
        status = "In the Colosseum";
        hint = "Use the console to battle";
        break;
    case PblPhaseConsole:
    case PblPhaseXbRn:
    case PblPhaseXbParty:
    case PblPhaseXbPatch:
        status = "Exchanging teams...";
        hint = "Keep the cable still";
        break;
    default:
        break;
    }
    snprintf(m->status, sizeof(m->status), "%s", status);
    snprintf(m->hint, sizeof(m->hint), "%s", hint);
}

static void side_view(const PkbEngine* E, bool enemy, PkbBattleViewSide* out) {
    PkbSideView v;
    pkb_engine_side_view(E, enemy, &v);
    if(v.species)
        pkb_species_name(v.species, out->name, sizeof(out->name));
    else
        snprintf(out->name, sizeof(out->name), "-");
    snprintf(out->status, sizeof(out->status), "%s", pkb_status_abbrev(v.status));
    out->hp = v.hp;
    out->max_hp = v.max_hp;
    out->count = v.count;
    out->alive = v.alive;
}

static void battle_finish(PkbApp* app, PkbBattleResult r) {
    PkbBattleState* b = &app->battle;
    PkbBattleViewModel* m = &b->model;
    static const char* const text[] = {
        "Battle over", "Flipper won!", "Flipper lost", "Draw", "Aborted"};

    /* Snapshot the screen before freeing the engine. */
    if(b->engine.E) {
        side_view(b->engine.E, false, &m->me);
        side_view(b->engine.E, true, &m->gb);
        m->desync = (b->engine.E->flags & PKB_ENGINE_UNSUPPORTED) ? "unported" :
                    (b->engine.E->flags & PKB_ENGINE_GAME_DESYNC) ? "game bug" :
                                                                    NULL;
    }
    m->turns = pbl_battle_exchanges_done(&app->link.session);
    pkb_engine_glue_free(&b->engine);

    snprintf(m->event, sizeof(m->event), "%s", text[r]);
    b->active = false;
    b->battles++;
    m->show_mons = true;
    m->battle_running = false;
    m->battles = b->battles - 1;
}

static void battle_tick(PkbApp* app) {
    PblSession* s = &app->link.session;
    PkbBattleState* b = &app->battle;
    PkbBattleViewModel* m = &b->model;
    uint32_t ev = pkb_link_take_events(&app->link);

    if(b->error) return;
    if(b->active) {
        if(pkb_engine_glue_finished(&b->engine)) {
            battle_finish(app, b->engine.result);
            /* rematch: a fresh random team, the same boss team */
            if(!battle_prepare_team(app, app->settings.kind == PkbOpponentRandom)) return;
        }
    }
    if(ev & PBL_EV_BLOCKS_DONE) {
        b->active = true;
        uint64_t seed = ((uint64_t)furi_hal_random_get() << 32) | furi_hal_random_get();
        if(!pkb_engine_glue_start(&b->engine, s, tx_party, tx_patch, seed)) {
            battle_fail(
                app,
                "Link error",
                "The Game Boy's team did\nnot arrive intact.\nReset both and retry.");
            return;
        }
    }
    if(ev & PBL_EV_NEED_TX) {
        battle_fail(app, "Link error", "Our team was not ready\nin time. Reset both\nand retry.");
        return;
    }

    PblPhase phase = atomic_load(&s->phase);
    snprintf(m->phase, sizeof(m->phase), "%s", pbl_phase_name(phase));
    waiting_text(app, phase, m);
    /* read while the engine thread runs: display only, a torn value is harmless.
     * After the fight the engine has been freed and the snapshot above stays. */
    const PkbEngine* E = b->engine.E;
    if(E) {
        m->show_mons = b->active || b->battles;
        if(m->show_mons) {
            side_view(E, false, &m->me);
            side_view(E, true, &m->gb);
            if(b->active) {
                PkbSideView me;
                char move[16];
                pkb_engine_side_view(E, false, &me);
                if(me.last_move) {
                    pkb_move_name(me.last_move, move, sizeof(move));
                    snprintf(m->event, sizeof(m->event), "Flipper used %s", move);
                } else {
                    snprintf(m->event, sizeof(m->event), "Battling");
                }
            }
            m->desync = (E->flags & PKB_ENGINE_UNSUPPORTED) ? "unported" :
                        (E->flags & PKB_ENGINE_GAME_DESYNC) ? "game bug" :
                                                              NULL;
        } else {
            m->desync = NULL;
        }
    }
    m->battle_running = b->active;
    m->rematch = !b->active && b->battles && phase == PblPhaseRoom;
    m->bytes = s->bytes;
    if(b->active) m->turns = pbl_battle_exchanges_done(s);
    /* the battle shown: the running one, or the last one */
    m->battles = b->battles - (!b->active && b->battles ? 1 : 0);
    pkb_battle_view_update(app->battle_view, m);
}

void pkb_scene_battle_on_enter(void* context) {
    PkbApp* app = context;
    PkbBattleState* b = &app->battle;

    memset(b, 0, sizeof(*b));
    memset(&app->link.session, 0, sizeof(app->link.session));
    app->link.session.auto_action = PBL_ACTION_NONE;
    pkb_battle_view_reset(app->battle_view);

    if(battle_prepare_team(app, false)) pkb_link_start(&app->link, app->gblink);
    battle_tick(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewBattle);
}

bool pkb_scene_battle_on_event(void* context, SceneManagerEvent event) {
    PkbApp* app = context;

    if(event.type == SceneManagerEventTypeTick) {
        battle_tick(app);
        return true;
    }
    if(event.type != SceneManagerEventTypeCustom) return false;
    if(event.event == PKB_BATTLE_VIEW_EVENT_ABORT) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    if(event.event == PKB_EVENT_ERROR) {
        scene_manager_next_scene(app->scene_manager, PkbSceneError);
        return true;
    }
    return false;
}

void pkb_scene_battle_on_exit(void* context) {
    PkbApp* app = context;
    PkbBattleState* b = &app->battle;

    pkb_engine_glue_stop(&b->engine);
    pkb_link_stop(&app->link);
    b->active = false;
    pkb_engine_glue_free(&b->engine);
    /* a random team was used; View Team shows the next one */
    if(app->settings.kind == PkbOpponentRandom && b->battles) app->have_team = false;
}
