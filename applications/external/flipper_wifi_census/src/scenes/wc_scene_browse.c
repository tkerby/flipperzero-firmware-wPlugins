#include "include/domain/wc_observation.h"
#include "include/domain/wc_timefmt.h"
#include "include/scenes/wc_scenes_common.h"

// Replace the capture extension on `base` (bare label) to build "<base>.csv".
static void csv_name_of(const char *base_with_ext, char *out, size_t cap) {
    size_t n = strlen(base_with_ext);
    size_t el = strlen(WC_CAP_EXT);
    size_t stem = (n > el) ? n - el : n;
    snprintf(out, cap, "%.*s%s", (int)stem, base_with_ext, WC_CSV_EXT);
}

// ---------------------------------------------------------------------------
// Files
// ---------------------------------------------------------------------------

void wc_scene_files_on_enter(void *context) {
    WcApp *app = context;
    if (app->browse_census) {
        wc_census_free(app->browse_census);
        free(app->browse_census);
        app->browse_census = NULL;
    }
    wc_populate_files(app, "Captures");
}

bool wc_scene_files_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event < app->list_count) {
        strncpy(app->selected_file, app->list_names[event.event], WC_TEXT_BUF_SIZE - 1);
        app->selected_file[WC_TEXT_BUF_SIZE - 1] = '\0';
        scene_manager_next_scene(app->scene_manager, WcSceneFileActions);
        return true;
    }
    return false;
}

void wc_scene_files_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

// ---------------------------------------------------------------------------
// File actions
// ---------------------------------------------------------------------------

typedef enum {
    ActionSummary,
    ActionDevices,
    ActionNetworks,
    ActionRename,
    ActionDelete,
} FileAction;

// Load the selected capture into browse_census (allocating once). Returns true on success.
static bool ensure_browse_loaded(WcApp *app) {
    WcCaptureMeta meta; // the loader needs somewhere to put it; nothing reads it back
    if (!app->browse_census) {
        app->browse_census = malloc(sizeof(WcCensus));
        if (app->browse_census) {
            wc_census_init(app->browse_census); // the reader keeps the ceiling it is handed
        }
    }
    return app->browse_census &&
           wc_capture_service_load(&app->store, app->selected_file, &meta, app->browse_census);
}

// A stored capture had no summary at all: its numbers only ever existed on the live scan
// screen. The phone bound only appears for captures imported from a pcap - a UART scan reads
// Marauder's summary lines, which carry no IE fingerprint to bound anything with.
static void show_capture_summary(WcApp *app) {
    const WcCensus *c = app->browse_census;
    WcCensusStats s = wc_census_stats(c);
    uint16_t lo = 0, hi = 0;
    char phones[160];
    if (wc_census_phone_bound(c, &lo, &hi)) {
        snprintf(phones, sizeof(phones),
                 "\nPhones behind those\nrandom MACs: %u-%u\n(same model shares a\nfingerprint, so "
                 "the\nlow end is a floor)",
                 lo, hi);
    } else {
        snprintf(phones, sizeof(phones),
                 "\nNo IE fingerprints:\nimport a pcap to bound\nthe phone count.");
    }
    char lost[72] = "";
    if (s.dropped > 0) {
        snprintf(lost, sizeof(lost), "\n%u DROPPED at the\n%u-device ceiling\n", s.dropped,
                 (unsigned)WC_CENSUS_MAX_DEVICES);
    }
    snprintf(
        app->result_text, WC_RESULT_TEXT_SIZE,
        "%s\n\nDevices: %u\nStable: %u\nRandom: %u (%u%%)\n%s\nPhone %u  Laptop %u\nIoT %u  AP %u\n"
        "Networks sought: %u\n%s",
        app->selected_file, s.total, s.unique_stable, s.random_count, s.pct_random, lost,
        s.by_type[WcDevicePhone], s.by_type[WcDeviceLaptop], s.by_type[WcDeviceIot],
        s.by_type[WcDeviceAp], s.networks, phones);
    scene_manager_next_scene(app->scene_manager, WcSceneMsg);
}

