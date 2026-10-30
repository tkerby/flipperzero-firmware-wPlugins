#include "../debug_mode.h"

void debug_mode_scene_splash_on_enter(void* context) {
    DebugMode* app = context;
    debug_mode_splash_view_tick(app->splash_view);
    furi_timer_start(app->splash_timer, furi_ms_to_ticks(450));
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdSplash);
}

bool debug_mode_scene_splash_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        consumed = true;
        switch(event.event) {
        case DebugModeCustomEventSplashTick:
            debug_mode_splash_view_tick(app->splash_view);
            break;
        case DebugModeCustomEventSplashStart:
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, DebugModeSceneMainMenu);
            break;
        default:
            consumed = false;
            break;
        }
    }

    return consumed;
}

void debug_mode_scene_splash_on_exit(void* context) {
    DebugMode* app = context;
    if(furi_timer_is_running(app->splash_timer)) {
        furi_timer_stop(app->splash_timer);
    }
}
