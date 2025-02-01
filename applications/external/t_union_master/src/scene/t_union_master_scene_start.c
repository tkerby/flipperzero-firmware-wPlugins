#include "../t_union_master_i.h"

enum SubmenuIndex {
    SubmenuIndexReadCard,
    SubmenuIndexHistory,
    SubmenuIndexSetting,
    SubmenuIndexAbout,
    SubmenuIndexTest,
};

static void tum_scene_start_submenu_callback(void* ctx, uint32_t index) {
    TUM_App* app = ctx;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
    scene_manager_set_scene_state(app->scene_manager, TUM_SceneStart, index);
}

void tum_scene_start_on_enter(void* ctx) {
    TUM_App* app = ctx;
    Submenu* submenu = app->submenu;

    submenu_set_header(submenu, "T-Union Master");
    submenu_add_item(
        submenu, "贴卡查询", SubmenuIndexReadCard, tum_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "历史记录", SubmenuIndexHistory, tum_scene_start_submenu_callback, app);
    submenu_add_item(
        submenu, "应用设置", SubmenuIndexSetting, tum_scene_start_submenu_callback, app);
    submenu_add_item(submenu, "关于", SubmenuIndexAbout, tum_scene_start_submenu_callback, app);
    submenu_add_item(submenu, "测试Test", SubmenuIndexTest, tum_scene_start_submenu_callback, app);

    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, TUM_SceneStart));
    view_dispatcher_switch_to_view(app->view_dispatcher, TUM_ViewMenu);
}

bool tum_scene_start_on_event(void* ctx, SceneManagerEvent event) {
    TUM_App* app = ctx;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case SubmenuIndexReadCard:
            scene_manager_next_scene(app->scene_manager, TUM_SceneRead);
            break;
        case SubmenuIndexAbout:
            scene_manager_next_scene(app->scene_manager, TUM_SceneAbout);
            break;
        case SubmenuIndexTest:
            scene_manager_next_scene(app->scene_manager, TUM_SceneTest);
            break;
        }
    }

    return consumed;
}

void tum_scene_start_on_exit(void* ctx) {
    TUM_App* app = ctx;

    submenu_reset(app->submenu);
}
