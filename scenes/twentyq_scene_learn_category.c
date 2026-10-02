#include "../twentyq_app_i.h"

static void twentyq_scene_learn_category_callback(void* context, uint32_t index) {
    TwentyQApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void twentyq_scene_learn_category_on_enter(void* context) {
    TwentyQApp* app = context;
    Submenu* submenu = app->submenu;

    submenu_reset(submenu);
    submenu_set_header(submenu, "What kind of thing?");
    for(uint8_t c = 0; c < TQ_CATEGORY_COUNT; c++) {
        submenu_add_item(
            submenu, tq_category_label(c), c, twentyq_scene_learn_category_callback, app);
    }
    submenu_set_selected_item(submenu, app->learn_category);

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewSubmenu);
}

bool twentyq_scene_learn_category_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;
    if(event.type != SceneManagerEventTypeCustom) return false;

    app->learn_category = (uint8_t)event.event;
    scene_manager_next_scene(app->scene_manager, TwentyQSceneLearnConfirm);
    return true;
}

void twentyq_scene_learn_category_on_exit(void* context) {
    TwentyQApp* app = context;
    submenu_reset(app->submenu);
}
