#include "twentyq_app_i.h"

#include <furi_hal_random.h>

static bool twentyq_custom_event_callback(void* context, uint32_t event) {
    furi_assert(context);
    TwentyQApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

static bool twentyq_back_event_callback(void* context) {
    furi_assert(context);
    TwentyQApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

void twentyq_notify(TwentyQApp* app, const NotificationSequence* seq) {
    if(app->settings.sound || seq == &sequence_blink_blue_10) {
        notification_message(app->notifications, seq);
    }
    if(app->settings.vibro && seq == &sequence_success) {
        notification_message(app->notifications, &sequence_single_vibro);
    }
}

void twentyq_show_message(TwentyQApp* app, const char* header, const char* text) {
    strncpy(app->message_header, header, sizeof(app->message_header) - 1);
    app->message_header[sizeof(app->message_header) - 1] = '\0';
    strncpy(app->message, text, sizeof(app->message) - 1);
    app->message[sizeof(app->message) - 1] = '\0';
    scene_manager_next_scene(app->scene_manager, TwentyQSceneError);
}

void twentyq_show_error(TwentyQApp* app, const char* text) {
    twentyq_show_message(app, "Sorry", text);
}

static TwentyQApp* twentyq_app_alloc(void) {
    TwentyQApp* app = malloc(sizeof(TwentyQApp));
    memset(app, 0, sizeof(TwentyQApp));

    app->gui = furi_record_open(RECORD_GUI);
    app->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->storage = furi_record_open(RECORD_STORAGE);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager = scene_manager_alloc(&twentyq_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, twentyq_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, twentyq_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TwentyQViewSubmenu, submenu_get_view(app->submenu));
    app->dialog = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TwentyQViewDialog, dialog_ex_get_view(app->dialog));
    app->popup = popup_alloc();
    view_dispatcher_add_view(app->view_dispatcher, TwentyQViewPopup, popup_get_view(app->popup));
    app->text_input = text_input_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TwentyQViewTextInput, text_input_get_view(app->text_input));
    app->var_item_list = variable_item_list_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        TwentyQViewVarItemList,
        variable_item_list_get_view(app->var_item_list));
    app->widget = widget_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TwentyQViewWidget, widget_get_view(app->widget));
    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, TwentyQViewLoading, loading_get_view(app->loading));
    app->question_view = twentyq_question_view_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher,
        TwentyQViewQuestion,
        twentyq_question_view_get_view(app->question_view));

    twentyq_settings_load(app->storage, &app->settings);
    twentyq_stats_load(app->storage, &app->stats);
    return app;
}

static void twentyq_app_free(TwentyQApp* app) {
    furi_assert(app);

    if(app->engine) tq_engine_free(app->engine);
    tq_learned_clear(&app->learned);
    twentyq_storage_close_dataset(&app->store, app->dataset_ready ? &app->dataset : NULL);

    view_dispatcher_remove_view(app->view_dispatcher, TwentyQViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, TwentyQViewDialog);
    view_dispatcher_remove_view(app->view_dispatcher, TwentyQViewPopup);
    view_dispatcher_remove_view(app->view_dispatcher, TwentyQViewTextInput);
    view_dispatcher_remove_view(app->view_dispatcher, TwentyQViewVarItemList);
    view_dispatcher_remove_view(app->view_dispatcher, TwentyQViewWidget);
    view_dispatcher_remove_view(app->view_dispatcher, TwentyQViewLoading);
    view_dispatcher_remove_view(app->view_dispatcher, TwentyQViewQuestion);

    submenu_free(app->submenu);
    dialog_ex_free(app->dialog);
    popup_free(app->popup);
    text_input_free(app->text_input);
    variable_item_list_free(app->var_item_list);
    widget_free(app->widget);
    loading_free(app->loading);
    twentyq_question_view_free(app->question_view);

    scene_manager_free(app->scene_manager);
    view_dispatcher_free(app->view_dispatcher);

    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_NOTIFICATION);
    furi_record_close(RECORD_GUI);
    free(app);
}

int32_t twentyq_app(void* p) {
    UNUSED(p);
    TwentyQApp* app = twentyq_app_alloc();

    Tq_Error err = twentyq_storage_open_dataset(&app->store, app->storage, &app->dataset);
    if(err == TqOk) {
        app->dataset_ready = true;
        twentyq_learned_load_file(app->storage, &app->learned, app->dataset.crc32);
        app->engine = tq_engine_alloc(&app->dataset, &app->learned);
        tq_engine_seed(app->engine, furi_hal_random_get());
        FURI_LOG_I(
            TAG,
            "%s: %lu entities x %lu questions",
            app->dataset.dataset_id,
            app->dataset.entity_count,
            app->dataset.question_count);
    }

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    if(err == TqOk) {
        scene_manager_next_scene(app->scene_manager, TwentyQSceneStart);
    } else {
        /* Data file missing or corrupt: say so plainly and exit (SPEC 9). */
        const char* detail = (err == TqErrIo) ? "Data file missing.\nReinstall the app." :
                                                "Data file unusable.";
        snprintf(app->message, sizeof(app->message), "%s\n(%s)", detail, tq_error_str(err));
        strcpy(app->message_header, "Sorry");
        scene_manager_next_scene(app->scene_manager, TwentyQSceneError);
    }

    view_dispatcher_run(app->view_dispatcher);
    twentyq_app_free(app);
    return 0;
}
