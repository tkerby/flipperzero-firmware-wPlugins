/* Error scene: app->error_title and the detail in app->text (see
 * pkb_app_set_error). Back returns to the main menu. */
#include "../pkb_app.h"
#include "pkb_scene.h"

void pkb_scene_error_on_enter(void* context) {
    PkbApp* app = context;

    widget_reset(app->widget);
    widget_add_string_element(
        app->widget, 64, 2, AlignCenter, AlignTop, FontPrimary, app->error_title);
    widget_add_string_multiline_element(
        app->widget, 64, 36, AlignCenter, AlignCenter, FontSecondary, app->text);
    widget_add_string_element(
        app->widget, 64, 63, AlignCenter, AlignBottom, FontSecondary, "Back: main menu");

    view_dispatcher_switch_to_view(app->view_dispatcher, PkbViewWidget);
}

bool pkb_scene_error_on_event(void* context, SceneManagerEvent event) {
    PkbApp* app = context;

    if(event.type != SceneManagerEventTypeBack) return false;
    scene_manager_search_and_switch_to_previous_scene(app->scene_manager, PkbSceneMainMenu);
    return true;
}

void pkb_scene_error_on_exit(void* context) {
    PkbApp* app = context;
    widget_reset(app->widget);
}
