#include "ami_tool_i.h"
#include <string.h>

/* Forward declarations of callbacks */
static bool ami_tool_custom_event_callback(void* context, uint32_t event);
static bool ami_tool_back_event_callback(void* context);
static void ami_tool_tick_event_callback(void* context);

/* Allocate and initialize app */
AmiToolApp* ami_tool_alloc(void) {
    AmiToolApp* app = malloc(sizeof(AmiToolApp));

    /* Scene manager */
    app->scene_manager = scene_manager_alloc(&ami_tool_scene_handlers, app);

    /* View dispatcher */
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, ami_tool_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, ami_tool_back_event_callback);
    view_dispatcher_set_tick_event_callback(
        app->view_dispatcher, ami_tool_tick_event_callback, 100);

    /* GUI record and attach */
    app->gui = furi_record_open(RECORD_GUI);
    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    /* Submenu (main menu view) */
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AmiToolViewMenu, submenu_get_view(app->submenu));

    /* TextBox (simple placeholder screens) */
    app->text_box = text_box_alloc();
    text_box_set_focus(app->text_box, TextBoxFocusStart);
    app->text_box_store = furi_string_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, AmiToolViewTextBox, text_box_get_view(app->text_box));

    /* Storage (for assets) */
    app->storage = furi_record_open(RECORD_STORAGE);

    /* NFC resources */
    app->nfc = nfc_alloc();
    app->read_thread = NULL;
    app->read_scene_active = false;
    memset(&app->read_result, 0, sizeof(app->read_result));
    app->read_result.type = AmiToolReadResultNone;
    app->read_result.error = MfUltralightErrorNone;
    app->tag_data = mf_ultralight_alloc();
    app->tag_data_valid = false;

    /* Generate scene state */
    app->generate_state = AmiToolGenerateStateRootMenu;
    app->generate_return_state = AmiToolGenerateStateRootMenu;
    app->generate_platform = AmiToolGeneratePlatform3DS;
    app->generate_game_count = 0;

    return app;
}

/* Free everything */
void ami_tool_free(AmiToolApp* app) {
    furi_assert(app);

    /* Remove views from dispatcher */
    view_dispatcher_remove_view(app->view_dispatcher, AmiToolViewMenu);
    view_dispatcher_remove_view(app->view_dispatcher, AmiToolViewTextBox);

    /* Free modules */
    submenu_free(app->submenu);
    text_box_free(app->text_box);
    furi_string_free(app->text_box_store);

    /* NFC resources */
    if(app->read_thread) {
        furi_thread_join(app->read_thread);
        furi_thread_free(app->read_thread);
        app->read_thread = NULL;
    }
    if(app->nfc) {
        nfc_free(app->nfc);
        app->nfc = NULL;
    }
    if(app->tag_data) {
        mf_ultralight_free(app->tag_data);
        app->tag_data = NULL;
    }

    /* View dispatcher & scene manager */
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    /* Storage */
    if(app->storage) {
        furi_record_close(RECORD_STORAGE);
        app->storage = NULL;
    }

    /* GUI record */
    furi_record_close(RECORD_GUI);
    app->gui = NULL;

    free(app);
}

/* Custom events from views -> SceneManager */
static bool ami_tool_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    AmiToolApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

/* Back button -> SceneManager (and scenes decide what to do) */
static bool ami_tool_back_event_callback(void* context) {
    furi_assert(context);
    AmiToolApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/* Tick events (not used yet but enabled for future animations) */
static void ami_tool_tick_event_callback(void* context) {
    furi_assert(context);
    AmiToolApp* app = context;
    scene_manager_handle_tick_event(app->scene_manager);
}

/* Entry point (matches application.fam entry_point) */
int32_t ami_tool_app(void* p) {
    UNUSED(p);

    AmiToolApp* app = ami_tool_alloc();

    /* Start with main menu scene */
    scene_manager_next_scene(app->scene_manager, AmiToolSceneMainMenu);

    /* Main loop; returns when scene_manager_stop() called */
    view_dispatcher_run(app->view_dispatcher);

    ami_tool_free(app);

    return 0;
}
