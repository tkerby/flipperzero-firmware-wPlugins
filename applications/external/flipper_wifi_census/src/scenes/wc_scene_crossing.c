#include "include/application/wc_merge_service.h"
#include "include/domain/wc_timefmt.h"
#include "include/scenes/wc_scenes_common.h"

// Store the capture just picked from a file list in slot 0 or 1. Returns false when the event
// is not a pick (the "(no captures)" placeholder carries an index past the list), so the scene
// can ignore it. Compare and Merge differ only in which slot they fill and what follows.
static bool pick_capture(WcApp *app, SceneManagerEvent event, uint8_t slot) {
    if (event.type != SceneManagerEventTypeCustom || event.event >= app->list_count) {
        return false;
    }
    strncpy(app->picked[slot], app->list_names[event.event], WC_TEXT_BUF_SIZE - 1);
    app->picked[slot][WC_TEXT_BUF_SIZE - 1] = '\0';
    return true;
}

static const char *known_label_for(WcApp *app, const WcMatch *m) {
    WcSignature sig;
    memset(&sig, 0, sizeof(sig));
    memcpy(sig.mac, m->mac, 6);
    sig.mac_random = wc_mac_is_random(m->mac);
    if (m->reason == WcMatchBySsid) {
        strncpy(sig.ssids[0], m->detail, WC_SSID_MAX_LEN);
        sig.ssid_count = 1;
    }
    const WcKnown *k = wc_known_match(&app->known, &sig);
    return k ? k->label : NULL;
}

// ---------------------------------------------------------------------------
// Compare: pick A, pick B, show result
// ---------------------------------------------------------------------------

void wc_scene_compare_a_on_enter(void *context) {
    WcApp *app = context;
    wc_populate_files(app, "Compare: pick A");
}

bool wc_scene_compare_a_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (!pick_capture(app, event, 0)) {
        return false;
    }
    scene_manager_next_scene(app->scene_manager, WcSceneCompareB);
    return true;
}

void wc_scene_compare_a_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

// The match list is bounded twice over - by WC_COMPARE_MAX_MATCHES and by this text buffer -
// so it says how many of them it is actually showing. A list that just stops looks like the
// crossing found fewer devices than it did.
static void format_compare(WcApp *app, const WcCompareResult *r) {
    size_t len = 0;
    len += snprintf(app->result_text + len, WC_RESULT_TEXT_SIZE - len,
                    "A:%u B:%u  common:%u\nrandom(not crossable) A:%u B:%u\n", r->na, r->nb,
                    r->intersection, r->random_a, r->random_b);
    // Keep room for the "showing N of M" line, so the warning can never itself be cut off.
    // 64, not 48: the tail below is 54 fixed characters plus the two counts, so the line
    // written to stop anything being hidden was itself always cut off mid-word.
    const size_t room = WC_RESULT_TEXT_SIZE - 64;
    uint16_t shown = 0;
    for (uint16_t i = 0; i < r->match_count && len < room; i++) {
        const WcMatch *m = &r->matches[i];
        const char *label = known_label_for(app, m);
        int n;
        if (m->reason == WcMatchBySsid) {
            n = snprintf(app->result_text + len, room - len, "- %s [ssid:%s]\n",
                         label ? label : "device", m->detail);
        } else {
            n = snprintf(app->result_text + len, room - len, "- %s %02X:%02X:%02X:%02X:%02X:%02X\n",
                         label ? label : "device", m->mac[0], m->mac[1], m->mac[2], m->mac[3],
                         m->mac[4], m->mac[5]);
        }
        // snprintf returns what it WOULD have written, so a truncated row must not be added
        // whole: len would run past the buffer and the tail write below would go out of bounds.
        if (n <= 0 || len + (size_t)n >= room) {
            len = (len < room) ? len : room - 1;
            break; // the row did not fit, so it was not shown
        }
        len += (size_t)n;
        shown++;
    }
    if (shown < r->intersection) {
        snprintf(app->result_text + len, WC_RESULT_TEXT_SIZE - len,
                 "\n(listing %u of %u -\nthe count above is\nthe whole answer)\n", shown,
                 r->intersection);
    }
}

void wc_scene_compare_b_on_enter(void *context) {
    WcApp *app = context;
    wc_populate_files(app, "Pick B");
}