void wc_scene_file_actions_on_enter(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, app->selected_file);
    wc_scroll_list_add_item(app->list_view, "Summary", ActionSummary, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Devices", ActionDevices, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Networks sought", ActionNetworks, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Rename", ActionRename, wc_list_cb, app);
    wc_scroll_list_add_item(app->list_view, "Delete", ActionDelete, wc_list_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
}

bool wc_scene_file_actions_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type != SceneManagerEventTypeCustom) {
        return false;
    }
    switch (event.event) {
        case ActionSummary:
            if (ensure_browse_loaded(app)) {
                show_capture_summary(app);
            } else {
                wc_explain_load_failure(app, app->selected_file, app->result_text,
                                        WC_RESULT_TEXT_SIZE);
                scene_manager_next_scene(app->scene_manager, WcSceneMsg);
            }
            return true;
        case ActionDevices:
            if (ensure_browse_loaded(app)) {
                scene_manager_next_scene(app->scene_manager, WcSceneDevices);
            } else {
                wc_explain_load_failure(app, app->selected_file, app->result_text,
                                        WC_RESULT_TEXT_SIZE);
                scene_manager_next_scene(app->scene_manager, WcSceneMsg);
            }
            return true;
        case ActionNetworks:
            if (ensure_browse_loaded(app)) {
                scene_manager_next_scene(app->scene_manager, WcSceneNetworks);
            } else {
                wc_explain_load_failure(app, app->selected_file, app->result_text,
                                        WC_RESULT_TEXT_SIZE);
                scene_manager_next_scene(app->scene_manager, WcSceneMsg);
            }
            return true;
        case ActionRename:
            scene_manager_next_scene(app->scene_manager, WcSceneRename);
            return true;
        case ActionDelete: {
            char csv[WC_TEXT_BUF_SIZE];
            csv_name_of(app->selected_file, csv, sizeof(csv));
            if (!app->store.delete_file(app->store.self, app->selected_file)) {
                wc_show_message(app, "Could not delete it.\nThe SD may be absent\nor write "
                                     "protected.");
                return true;
            }
            app->store.delete_file(app->store.self, csv); // a missing sidecar is not an error
            scene_manager_previous_scene(app->scene_manager);
            return true;
        }
        default:
            return false;
    }
}

void wc_scene_file_actions_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

// ---------------------------------------------------------------------------
// Devices (of a loaded capture)
// ---------------------------------------------------------------------------

// Lead with the most identifying bit so a row still says something before it scrolls: the
// probed network if any (the key signal), else the vendor, else the MAC tail. Full detail is
// one click away in the detail scene.
static void device_label(const void *context, uint32_t index, char *out, size_t cap) {
    const WcApp *app = context;
    const WcCensus *c = app->browse_census;
    if (!c || index >= c->count) {
        out[0] = '\0';
        return;
    }
    const WcSignature *d = &c->devices[index];
    if (d->ssid_count > 0) {
        snprintf(out, cap, "%s >%s", wc_device_type_name(d->type), d->ssids[0]);
        return;
    }
    const char *vendor = wc_signature_vendor(d);
    if (vendor[0]) {
        snprintf(out, cap, "%s %s %02X:%02X:%02X:%02X:%02X:%02X", wc_device_type_name(d->type),
                 vendor, d->mac[0], d->mac[1], d->mac[2], d->mac[3], d->mac[4], d->mac[5]);
    } else {
        snprintf(out, cap, "%s %02X:%02X:%02X:%02X:%02X:%02X", wc_device_type_name(d->type),
                 d->mac[0], d->mac[1], d->mac[2], d->mac[3], d->mac[4], d->mac[5]);
    }
}

void wc_scene_devices_on_enter(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, "Devices (select)");
    const WcCensus *c = app->browse_census;
    uint16_t n = c ? c->count : 0;
    if (n > WC_MAX_LIST) {
        n = WC_MAX_LIST;
    }
    wc_scroll_list_add_generated(app->list_view, n, device_label, wc_list_cb, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
}

