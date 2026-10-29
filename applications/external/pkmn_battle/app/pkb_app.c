#include "pkb_app.h"

#include <pkb/engine.h>
#include <pkb/music_test.h>
#include <pkb/picture.h>
#include "scenes/pkb_scene.h"
#include "storage_glue.h"

#include <furi_hal_random.h>

#include <gblink.h>
#include <gblink/include/gblink_pinconf.h>

static bool pkb_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    PkbApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static void pkb_tick_event_callback(void* context) {
    furi_assert(context);
    PkbApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

static bool pkb_back_event_callback(void* context) {
    furi_assert(context);
    PkbApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void pkb_app_set_error(PkbApp* app, const char* title, const char* detail) {
    snprintf(app->error_title, sizeof(app->error_title), "%s", title);
    snprintf(app->text, sizeof(app->text), "%s", detail);
}

#define PKB_SETTINGS_PATH    APP_DATA_PATH("settings.bin")
#define PKB_SETTINGS_VERSION 3

static void settings_defaults(PkbSettings* s) {
    memset(s, 0, sizeof(*s));
    s->kind = PkbOpponentRandom;
    s->level_lo = s->level_hi = 50;
}

static bool settings_valid(const PkbSettings* s) {
    return s->kind <= PkbOpponentBoss && s->level_lo >= PKB_RANDOM_LEVEL_MIN &&
           s->level_lo <= s->level_hi && s->level_hi <= PKB_LEVEL_MAX &&
           s->boss < pkb_boss_count() &&
           (!s->boss_ace || (s->boss_ace >= PKB_LEVEL_MIN && s->boss_ace <= PKB_LEVEL_MAX)) &&
           pkb_music_setting_ok(s->music_test) && pkb_picture_setting_ok(s->picture);
}

void pkb_settings_load(PkbApp* app) {
    uint8_t buf[1 + sizeof(PkbSettings)];
    File* f = storage_file_alloc(app->storage);
    bool ok = storage_file_open(f, PKB_SETTINGS_PATH, FSAM_READ, FSOM_OPEN_EXISTING) &&
              storage_file_read(f, buf, sizeof(buf)) == sizeof(buf) &&
              buf[0] == PKB_SETTINGS_VERSION;
    storage_file_close(f);
    storage_file_free(f);
    if(ok) {
        memcpy(&app->settings, buf + 1, sizeof(PkbSettings));
        /* the first music build stored 3 for "Champion" */
        if(app->settings.music_test == PKB_MUSIC_CHAMPION)
            app->settings.music_test = PKB_SONG_CHAMPION;
    }
    if(!ok || !settings_valid(&app->settings)) settings_defaults(&app->settings);
}

void pkb_settings_reset(PkbApp* app) {
    settings_defaults(&app->settings);
    app->have_team = false;
    pkb_settings_save(app);
}

void pkb_settings_save(PkbApp* app) {
    uint8_t buf[1 + sizeof(PkbSettings)];
    buf[0] = PKB_SETTINGS_VERSION;
    memcpy(buf + 1, &app->settings, sizeof(PkbSettings));
    File* f = storage_file_alloc(app->storage);
    if(storage_file_open(f, PKB_SETTINGS_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        storage_file_write(f, buf, sizeof(buf));
    }
    storage_file_close(f);
    storage_file_free(f);
}

void pkb_app_opponent_label(const PkbApp* app, char* out, size_t size) {
    const PkbSettings* s = &app->settings;
    if(s->kind == PkbOpponentRandom) {
        if(s->level_lo == s->level_hi)
            snprintf(out, size, "Random Lv%u", s->level_lo);
        else
            snprintf(out, size, "Random %u-%u", s->level_lo, s->level_hi);
        return;
    }
    /* gym, Elite Four, rival, Red, champion. Labels are unique; the rival's
     * is his starter, and Lance's menu name is Champion Lance. */
    static const char* const prefix[PkbBossCategories] = {"", "", "Rival ", "", "Champion ", ""};
    int n =
        snprintf(out, size, "%s%s", prefix[pkb_boss_category(s->boss)], pkb_boss_label(s->boss));
    if(s->boss_ace && n > 0 && (size_t)n < size) snprintf(out + n, size - n, " Lv%u", s->boss_ace);
}

bool pkb_app_new_team(PkbApp* app) {
    PkbSdBlob blob;
    PkbBlobReader pools;

    if(app->settings.kind == PkbOpponentBoss) {
        pkb_boss_team_build(
            &app->team,
            app->settings.boss,
            app->settings.boss_ace,
            (uint16_t)pkb_policy_rng_next(&app->rng));
        app->have_team = true;
        return true;
    }
    if(!pkb_sd_blob_open(&blob, app->storage, PKB_MOVEPOOLS_PATH, &pools)) {
        pkb_app_set_error(
            app, "Missing data file", "movepools.bin was not found.\nReinstall the app.");
        return false;
    }
    PkbTeamResult res = PkbTeamErrIo;
    if(pkb_movepool_check(&pools)) {
        res = pkb_team_generate(
            &app->team,
            PKB_PARTY_LENGTH,
            app->settings.level_lo,
            app->settings.level_hi,
            &pools,
            pkb_engine_move_filter,
            NULL,
            &app->rng);
    }
    pkb_sd_blob_close(&blob);

    app->have_team = res == PkbTeamOk;
    if(res == PkbTeamErrIo) {
        pkb_app_set_error(app, "Bad data file", "movepools.bin is damaged.\nReinstall the app.");
    } else if(!app->have_team) {
        pkb_app_set_error(app, "No team", "Not enough species\nwith 4 usable moves.");
    }
    return app->have_team;
}

static PkbApp* pkb_app_alloc(void) {
    PkbApp* app = malloc(sizeof(PkbApp));
    memset(app, 0, sizeof(PkbApp));

    uint64_t seed = ((uint64_t)furi_hal_random_get() << 32) | furi_hal_random_get();
    pkb_policy_rng_seed(&app->rng, seed);

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, pkb_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, pkb_back_event_callback);
    view_dispatcher_set_tick_event_callback(app->view_dispatcher, pkb_tick_event_callback, 100);
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher, PkbViewSubmenu, submenu_get_view(app->submenu));

    app->widget = widget_alloc();
    view_dispatcher_add_view(app->view_dispatcher, PkbViewWidget, widget_get_view(app->widget));

    app->variable_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        PkbViewVariableItemList,
        variable_item_list_get_view(app->variable_item_list));

    app->battle_view = pkb_battle_view_alloc(app->view_dispatcher);
    view_dispatcher_add_view(
        app->view_dispatcher, PkbViewBattle, pkb_battle_view_get_view(app->battle_view));

    app->team_view = pkb_team_view_alloc(app->view_dispatcher);
    view_dispatcher_add_view(
        app->view_dispatcher, PkbViewTeam, pkb_team_view_get_view(app->team_view));

    app->gblink = gblink_alloc();
    gblink_pinconf_load(app->gblink);

    pkb_settings_load(app);
    app->scene_manager = scene_manager_alloc(&pkb_scene_handlers, app);
    scene_manager_next_scene(app->scene_manager, PkbSceneMainMenu);

    return app;
}

static void pkb_app_free(PkbApp* app) {
    furi_assert(app);

    gblink_free(app->gblink);

    view_dispatcher_remove_view(app->view_dispatcher, PkbViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, PkbViewWidget);
    widget_free(app->widget);
    view_dispatcher_remove_view(app->view_dispatcher, PkbViewVariableItemList);
    variable_item_list_free(app->variable_item_list);
    view_dispatcher_remove_view(app->view_dispatcher, PkbViewBattle);
    pkb_battle_view_free(app->battle_view);
    view_dispatcher_remove_view(app->view_dispatcher, PkbViewTeam);
    pkb_team_view_free(app->team_view);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t pkb_app_main(void* p) {
    UNUSED(p);
    PkbApp* app = pkb_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    pkb_app_free(app);
    return 0;
}
