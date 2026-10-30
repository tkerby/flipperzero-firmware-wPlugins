#include "../twentyq_app_i.h"

typedef enum {
    SubmenuIndexPlay,
    SubmenuIndexHowItWorks,
    SubmenuIndexStats,
    SubmenuIndexSettings,
} SubmenuIndex;

static void twentyq_scene_start_callback(void* context, uint32_t index) {
    TwentyQApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void twentyq_scene_start_on_enter(void* context) {
    TwentyQApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "20 Questions");
    submenu_add_item(submenu, "Play", SubmenuIndexPlay, twentyq_scene_start_callback, app);
    submenu_add_item(
        submenu, "How it works", SubmenuIndexHowItWorks, twentyq_scene_start_callback, app);
    submenu_add_item(submenu, "Stats", SubmenuIndexStats, twentyq_scene_start_callback, app);
    submenu_add_item(submenu, "Settings", SubmenuIndexSettings, twentyq_scene_start_callback, app);
    submenu_set_selected_item(
        submenu, scene_manager_get_scene_state(app->scene_manager, TwentyQSceneStart));

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewSubmenu);
}

bool twentyq_scene_start_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    scene_manager_set_scene_state(app->scene_manager, TwentyQSceneStart, event.event);
    switch(event.event) {
    case SubmenuIndexPlay:
        app->category_hint = TQ_NO_CATEGORY;
        app->guess_number = 0;
        app->game_won = false;
        if(app->settings.ask_category) {
            scene_manager_next_scene(app->scene_manager, TwentyQSceneCategory);
        } else {
            app->job = TwentyQJobStartGame;
            scene_manager_next_scene(app->scene_manager, TwentyQSceneThinking);
        }
        return true;
    case SubmenuIndexHowItWorks:
        scene_manager_next_scene(app->scene_manager, TwentyQSceneAbout);
        return true;
    case SubmenuIndexStats:
        scene_manager_next_scene(app->scene_manager, TwentyQSceneStats);
        return true;
    case SubmenuIndexSettings:
        scene_manager_next_scene(app->scene_manager, TwentyQSceneSettings);
        return true;
    default:
        return false;
    }
}

void twentyq_scene_start_on_exit(void* context) {
    TwentyQApp* app = context;
    submenu_reset(app->submenu);
}
