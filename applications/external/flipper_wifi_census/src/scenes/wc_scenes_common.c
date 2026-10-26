#include "include/scenes/wc_scenes_common.h"

// ---------------------------------------------------------------------------
// Shared callbacks and helpers
// ---------------------------------------------------------------------------

void wc_list_cb(void *context, uint32_t index) {
    WcApp *app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void wc_text_input_cb(void *context) {
    WcApp *app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, WcCustomEventTextDone);
}

// Called once per stored file. Only captures are listed; the .csv sidecars are not something
// the app can open.
static void files_list_cb(void *context, const char *name) {
    WcApp *app = context;
    size_t n = strlen(name);
    const size_t el = strlen(WC_CAP_EXT);
    if (n <= el || strcmp(name + n - el, WC_CAP_EXT) != 0) {
        return;
    }
    if (n >= WC_TEXT_BUF_SIZE) {
        app->list_overflow++; // stored truncated it would name a file that does not exist
        return;
    }
    if (app->list_count >= WC_MAX_NAMED) {
        app->list_overflow++; // no room to even name it; the header will say how many
        return;
    }
    strncpy(app->list_names[app->list_count], name, WC_TEXT_BUF_SIZE - 1);
    app->list_names[app->list_count][WC_TEXT_BUF_SIZE - 1] = '\0';
    wc_scroll_list_add_item(app->list_view, app->list_names[app->list_count], app->list_count,
                            wc_list_cb, app);
    app->list_count++;
}

void wc_populate_files(WcApp *app, const char *header) {
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, header);
    app->list_count = 0;
    app->list_overflow = 0;
    app->store.list(app->store.self, files_list_cb, app);
    wc_scroll_list_note_hidden(app->list_view, app->list_overflow);
    if (app->list_count == 0) {
        wc_scroll_list_add_item(app->list_view, "(no captures)", WC_MAX_LIST, wc_list_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
}

void wc_explain_load_failure(WcApp *app, const char *filename, char *out, size_t cap) {
    uint16_t count = wc_capture_service_device_count(&app->store, filename);
    if (count > WC_CENSUS_MAX_DEVICES) {
        snprintf(out, cap,
                 "%s holds\n%u devices.\nThis build opens %u.\n\nNothing is lost: merge\n"
                 "or compare it on a PC\n(see the README).",
                 filename, count, (unsigned)WC_CENSUS_MAX_DEVICES);
    } else {
        snprintf(out, cap, "Could not read\n%s.", filename);
    }
}

void wc_explain_which_failed(WcApp *app, const char *first, const char *second) {
    WcCaptureMeta meta;
    WcCensus probe;
    wc_census_init(&probe);
    bool first_loads = wc_capture_service_load(&app->store, first, &meta, &probe);
    wc_census_free(&probe);
    wc_explain_load_failure(app, first_loads ? second : first, app->result_text,
                            WC_RESULT_TEXT_SIZE);
}

// ---------------------------------------------------------------------------
// Msg (a one-off message screen)
// ---------------------------------------------------------------------------

// Show app->result_text on a full screen (used for load errors and other one-off messages).
void wc_show_message(WcApp *app, const char *text) {
    snprintf(app->result_text, sizeof(app->result_text), "%s", text);
    scene_manager_next_scene(app->scene_manager, WcSceneMsg);
}

void wc_scene_msg_on_enter(void *context) {
    WcApp *app = context;
    widget_reset(app->widget);
    widget_add_text_scroll_element(app->widget, 0, 0, 128, 64, app->result_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewWidget);
}

bool wc_scene_msg_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_msg_on_exit(void *context) {
    WcApp *app = context;
    widget_reset(app->widget);
}