bool wc_scene_devices_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    uint16_t n = app->browse_census ? app->browse_census->count : 0;
    if (event.type == SceneManagerEventTypeCustom && event.event < n) {
        app->selected_device = (uint16_t)event.event;
        scene_manager_next_scene(app->scene_manager, WcSceneDeviceDetail);
        return true;
    }
    return false;
}

void wc_scene_devices_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

// ---------------------------------------------------------------------------
// Device detail (full info + probed networks; mark known from here)
// ---------------------------------------------------------------------------

static void device_detail_button_cb(GuiButtonType result, InputType type, void *context) {
    if (result == GuiButtonTypeCenter && type == InputTypeShort) {
        WcApp *app = context;
        view_dispatcher_send_custom_event(app->view_dispatcher, WcCustomEventButton);
    }
}

// Bounded append: snprintf into buf+*len, advancing *len and never running past cap.
static void detail_catf(char *buf, size_t *len, size_t cap, const char *s) {
    if (*len >= cap) {
        return;
    }
    int r = snprintf(buf + *len, cap - *len, "%s", s);
    if (r > 0) {
        *len += (size_t)r;
    }
    if (*len >= cap) {
        *len = cap - 1;
    }
}

void wc_scene_device_detail_on_enter(void *context) {
    WcApp *app = context;
    widget_reset(app->widget);
    WcCensus *c = app->browse_census;
    if (c && app->selected_device < c->count) {
        const WcSignature *d = &c->devices[app->selected_device];
        const char *vendor = wc_signature_vendor(d);
        bool markable = !d->mac_random || d->ssid_count > 0;

        char buf[320];
        size_t len = 0;
        char line[96];
        snprintf(line, sizeof(line), "%s%s%s\n", wc_device_type_name(d->type),
                 vendor[0] ? " - " : "", vendor);
        detail_catf(buf, &len, sizeof(buf), line);
        snprintf(line, sizeof(line), "%02X:%02X:%02X:%02X:%02X:%02X (%s)\n", d->mac[0], d->mac[1],
                 d->mac[2], d->mac[3], d->mac[4], d->mac[5], d->mac_random ? "random" : "stable");
        detail_catf(buf, &len, sizeof(buf), line);
        snprintf(line, sizeof(line), "RSSI %d  seen %lu\n", (int)d->rssi_max,
                 (unsigned long)d->obs_count);
        detail_catf(buf, &len, sizeof(buf), line);

        if (d->ssid_count > 0) {
            detail_catf(buf, &len, sizeof(buf), "Asks for:");
            for (uint8_t i = 0; i < d->ssid_count; i++) {
                snprintf(line, sizeof(line), "\n %s", d->ssids[i]);
                detail_catf(buf, &len, sizeof(buf), line);
            }
        } else {
            detail_catf(buf, &len, sizeof(buf), "Asks for: (wildcard only)");
        }
        if (!markable) {
            detail_catf(buf, &len, sizeof(buf),
                        "\nRandom MAC, no named net:\ncan't save. Tag its network.");
        }

        widget_add_text_scroll_element(app->widget, 0, 0, 128, 52, buf);
        if (markable) {
            widget_add_button_element(app->widget, GuiButtonTypeCenter, "Mark known",
                                      device_detail_button_cb, app);
        }
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewWidget);
}

bool wc_scene_device_detail_on_event(void *context, SceneManagerEvent event) {
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventButton) {
        WcApp *app = context;
        app->mark_mode = WcMarkDevice;
        scene_manager_next_scene(app->scene_manager, WcSceneMarkLabel);
        return true;
    }
    return false;
}

void wc_scene_device_detail_on_exit(void *context) {
    WcApp *app = context;
    widget_reset(app->widget);
}

// ---------------------------------------------------------------------------
// Networks sought (directed SSIDs devices are probing for)
// ---------------------------------------------------------------------------

static void network_label(const void *context, uint32_t index, char *out, size_t cap) {
    const WcApp *app = context;
    if (index >= app->list_count) {
        out[0] = '\0';
        return;
    }
    snprintf(out, cap, "%s (%u)", app->list_names[index], app->list_counts[index]);
}

