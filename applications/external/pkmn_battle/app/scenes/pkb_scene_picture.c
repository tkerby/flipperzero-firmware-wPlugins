/* Trainer picture: Default follows the opponent, Link keeps Cal, or a class is forced. */
#include <pkb/picture.h>

#include "../pkb_app.h"
#include "pkb_scene.h"

static void picture_callback(void* context, uint32_t index) {
    PkbApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void pkb_scene_picture_on_enter(void* context) {
    PkbApp* app = context;
    Submenu* submenu = app->submenu;
    uint8_t selected = 0;

    submenu_reset(submenu);
    submenu_set_header(submenu, "Trainer Picture");
    for(uint8_t i = 0; i < pkb_picture_choice_count(); i++) {
        submenu_add_item(submenu, pkb_picture_choice_label(i), i, picture_callback, app);
        if(pkb_picture_choice_id(i) == app->settings.picture) selected = i;
    }
    submenu_set_selected_item(submenu, selected);
    view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewSubmenu);
}

bool pkb_scene_picture_on_event(void* context, SceneManagerEvent event) {
    PkbApp* app = context;
    if(event.type != SceneManagerEventTypeCustom || event.event >= pkb_picture_choice_count())
        return false;
    app->settings.picture = pkb_picture_choice_id((uint8_t)event.event);
    pkb_settings_save(app);
    scene_manager_previous_scene(app->scene_manager);
    return true;
}

void pkb_scene_picture_on_exit(void* context) {
    PkbApp* app = context;
    submenu_reset(app->submenu);
}
