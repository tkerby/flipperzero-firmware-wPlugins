#include "include/domain/wc_observation.h"
#include "include/scenes/wc_scenes_common.h"

// ---------------------------------------------------------------------------
// Mark label (turn a device into a known rule)
// ---------------------------------------------------------------------------

void wc_scene_mark_label_on_enter(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
    // Pre-fill a generic name so the user can just confirm without typing.
    const char *header = "Label for device";
    switch (app->mark_mode) {
        case WcMarkSsid:
            header = "Label for network";
            snprintf(app->text_buf, sizeof(app->text_buf), "net_%s", app->selected_ssid);
            break;
        case WcMarkRename:
            header = "Rename";
            if (app->selected_known < app->known.count) {
                snprintf(app->text_buf, sizeof(app->text_buf), "%s",
                         app->known.items[app->selected_known].label);
            } else {
                app->text_buf[0] = '\0';
            }
            break;
        case WcMarkDevice:
        default:
            if (app->browse_census && app->selected_device < app->browse_census->count) {
                const WcSignature *d = &app->browse_census->devices[app->selected_device];
                if (d->ssid_count > 0) {
                    snprintf(app->text_buf, sizeof(app->text_buf), "net_%s", d->ssids[0]);
                } else {
                    snprintf(app->text_buf, sizeof(app->text_buf), "dev_%02X%02X%02X", d->mac[3],
                             d->mac[4], d->mac[5]);
                }
            } else {
                app->text_buf[0] = '\0';
            }
            break;
    }
    app->text_buf[WC_KNOWN_LABEL_MAX] = '\0'; // labels are stored clamped to this length
    text_input_set_header_text(app->text_input, header);
    text_input_set_result_callback(app->text_input, wc_text_input_cb, app, app->text_buf,
                                   WC_KNOWN_LABEL_MAX + 1, false);
    text_input_set_minimum_length(app->text_input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextInput);
}

bool wc_scene_mark_label_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventTextDone) {
        // The outcome decides what the user is told. Dropping it made a full registry, an
        // undurable device and a failed write all look like success.
        bool ok = false;
        bool undurable = false; // the device itself yields no rule, as opposed to a failed write
        switch (app->mark_mode) {
            case WcMarkSsid:
                ok = wc_known_service_mark_ssid(&app->store, app->selected_ssid, app->text_buf);
                break;
            case WcMarkRename:
                ok = wc_known_service_rename(&app->store, app->selected_known, app->text_buf);
                break;
            case WcMarkDevice:
            default:
                if (app->browse_census && app->selected_device < app->browse_census->count) {
                    const WcSignature *sig = &app->browse_census->devices[app->selected_device];
                    WcKnown probe;
                    undurable = !wc_known_rule_from_signature(&probe, sig, app->text_buf);
                    ok = !undurable && wc_known_service_mark(&app->store, sig, app->text_buf);
                }
                break;
        }
        wc_known_service_load(&app->store, &app->known);
        if (!ok) {
            if (app->known.count >= WC_KNOWN_MAX) {
                snprintf(app->result_text, WC_RESULT_TEXT_SIZE,
                         "Not saved.\n\nThe known registry is\nfull (%u rules).\nDelete one "
                         "first.",
                         (unsigned)WC_KNOWN_MAX);
            } else if (undurable) {
                snprintf(app->result_text, WC_RESULT_TEXT_SIZE,
                         "Not saved.\n\nThis device gives no\ndurable rule: a random\nMAC that "
                         "names no\nnetwork cannot be\nrecognised again.");
            } else {
                // Everything the rule needs was there, so the card refused the write. Saying
                // "no durable rule" here blamed the device for an SD problem.
                snprintf(app->result_text, WC_RESULT_TEXT_SIZE,
                         "Not saved.\n\nThe SD refused the\nwrite - card absent,\nfull, or "
                         "write\nprotected.");
            }
            scene_manager_next_scene(app->scene_manager, WcSceneMsg);
            return true;
        }
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void wc_scene_mark_label_on_exit(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
}

// ---------------------------------------------------------------------------
// Known devices (list)
// ---------------------------------------------------------------------------

static void known_row_label(const void *context, uint32_t index, char *out, size_t cap) {
    const WcApp *app = context;
    if (index >= app->known.count) {
        out[0] = '\0';
        return;
    }
    const WcKnown *k = &app->known.items[index];
    if (k->type == WcRuleBySsid) {
        snprintf(out, cap, "%s [ssid:%s]", k->label, k->ssid);
    } else {
        snprintf(out, cap, "%s [%02X:%02X:%02X:%02X:%02X:%02X]", k->label, k->mac[0], k->mac[1],
                 k->mac[2], k->mac[3], k->mac[4], k->mac[5]);
    }
}

void wc_scene_known_on_enter(void *context) {
    WcApp *app = context;
    wc_known_service_load(&app->store, &app->known);
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, "Known (select to edit)");
    if (app->known.count > 0) {
        wc_scroll_list_add_generated(app->list_view, app->known.count, known_row_label, wc_list_cb,
                                     app);
    } else {
        wc_scroll_list_add_item(app->list_view, "(none yet)", WC_MAX_LIST, wc_list_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
}

bool wc_scene_known_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event < app->known.count) {
        app->selected_known = (uint16_t)event.event;
        scene_manager_next_scene(app->scene_manager, WcSceneKnownActions);
        return true;
    }
    return false;
}

void wc_scene_known_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

// ---------------------------------------------------------------------------
// Known actions (rename / delete the selected known entry)
// ---------------------------------------------------------------------------

enum {
    WcKnownActionRename = 0,
    WcKnownActionDelete = 1,
};

void wc_scene_known_actions_on_enter(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
    const char *lbl = (app->selected_known < app->known.count)
                          ? app->known.items[app->selected_known].label
                          : "?";
    wc_scroll_list_set_header(app->list_view, lbl);
    wc_scroll_list_add_item(app->list_view, "Rename", WcKnownActionRename, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Delete", WcKnownActionDelete, wc_list_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
}

bool wc_scene_known_actions_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom) {
        if (event.event == WcKnownActionRename) {
            app->mark_mode = WcMarkRename;
            scene_manager_next_scene(app->scene_manager, WcSceneMarkLabel);
            return true;
        }
        if (event.event == WcKnownActionDelete) {
            wc_known_service_remove(&app->store, app->selected_known);
            wc_known_service_load(&app->store, &app->known);
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
    }
    return false;
}

void wc_scene_known_actions_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}
