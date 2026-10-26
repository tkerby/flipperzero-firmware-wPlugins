#include "../twentyq_app_i.h"

/* The hint only tilts the prior, it never filters (SPEC 6.2) -- people
 * genuinely disagree about whether a mushroom is a vegetable. */
static void twentyq_scene_category_callback(void* context, uint32_t index) {
    TwentyQApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void twentyq_scene_category_on_enter(void* context) {
    TwentyQApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Think of something...");
    submenu_add_item(submenu, "Animal", 0, twentyq_scene_category_callback, app);
    submenu_add_item(submenu, "Vegetable / food", 1, twentyq_scene_category_callback, app);
    submenu_add_item(submenu, "Mineral / object", 2, twentyq_scene_category_callback, app);
    submenu_add_item(submenu, "Something else", 3, twentyq_scene_category_callback, app);
    submenu_add_item(submenu, "Not sure - just ask", 4, twentyq_scene_category_callback, app);
    submenu_set_selected_item(submenu, 4);

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewSubmenu);
}

bool twentyq_scene_category_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    app->category_hint = (event.event <= 3) ? (uint8_t)event.event : TQ_NO_CATEGORY;
    app->job = TwentyQJobStartGame;
    scene_manager_next_scene(app->scene_manager, TwentyQSceneThinking);
    return true;
}

void twentyq_scene_category_on_exit(void* context) {
    TwentyQApp* app = context;
    submenu_reset(app->submenu);
}
