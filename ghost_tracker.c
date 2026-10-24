#include "ghost_tracker_i.h"

static void ghost_tracker_menu_select_callback(void* context, uint32_t index) {
    GhostTrackerApp* app = context;
    /* MenuView's item index is 0-based (EMF=0, Spirit=1, REM=2, About=3),
     * but GhostTrackerViewMenu occupies slot 0 in GhostTrackerViewId, so
     * every view ID is shifted up by one relative to the menu's index. */
    GhostTrackerViewId view_id = (GhostTrackerViewId)(index + 1);
    app->current_view = view_id;
    view_dispatcher_switch_to_view(app->view_dispatcher, view_id);
}

static void ghost_tracker_splash_done_callback(void* context) {
    GhostTrackerApp* app = context;
    /* Called from either the GUI thread (key press) or the timer service
     * thread (timeout) - send_custom_event is safe from both; the actual
     * view switch happens on the dispatcher's own thread below. */
    view_dispatcher_send_custom_event(app->view_dispatcher, GhostTrackerCustomEventSplashDone);
}

static bool ghost_tracker_custom_event_callback(void* context, uint32_t event) {
    GhostTrackerApp* app = context;
    if(event == GhostTrackerCustomEventSplashDone && app->current_view == GhostTrackerViewSplash) {
        app->current_view = GhostTrackerViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, GhostTrackerViewMenu);
    }
    return true;
}

static bool ghost_tracker_navigation_callback(void* context) {
    GhostTrackerApp* app = context;

    if(app->current_view == GhostTrackerViewMenu) {
        /* Already at the top-level menu: back exits the app. */
        view_dispatcher_stop(app->view_dispatcher);
    } else if(app->current_view == GhostTrackerViewSplash) {
        /* Back during the splash just skips it, same as any other key. */
        ghost_tracker_splash_done_callback(app);
    } else {
        app->current_view = GhostTrackerViewMenu;
        view_dispatcher_switch_to_view(app->view_dispatcher, GhostTrackerViewMenu);
    }
    return true;
}

static GhostTrackerApp* ghost_tracker_app_alloc(void) {
    GhostTrackerApp* app = malloc(sizeof(GhostTrackerApp));

    app->gui = furi_record_open(RECORD_GUI);

    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ghost_tracker_navigation_callback);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, ghost_tracker_custom_event_callback);

    app->splash_view = splash_view_alloc();
    splash_view_set_done_callback(app->splash_view, ghost_tracker_splash_done_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTrackerViewSplash, splash_view_get_view(app->splash_view));

    app->menu_view = menu_view_alloc();
    menu_view_set_select_callback(app->menu_view, ghost_tracker_menu_select_callback, app);
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTrackerViewMenu, menu_view_get_view(app->menu_view));

    app->emf_view = emf_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTrackerViewEmf, emf_view_get_view(app->emf_view));

    app->spirit_view = spirit_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTrackerViewSpirit, spirit_view_get_view(app->spirit_view));

    app->rem_view = rem_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTrackerViewRem, rem_view_get_view(app->rem_view));

    app->about_view = about_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, GhostTrackerViewAbout, about_view_get_view(app->about_view));

    app->current_view = GhostTrackerViewSplash;

    return app;
}

static void ghost_tracker_app_free(GhostTrackerApp* app) {
    view_dispatcher_remove_view(app->view_dispatcher, GhostTrackerViewAbout);
    about_view_free(app->about_view);

    view_dispatcher_remove_view(app->view_dispatcher, GhostTrackerViewSplash);
    splash_view_free(app->splash_view);

    view_dispatcher_remove_view(app->view_dispatcher, GhostTrackerViewRem);
    rem_view_free(app->rem_view);

    view_dispatcher_remove_view(app->view_dispatcher, GhostTrackerViewSpirit);
    spirit_view_free(app->spirit_view);

    view_dispatcher_remove_view(app->view_dispatcher, GhostTrackerViewEmf);
    emf_view_free(app->emf_view);

    view_dispatcher_remove_view(app->view_dispatcher, GhostTrackerViewMenu);
    menu_view_free(app->menu_view);

    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);

    free(app);
}

int32_t ghost_tracker_app(void* p) {
    UNUSED(p);
    GhostTrackerApp* app = ghost_tracker_app_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    view_dispatcher_switch_to_view(app->view_dispatcher, GhostTrackerViewSplash);

    view_dispatcher_run(app->view_dispatcher);

    ghost_tracker_app_free(app);
    return 0;
}