void wc_scene_networks_on_enter(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
    wc_scroll_list_set_header(app->list_view, "Networks (select to tag)");
    app->list_count = 0;
    bool tallied = false;
    if (app->browse_census) {
        WcSsidTally *t = malloc(sizeof(WcSsidTally) * WC_MAX_NAMED);
        if (t) {
            tallied = true;
            // A device probes up to two networks, so there can be more SSIDs than slots.
            uint16_t n = wc_census_ssid_tally(app->browse_census, t, WC_MAX_NAMED);
            app->list_overflow = (n > WC_MAX_NAMED) ? (uint16_t)(n - WC_MAX_NAMED) : 0;
            for (uint16_t i = 0; i < n && i < WC_MAX_NAMED; i++) {
                strncpy(app->list_names[i], t[i].ssid, WC_TEXT_BUF_SIZE - 1);
                app->list_names[i][WC_TEXT_BUF_SIZE - 1] = '\0';
                app->list_counts[i] = t[i].devices;
                app->list_count++;
            }
            free(t);
        }
    }
    if (app->list_count > 0) {
        wc_scroll_list_note_hidden(app->list_view, app->list_overflow);
        wc_scroll_list_add_generated(app->list_view, app->list_count, network_label, wc_list_cb,
                                     app);
    } else {
        wc_scroll_list_add_item(app->list_view, tallied ? "(none sought)" : "(out of memory)",
                                WC_MAX_LIST, wc_list_cb, app);
    }
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewList);
}

bool wc_scene_networks_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event < app->list_count) {
        strncpy(app->selected_ssid, app->list_names[event.event], WC_SSID_MAX_LEN);
        app->selected_ssid[WC_SSID_MAX_LEN] = '\0';
        app->mark_mode = WcMarkSsid;
        scene_manager_next_scene(app->scene_manager, WcSceneMarkLabel);
        return true;
    }
    return false;
}

void wc_scene_networks_on_exit(void *context) {
    WcApp *app = context;
    wc_scroll_list_reset(app->list_view);
}

// ---------------------------------------------------------------------------
// Rename
// ---------------------------------------------------------------------------

void wc_scene_rename_on_enter(void *context) {
    WcApp *app = context;
    // Prefill with the current stem (without the extension).
    size_t el = strlen(WC_CAP_EXT);
    size_t n = strlen(app->selected_file);
    size_t stem = (n > el) ? n - el : n;
    snprintf(app->text_buf, sizeof(app->text_buf), "%.*s", (int)stem, app->selected_file);
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "New name");
    text_input_set_result_callback(app->text_input, wc_text_input_cb, app, app->text_buf,
                                   WC_BASENAME_MAX, false);
    text_input_set_minimum_length(app->text_input, 1);
    view_dispatcher_switch_to_view(app->view_dispatcher, WcViewTextInput);
}

bool wc_scene_rename_on_event(void *context, SceneManagerEvent event) {
    WcApp *app = context;
    if (event.type == SceneManagerEventTypeCustom && event.event == WcCustomEventTextDone) {
        char old_csv[WC_TEXT_BUF_SIZE + 8], new_bin[WC_TEXT_BUF_SIZE + 8],
            new_csv[WC_TEXT_BUF_SIZE + 8];
        csv_name_of(app->selected_file, old_csv, sizeof(old_csv));
        snprintf(new_bin, sizeof(new_bin), "%s%s", app->text_buf, WC_CAP_EXT);
        snprintf(new_csv, sizeof(new_csv), "%s%s", app->text_buf, WC_CSV_EXT);
        if (!app->store.rename_file(app->store.self, app->selected_file, new_bin)) {
            wc_show_message(app, "Could not rename it.\n\nThe new name may be\ntaken, or the SD "
                                 "may be\nabsent or write\nprotected.");
            return true;
        }
        app->store.rename_file(app->store.self, old_csv, new_csv); // sidecar may not exist
        scene_manager_search_and_switch_to_another_scene(app->scene_manager, WcSceneFiles);
        return true;
    }
    return false;
}

void wc_scene_rename_on_exit(void *context) {
    WcApp *app = context;
    text_input_reset(app->text_input);
}
