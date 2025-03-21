#include <mizip_balance_editor.h>

// This function is called when there are custom events to process.
bool mizip_balance_editor_app_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

// This function is called when the user has pressed the Back key.
bool mizip_balance_editor_app_back_event_callback(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

// This function is called when the user presses the "Switch View" button on the Widget view.
void mizip_balance_editor_app_button_callback(
    GuiButtonType button_type,
    InputType input_type,
    void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    // Only request the view switch if the user short-presses the Center button.
    if(button_type == GuiButtonTypeCenter && input_type == InputTypeShort) {
        // Request switch to the Submenu view via the custom event queue.
        view_dispatcher_send_custom_event(app->view_dispatcher, MiZipBalanceEditorViewIdMainMenu);
    }
}

//Function to check if the file is a MiZip file
void mizip_balance_editor_load_file(MiZipBalanceEditorApp* app) {
    furi_assert(app);

    NfcDevice* nfc_device = nfc_device_alloc();
    nfc_device_load(nfc_device, furi_string_get_cstr(app->filePath));
    if(nfc_device_get_protocol(nfc_device) == NfcProtocolMfClassic) {
        //MiFare Classic
        //TODO Check if file is MiZip and load
        const MfClassicData* mf_classic_data =
            nfc_device_get_data(nfc_device, NfcProtocolMfClassic);
        mf_classic_copy(app->mf_classic_data, mf_classic_data);
    } else {
        //Invalid file
        //TODO Show error message and return to main menu
    }

    nfc_device_free(nfc_device);
}

// Application constructor function.
static MiZipBalanceEditorApp* mizip_balance_editor_app_alloc() {
    MiZipBalanceEditorApp* app = malloc(sizeof(MiZipBalanceEditorApp));
    app->gui = furi_record_open(RECORD_GUI);

    // Create the ViewDispatcher instance.
    app->view_dispatcher = view_dispatcher_alloc();

    app->scene_manager = scene_manager_alloc(&mizip_balance_editor_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, mizip_balance_editor_app_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, mizip_balance_editor_app_back_event_callback);
    // Create and initialize the Submenu view.
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MiZipBalanceEditorViewIdMainMenu, submenu_get_view(app->submenu));
    // Create and initialize the Widget view.
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, MiZipBalanceEditorViewIdWidget, widget_get_view(app->widget));

    app->storage = furi_record_open(RECORD_STORAGE);

    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->mf_classic_data = mf_classic_alloc();
    app->filePath = furi_string_alloc_set(NFC_APP_FOLDER);

    return app;
}

// Application destructor function.
static void mizip_balance_editor_app_free(MiZipBalanceEditorApp* app) {
    // All views must be un-registered (removed) from a ViewDispatcher instance
    // before deleting it. Failure to do so will result in a crash.
    view_dispatcher_remove_view(app->view_dispatcher, MiZipBalanceEditorViewIdMainMenu);
    submenu_free(app->submenu);

    view_dispatcher_remove_view(app->view_dispatcher, MiZipBalanceEditorViewIdWidget);
    widget_free(app->widget);

    // Now it is safe to delete the ViewDispatcher instance.
    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_GUI);
    app->gui = NULL;
    // Delete the views
    // End access to hte the GUI API.

    furi_record_close(RECORD_STORAGE);
    app->storage = NULL;

    furi_record_close(RECORD_DIALOGS);
    app->dialogs = NULL;

    mf_classic_free(app->mf_classic_data);
    furi_string_free(app->filePath);

    free(app);
}

int32_t mizip_balance_editor_app(void* p) {
    UNUSED(p);

    MiZipBalanceEditorApp* app = mizip_balance_editor_app_alloc();
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorSceneMainMenu);
    view_dispatcher_run(app->view_dispatcher);
    mizip_balance_editor_app_free(app);

    return 0;
}
