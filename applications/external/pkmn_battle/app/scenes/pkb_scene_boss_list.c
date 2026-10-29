/* The bosses of one category (gym leaders, Elite Four, the final rival by
 * its starter). Scene state: category << 8 | selected item. */
#include "../pkb_app.h"
#include "pkb_scene.h"

static void boss_list_callback(void* context, uint32_t index) {
    PkbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void pkb_scene_boss_list_on_enter(void* context) {
    PkbApp* app = context;
    Submenu* submenu = app->submenu;
    uint32_t state = scene_manager_get_scene_state(app->scene_manager, PkbSceneBossList);
    uint8_t category = (uint8_t)(state >> 8);
    static const char* const headers[PkbBossCategories] = {
        "Gym Leader", "Elite Four", "Rival's starter", "Red", "Champion Lance", "Prof. Oak"};

    submenu_reset(submenu);
    submenu_set_header(submenu, headers[category]);
    for(uint8_t i = 0; i < pkb_boss_count(); i++) {
        if(pkb_boss_category(i) == category)
            submenu_add_item(submenu, pkb_boss_label(i), i, boss_list_callback, app);
    }
    /* the current boss if it is in this list */
    if(app->settings.kind == PkbOpponentBoss &&
       pkb_boss_category(app->settings.boss) == category) {
        submenu_set_selected_item(submenu, app->settings.boss);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewSubmenu);
}

bool pkb_scene_boss_list_on_event(void* context, SceneManagerEvent event) {
    PkbApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event >= pkb_boss_count()) return false;
    if(app->settings.kind != PkbOpponentBoss || app->settings.boss != event.event)
        app->have_team = false;
    app->settings.kind = PkbOpponentBoss;
    app->settings.boss = (uint8_t)event.event;
    scene_manager_next_scene(app->scene_manager, PkbSceneLevels);
    return true;
}

void pkb_scene_boss_list_on_exit(void* context) {
    PkbApp* app = context;
    submenu_reset(app->submenu);
}
