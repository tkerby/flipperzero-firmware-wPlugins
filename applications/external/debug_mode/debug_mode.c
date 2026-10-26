#include "debug_mode.h"
#include <string.h>

static bool debug_mode_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    DebugMode* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool debug_mode_back_event_callback(void* context) {
    furi_assert(context);
    DebugMode* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

static void debug_mode_splash_timer_callback(void* context) {
    furi_assert(context);
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DebugModeCustomEventSplashTick);
}

static void debug_mode_nfc_scan_timer_callback(void* context) {
    furi_assert(context);
    DebugMode* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, DebugModeCustomEventNfcScanTick);
}

static DebugMode* debug_mode_alloc(void) {
    DebugMode* app = malloc(sizeof(DebugMode));
    memset(app, 0, sizeof(DebugMode));

    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->nfc = nfc_alloc();

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&debug_mode_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, debug_mode_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, debug_mode_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DebugModeViewIdSubmenu, submenu_get_view(app->submenu));

    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DebugModeViewIdTextInput, text_input_get_view(app->text_input));

    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DebugModeViewIdWidget, widget_get_view(app->widget));

    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, DebugModeViewIdDialogEx, dialog_ex_get_view(app->dialog_ex));

    app->splash_view = debug_mode_splash_view_alloc(app);
    view_dispatcher_add_view(app->view_dispatcher, DebugModeViewIdSplash, app->splash_view);

    app->splash_timer =
        furi_timer_alloc(debug_mode_splash_timer_callback, FuriTimerTypePeriodic, app);
    app->nfc_scan_timer =
        furi_timer_alloc(debug_mode_nfc_scan_timer_callback, FuriTimerTypePeriodic, app);

    case_store_init(app->storage);

    return app;
}

static void debug_mode_free(DebugMode* app) {
    furi_assert(app);

    if(furi_timer_is_running(app->splash_timer)) {
        furi_timer_stop(app->splash_timer);
    }
    furi_timer_free(app->splash_timer);

    if(furi_timer_is_running(app->nfc_scan_timer)) {
        furi_timer_stop(app->nfc_scan_timer);
    }
    furi_timer_free(app->nfc_scan_timer);

    view_dispatcher_remove_view(app->view_dispatcher, DebugModeViewIdSplash);
    debug_mode_splash_view_free(app->splash_view);

    view_dispatcher_remove_view(app->view_dispatcher, DebugModeViewIdDialogEx);
    dialog_ex_free(app->dialog_ex);

    view_dispatcher_remove_view(app->view_dispatcher, DebugModeViewIdWidget);
    widget_free(app->widget);

    view_dispatcher_remove_view(app->view_dispatcher, DebugModeViewIdTextInput);
    text_input_free(app->text_input);

    view_dispatcher_remove_view(app->view_dispatcher, DebugModeViewIdSubmenu);
    submenu_free(app->submenu);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    nfc_free(app->nfc);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_GUI);

    free(app);
}

void debug_mode_save_pending_entry_and_return(DebugMode* app) {
    bool ok = case_store_append_entry(
        app->storage,
        app->current_case_id,
        app->pending_entry_type,
        app->pending_entry_text,
        app->pending_entry_outcome,
        app->pending_entry_note);

    if(!ok) {
        snprintf(app->message_title, sizeof(app->message_title), "Save Failed");
        snprintf(
            app->message_body,
            sizeof(app->message_body),
            "Could not save the entry. The case may be full or missing.");
        app->message_return_scene = DebugModeSceneCaseView;
        scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
        return;
    }

    if(!scene_manager_search_and_switch_to_previous_scene(
           app->scene_manager, DebugModeSceneCaseView)) {
        snprintf(app->message_title, sizeof(app->message_title), "Logged");
        snprintf(
            app->message_body,
            sizeof(app->message_body),
            "Entry added to \"%s\".",
            app->current_case_title);
        app->message_return_scene = DebugModeSceneMainMenu;
        scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
    }
}

int32_t debug_mode_app(void* p) {
    UNUSED(p);
    DebugMode* app = debug_mode_alloc();

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, DebugModeSceneSplash);

    view_dispatcher_run(app->view_dispatcher);

    debug_mode_free(app);

    return 0;
}
