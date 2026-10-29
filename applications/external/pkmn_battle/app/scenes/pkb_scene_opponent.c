/* Opponent: Random Battle (pick a level range), a gym leader, an Elite Four
 * member, Champion Lance, the final rival or Red (exact Crystal teams). */
#include "../pkb_app.h"
#include "pkb_scene.h"

typedef enum {
    OpponentRandom,
    OpponentGym,
    OpponentE4,
    OpponentRival,
    OpponentRed,
    OpponentChampion, /* after Red so Gym/E4/Rival still map onto those categories */
    OpponentOak,
} OpponentIndex;

static void opponent_callback(void* context, uint32_t index) {
    PkbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void pkb_scene_opponent_on_enter(void* context) {
    PkbApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Opponent");
    submenu_add_item(submenu, "Random Battle", OpponentRandom, opponent_callback, app);
    submenu_add_item(submenu, "Gym Leader", OpponentGym, opponent_callback, app);
    submenu_add_item(submenu, "Elite Four", OpponentE4, opponent_callback, app);
    submenu_add_item(submenu, "Champion Lance", OpponentChampion, opponent_callback, app);
    submenu_add_item(submenu, "Rival", OpponentRival, opponent_callback, app);
    submenu_add_item(submenu, "Red", OpponentRed, opponent_callback, app);
    submenu_add_item(submenu, "Prof. Oak", OpponentOak, opponent_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, PkbSceneOpponent));
    view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewSubmenu);
}

bool pkb_scene_opponent_on_event(void* context, SceneManagerEvent event) {
    PkbApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, PkbSceneOpponent, event.event);
    switch(event.event) {
    case OpponentRandom:
        app->settings.kind = PkbOpponentRandom;
        app->have_team = false;
        scene_manager_next_scene(app->scene_manager, PkbSceneLevels);
        return true;
    case OpponentGym:
    case OpponentE4:
    case OpponentRival:
        /* the boss list shows this category */
        scene_manager_set_scene_state(
            app->scene_manager,
            PkbSceneBossList,
            (uint32_t)(event.event - OpponentGym + PkbBossGym) << 8);
        scene_manager_next_scene(app->scene_manager, PkbSceneBossList);
        return true;
    case OpponentChampion:
    case OpponentRed:
    case OpponentOak: {
        PkbBossCategory want = event.event == OpponentChampion ? PkbBossChampion :
                               event.event == OpponentRed      ? PkbBossRed :
                                                                 PkbBossOak;
        for(uint8_t i = 0; i < pkb_boss_count(); i++) {
            if(pkb_boss_category(i) == want) app->settings.boss = i;
        }
    }
        app->settings.kind = PkbOpponentBoss;
        app->have_team = false;
        scene_manager_next_scene(app->scene_manager, PkbSceneLevels);
        return true;
    default:
        return false;
    }
}

void pkb_scene_opponent_on_exit(void* context) {
    PkbApp* app = context;
    submenu_reset(app->submenu);
}
