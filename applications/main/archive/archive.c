#include "archive_i.h"

static bool archive_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    ArchiveApp* archive = context;
    return scene_manager_handle_custom_event(archive->scene_manager, event);
}

static bool archive_back_event_callback(void* context) {
    furi_assert(context);
    ArchiveApp* archive = context;
    return scene_manager_handle_back_event(archive->scene_manager);
}

static void archive_tick_event_callback(void* context) {
    furi_assert(context);
    ArchiveApp* archive = context;
    scene_manager_handle_tick_event(archive->scene_manager);
}

static ArchiveApp* archive_alloc(void) {
    ArchiveApp* archive = malloc(sizeof(ArchiveApp));

    archive->gui = furi_record_open(RECORD_GUI);
    archive->loader = furi_record_open(RECORD_LOADER);
    archive->fav_move_str = furi_string_alloc();
    archive->file_extension = furi_string_alloc();

    archive->scene_manager = scene_manager_alloc(&archive_scene_handlers, archive);
    archive->view_dispatcher = view_dispatcher_alloc();

    ViewDispatcher* view_dispatcher = archive->view_dispatcher;
    view_dispatcher_enable_queue(view_dispatcher);
    view_dispatcher_set_event_callback_context(view_dispatcher, archive);
    view_dispatcher_set_custom_event_callback(view_dispatcher, archive_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(view_dispatcher, archive_back_event_callback);
    view_dispatcher_set_tick_event_callback(view_dispatcher, archive_tick_event_callback, 100);

    archive->dialogs = furi_record_open(RECORD_DIALOGS);

    archive->text_input = text_input_alloc();
    view_dispatcher_add_view(
        view_dispatcher, ArchiveViewTextInput, text_input_get_view(archive->text_input));

    archive->widget = widget_alloc();
    view_dispatcher_add_view(
        archive->view_dispatcher, ArchiveViewWidget, widget_get_view(archive->widget));

    archive->view_stack = view_stack_alloc();
    view_dispatcher_add_view(
        view_dispatcher, ArchiveViewStack, view_stack_get_view(archive->view_stack));

    archive->browser = browser_alloc();
    with_view_model(
        archive->browser->view,
        ArchiveBrowserViewModel * model,
        { model->archive = archive; },
        true);
    view_dispatcher_add_view(
        archive->view_dispatcher, ArchiveViewBrowser, archive_browser_get_view(archive->browser));

    // Loading
    archive->loading = loading_alloc();

    return archive;
}

void archive_free(ArchiveApp* archive) {
    furi_assert(archive);
    ViewDispatcher* view_dispatcher = archive->view_dispatcher;

    scene_manager_set_scene_state(archive->scene_manager, ArchiveAppSceneInfo, false);
    scene_manager_set_scene_state(archive->scene_manager, ArchiveAppSceneSearch, false);
    if(archive->info_thread) {
        furi_thread_join(archive->info_thread);
        furi_thread_free(archive->info_thread);
        archive->info_thread = NULL;
    }
    if(archive->search_thread) {
        furi_thread_join(archive->search_thread);
        furi_thread_free(archive->search_thread);
        archive->search_thread = NULL;
    }

    if(archive->browser->disk_image) {
        storage_virtual_quit(furi_record_open(RECORD_STORAGE));
        furi_record_close(RECORD_STORAGE);
        storage_file_free(archive->browser->disk_image);
        archive->browser->disk_image = NULL;
    }

    // Loading
    loading_free(archive->loading);

    view_dispatcher_remove_view(view_dispatcher, ArchiveViewTextInput);
    text_input_free(archive->text_input);

    view_dispatcher_remove_view(view_dispatcher, ArchiveViewWidget);
    widget_free(archive->widget);

    view_dispatcher_remove_view(view_dispatcher, ArchiveViewStack);
    view_stack_free(archive->view_stack);

    view_dispatcher_remove_view(view_dispatcher, ArchiveViewBrowser);

    view_dispatcher_free(archive->view_dispatcher);
    scene_manager_free(archive->scene_manager);

    browser_free(archive->browser);
    furi_string_free(archive->fav_move_str);
    furi_string_free(archive->file_extension);

    furi_record_close(RECORD_DIALOGS);
    archive->dialogs = NULL;

    furi_record_close(RECORD_LOADER);
    archive->loader = NULL;
    furi_record_close(RECORD_GUI);
    archive->gui = NULL;

    free(archive);
}

void archive_show_loading_popup(ArchiveApp* context, bool show) {
    ViewStack* view_stack = context->view_stack;
    Loading* loading = context->loading;

    if(show) {
        // Raise timer priority so that animations can play
        furi_timer_set_thread_priority(FuriTimerThreadPriorityElevated);
        view_stack_add_view(view_stack, loading_get_view(loading));
    } else {
        view_stack_remove_view(view_stack, loading_get_view(loading));
        // Restore default timer priority
        furi_timer_set_thread_priority(FuriTimerThreadPriorityNormal);
    }
}

int32_t archive_app(void* p) {
    UNUSED(p);

    ArchiveApp* archive = archive_alloc();
    view_dispatcher_attach_to_gui(
        archive->view_dispatcher, archive->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(archive->scene_manager, ArchiveAppSceneBrowser);
    view_dispatcher_run(archive->view_dispatcher);

    archive_free(archive);
    return 0;
}