bool wc_scene_compare_b_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (pick_capture(app, event, 1)) {
        WcCompareResult *r = malloc(sizeof(WcCompareResult));
        if (!r) {
            wc_show_message(app, "Not enough memory to\ncompare these two.\n\nTry shorter "
                                 "captures, or\ncompare them on a PC.");
            return true;
        }
        {
            if (wc_compare_service_run(&app->store, app->picked[0], app->picked[1], r)) {
                format_compare(app, r);
            } else {
                wc_explain_which_failed(app, app->picked[0], app->picked[1]);
            }
            free(r);
            scene_manager_next_scene(app->scene_manager, WcSceneCompareResult);
        }
        return true;
    }
    return false;
}

void wc_scene_compare_b_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

void wc_scene_compare_result_on_enter(void *context) {
    WcApp *app = context;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->result_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextBox);
}

bool wc_scene_compare_result_on_event(void *context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

void wc_scene_compare_result_on_exit(void *context) {
    WcApp *app = context;
    text_box_reset(app->text_box);
}

// ---------------------------------------------------------------------------
// Merge: pick A, pick B, name the accumulated capture
// ---------------------------------------------------------------------------

void wc_scene_merge_a_on_enter(void *context) {
    WcApp *app = context;
    wc_populate_files(app, "Merge - pick 1st capture");
}

bool wc_scene_merge_a_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (!pick_capture(app, event, 0)) {
        return false;
    }
    scene_manager_next_scene(app->scene_manager, WcSceneMergeB);
    return true;
}

void wc_scene_merge_a_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

void wc_scene_merge_b_on_enter(void *context) {
    WcApp *app = context;
    char header[WC_TEXT_BUF_SIZE + 20];
    snprintf(header, sizeof(header), "2nd to add to %s", app->picked[0]);
    wc_populate_files(app, header);
}

bool wc_scene_merge_b_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (pick_capture(app, event, 1)) {
        scene_manager_next_scene(app->scene_manager, WcSceneMergeName);
        return true;
    }
    return false;
}

void wc_scene_merge_b_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

void wc_scene_merge_name_on_enter(void *context) {
    WcApp *app = context;
    wc_default_name(wc_clock_now(&app->clock), "merge", app->text_buf, sizeof(app->text_buf));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "Merged name");
    text_input_set_result_callback(app->text_input, wc_text_input_cb, app, app->text_buf,
                                   WC_BASENAME_MAX, false);
    text_input_set_minimum_length(app->text_input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextInput);
}

bool wc_scene_merge_name_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventTextDone) {
        uint16_t devices = 0;
        uint16_t dropped = 0;
        bool ok = wc_merge_service_run(&app->store, app->picked[0], app->picked[1], app->text_buf,
                                       &devices, &dropped);
        if (ok) {
            // A merge past the ceiling still saves a valid file, so say what it cost: the loss
            // is otherwise indistinguishable from two captures that simply overlapped a lot.
            char tail[96];
            if (dropped > 0) {
                snprintf(tail, sizeof(tail),
                         "%u dropped (%u max)\nMerge on a PC to\nkeep them all\n(back = menu)",
                         dropped, (unsigned)WC_CENSUS_MAX_DEVICES);
            } else {
                snprintf(tail, sizeof(tail), "(back = menu)");
            }
            snprintf(app->result_text, WC_RESULT_TEXT_SIZE,
                     "Merged\n%s\n+ %s\n=> %s%s\n\n%u unique devices\n%s", app->picked[0],
                     app->picked[1], app->text_buf, WC_CAP_EXT, devices, tail);
        } else {
            wc_explain_which_failed(app, app->picked[0], app->picked[1]);
        }
        scene_manager_next_scene(app->scene_manager, WcSceneMergeResult);
        return true;
    }
    return false;
}

void wc_scene_merge_name_on_exit(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
}

void wc_scene_merge_result_on_enter(void *context) {
    WcApp *app = context;
    text_box_reset(app->text_box);
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_text(app->text_box, app->result_text);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextBox);
}

bool wc_scene_merge_result_on_event(void *context, SceneManagerEvent event) {
    if (event.type == SceneManagerEventTypeBack) {
        WcApp *app = context;
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, WcSceneStart);
        return true;
    }
    return false;
}

void wc_scene_merge_result_on_exit(void *context) {
    WcApp *app = context;
    text_box_reset(app->text_box);
}
