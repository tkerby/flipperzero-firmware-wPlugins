#include "../twentyq_app_i.h"

static void twentyq_scene_error_callback(void* context) {
    TwentyQApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, 0);
}

void twentyq_scene_error_on_enter(void* context) {
    TwentyQApp* app = context;
    Popup* popup = app->popup;

    popup_reset(popup);
    popup_set_header(
        popup,
        app->message_header[0] ? app->message_header : "Sorry",
        64,
        10,
        AlignCenter,
        AlignCenter);
    popup_set_text(popup, app->message, 64, 34, AlignCenter, AlignCenter);
    popup_set_callback(popup, twentyq_scene_error_callback);
    popup_set_context(popup, app);
    popup_set_timeout(popup, 5000);
    popup_enable_timeout(popup);

    view_dispatcher_switch_to_view(app->view_dispatcher, TwentyQViewPopup);
}

bool twentyq_scene_error_on_event(void* context, SceneManagerEvent event) {
    TwentyQApp* app = context;
    if(event.type == SceneManagerEventTypeBack || event.type == SceneManagerEventTypeCustom) {
        if(app->dataset_ready) {
            /* recoverable: back to the menu */
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, TwentyQSceneStart);
        } else {
            /* nothing to play with: leave cleanly */
            view_dispatcher_stop(app->view_dispatcher);
        }
        return true;
    }
    return false;
}

void twentyq_scene_error_on_exit(void* context) {
    TwentyQApp* app = context;
    popup_reset(app->popup);
}
