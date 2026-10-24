#include "../sigroam.h"

#include "../src/sr_dialect.h"
#include "../src/sr_marauder_join.h"
#include "../src/sr_marauder_upload.h"
#include "../src/sr_scan_ctl.h"
#include "../src/sr_uart_cap.h"

#include <stdio.h>
#include <string.h>

/*
 * Factory Marauder only. Joins the selected Wi-Fi with `join -a -p`.
 * Marauder manages up to five saved profiles. Does not send `upload` itself.
 *
 * Sequence: stop, scanall for one full dual-band sweep (51 x 250 ms),
 * stop, then list -a. Stopping at the first AP only covers channels 1..8.
 * The post-stop list is the one shown. StopScan does not clear access_points.
 *
 * Connected is not a timer. The next free command slot sends `info` as a
 * non-disconnecting barrier. No failure before its echo plus the selected
 * `Using SSID:` implies joinWiFi reached WL_CONNECTED in v1.17.0. The CLI
 * does not report whether the credential was persisted.
 */

enum {
    JoinEvPick = 3,
    JoinEvPass = 4,
    JoinEvAdd = 5,
    JoinEvList = 9,
};

enum {
    PhBlocked = 0,
    PhStop1,
    PhScan,
    PhList1,
    PhStop2,
    PhList2,
    PhPick,
    PhPass,
    PhVerify,
    PhJoin,
    PhJoined,
    PhFail,
    PhSaved,
    PhSavedList,
    PhStopBack,
};

enum {
    SrJoinRowInfo = 0,
    SrJoinRowSaved,
    SrJoinRowScan,
    SrJoinRowAp,
    SrJoinRowAdd,
};

enum { JOIN_ROWS = 20 };
enum { JOIN_TEXT = 40 };

enum {
    PlaceKeep = 0,
    PlaceScan,
    PlaceAp,
};

typedef struct {
    uint16_t index;
    int8_t rssi;
    bool saved_only;
    char ssid[SR_JOIN_SSID_MAX + 1u];
} JoinAp;

enum { JOIN_SAVED_CAP = 5 };

static char s_saved[JOIN_SAVED_CAP][SR_JOIN_SSID_MAX + 1u];
static size_t s_saved_n;

static unsigned s_phase;
static uint32_t s_phase_ms;
static uint32_t s_stop_rev;
static uint32_t s_hunt_ms;
static bool s_need_send;
static bool s_check_queued;
static bool s_check_sent;
static char s_pending[SR_WORKER_CMD_MAX];
static char s_psk[SR_JOIN_PASS_MAX + 1u];
static char s_why[24];
static JoinAp s_shown[SR_JOIN_AP_CAP + JOIN_SAVED_CAP];
static size_t s_shown_n;
static size_t s_cursor;
static uint32_t s_fp;
static uint8_t s_place;
static uint8_t s_line_n;
static uint8_t s_line_cur;
static uint32_t s_menu_sig;
static uint8_t s_menu_n;
static uint8_t s_ok_index;
static char s_hold[SR_JOIN_SSID_MAX + 1u];
static char s_line_text[JOIN_ROWS][JOIN_TEXT + 1u];
static uint8_t s_line_kind[JOIN_ROWS];
static uint8_t s_line_ap[JOIN_ROWS];
static uint8_t s_menu_line[JOIN_ROWS];

static bool join_known(const char* ssid);

static void join_wipe(char* p, size_t n) {
    size_t i;

    if(p == NULL) {
        return;
    }
    for(i = 0u; i < n; i++) {
        p[i] = '\0';
    }
}

static void join_fail(SigRoamApp* app, const char* why) {
    size_t i;

    s_phase = PhFail;
    s_need_send = false;
    join_wipe(s_psk, sizeof(s_psk));
    join_wipe(s_pending, sizeof(s_pending));
    for(i = 0u; i < sizeof(s_why); i++) {
        s_why[i] = '\0';
    }
    if(why != NULL) {
        for(i = 0u; i + 1u < sizeof(s_why) && why[i] != '\0'; i++) {
            s_why[i] = why[i];
        }
    }
    sr_join_listen(SrJoinListenOff);
    sr_uart_cap_mark("fap-not-connected");
    (void)app;
}

static void join_queue(const char* cmd) {
    size_t n = 0u;

    if(cmd == NULL) {
        s_need_send = false;
        return;
    }
    while(cmd[n] != '\0') {
        n++;
    }
    if(n == 0u || n >= sizeof(s_pending)) {
        s_need_send = false;
        return;
    }
    memcpy(s_pending, cmd, n);
    s_pending[n] = '\0';
    s_need_send = true;
}

static void join_copy_listen(JoinAp* dst, size_t* nout) {
    size_t n;
    size_t i;

    n = sr_join_ap_count();
    if(n > (size_t)SR_JOIN_AP_CAP) {
        n = (size_t)SR_JOIN_AP_CAP;
    }
    for(i = 0u; i < n; i++) {
        int8_t rssi = 0;

        dst[i].saved_only = false;
        dst[i].rssi = 0;
        if(!sr_join_ap_at(i, &dst[i].index, dst[i].ssid, sizeof(dst[i].ssid))) {
            n = i;
            break;
        }
        if(sr_join_ap_rssi(i, &rssi)) {
            dst[i].rssi = rssi;
        }
    }
    *nout = n;
}

static bool join_known(const char* ssid) {
    size_t i;

    if(ssid == NULL || ssid[0] == '\0') {
        return false;
    }
    for(i = 0u; i < s_saved_n; i++) {
        if(strcmp(s_saved[i], ssid) == 0) {
            return true;
        }
    }
    return false;
}

static void join_remember(const char* ssid) {
    size_t n;
    size_t i;

    if(ssid == NULL || ssid[0] == '\0' || join_known(ssid)) {
        return;
    }
    n = 0u;
    while(ssid[n] != '\0' && n < (size_t)SR_JOIN_SSID_MAX) {
        n++;
    }
    if(s_saved_n >= (size_t)JOIN_SAVED_CAP) {
        return;
    }
    for(i = 0u; i < n; i++) {
        s_saved[s_saved_n][i] = ssid[i];
    }
    s_saved[s_saved_n][n] = '\0';
    s_saved_n++;
}

static uint32_t join_fp(const JoinAp* row, size_t n) {
    uint32_t h = 2166136261u;
    size_t i;
    size_t c;

    h ^= (uint32_t)n;
    for(i = 0u; i < n; i++) {
        h = (h ^ (uint8_t)row[i].rssi) * 16777619u;
        h ^= row[i].saved_only ? 1u : 0u;
        for(c = 0u; row[i].ssid[c] != '\0'; c++) {
            h = (h ^ (uint8_t)row[i].ssid[c]) * 16777619u;
        }
    }
    return h;
}

static void join_publish(void) {
    JoinAp tmp[SR_JOIN_AP_CAP];
    JoinAp out[SR_JOIN_AP_CAP + JOIN_SAVED_CAP];
    size_t n = 0u;
    size_t i;
    size_t out_n;
    uint32_t fp;

    join_copy_listen(tmp, &n);
    for(i = 1u; i < n; i++) {
        JoinAp key = tmp[i];
        size_t j = i;

        while(j > 0u && tmp[j - 1u].rssi < key.rssi) {
            tmp[j] = tmp[j - 1u];
            j--;
        }
        tmp[j] = key;
    }
    out_n = n;
    for(i = 0u; i < n; i++) {
        out[i] = tmp[i];
    }
    fp = join_fp(out, out_n);
    if(fp == s_fp) {
        return;
    }
    s_hold[0] = '\0';
    if(s_line_cur < s_line_n && s_line_kind[s_line_cur] == (uint8_t)SrJoinRowAp &&
       s_line_ap[s_line_cur] < s_shown_n) {
        size_t c = 0u;

        while(s_shown[s_line_ap[s_line_cur]].ssid[c] != '\0' && c < (size_t)SR_JOIN_SSID_MAX) {
            s_hold[c] = s_shown[s_line_ap[s_line_cur]].ssid[c];
            c++;
        }
        s_hold[c] = '\0';
    }
    s_fp = fp;
    for(i = 0u; i < out_n; i++) {
        s_shown[i] = out[i];
    }
    s_shown_n = out_n;
    s_place = (uint8_t)PlaceAp;
}

static bool join_flush(SigRoamApp* app) {
    if(!s_need_send) {
        return true;
    }
    if(app->worker == NULL || !sr_worker_send_cmd(app->worker, s_pending)) {
        return false;
    }
    s_need_send = false;
    join_wipe(s_pending, sizeof(s_pending));
    return true;
}

static void join_push(uint8_t kind, uint8_t ap, const char* text) {
    size_t i = 0u;

    if(s_line_n >= (uint8_t)JOIN_ROWS) {
        return;
    }
    if(text != NULL) {
        while(text[i] != '\0' && i < (size_t)JOIN_TEXT) {
            s_line_text[s_line_n][i] = text[i];
            i++;
        }
    }
    s_line_text[s_line_n][i] = '\0';
    s_line_kind[s_line_n] = kind;
    s_line_ap[s_line_n] = ap;
    s_line_n++;
}

static bool join_scan_phase(unsigned phase) {
    return phase == PhStop1 || phase == PhStop2 || phase == PhScan || phase == PhList1 ||
           phase == PhList2 || phase == PhPick;
}

static const char* join_scan_status(void) {
    if(s_phase == PhStop1 || s_phase == PhStop2) {
        return "Stopping";
    }
    if(s_phase == PhScan) {
        return "Scanning";
    }
    if(s_phase == PhList1 || s_phase == PhList2) {
        return "Listing";
    }
    if(s_shown_n == 0u) {
        return "No AP";
    }
    return "Pick one";
}

static void join_place_cursor(void) {
    uint8_t i;
    uint8_t first_ap = 0u;
    bool have_ap = false;

    if(s_line_n == 0u) {
        s_line_cur = 0u;
        s_place = (uint8_t)PlaceKeep;
        return;
    }
    if(s_place == (uint8_t)PlaceScan) {
        for(i = 0u; i < s_line_n; i++) {
            if(s_line_kind[i] == (uint8_t)SrJoinRowScan) {
                s_line_cur = i;
                s_place = (uint8_t)PlaceKeep;
                return;
            }
        }
    } else if(s_place == (uint8_t)PlaceAp) {
        for(i = 0u; i < s_line_n; i++) {
            if(s_line_kind[i] != (uint8_t)SrJoinRowAp) {
                continue;
            }
            if(!have_ap) {
                first_ap = i;
                have_ap = true;
            }
            if(s_hold[0] != '\0' && s_line_ap[i] < s_shown_n &&
               strcmp(s_shown[s_line_ap[i]].ssid, s_hold) == 0) {
                s_line_cur = i;
                s_place = (uint8_t)PlaceKeep;
                return;
            }
        }
        if(have_ap) {
            s_line_cur = first_ap;
            s_place = (uint8_t)PlaceKeep;
            return;
        }
        for(i = 0u; i < s_line_n; i++) {
            if(s_line_kind[i] == (uint8_t)SrJoinRowScan) {
                s_line_cur = i;
                s_place = (uint8_t)PlaceKeep;
                return;
            }
        }
    }
    if(s_line_cur >= s_line_n) {
        s_line_cur = (uint8_t)(s_line_n - 1u);
    }
    s_place = (uint8_t)PlaceKeep;
}

static void join_label(char* dst, size_t cap, bool star, const char* ssid) {
    size_t i = 0u;
    size_t o = 0u;

    if(dst == NULL || cap == 0u) {
        return;
    }
    if(ssid == NULL || ssid[0] == '\0') {
        ssid = "(hidden)";
    }
    if(star && o + 2u < cap) {
        dst[o++] = '*';
        dst[o++] = ' ';
    }
    while(ssid[i] != '\0' && o + 1u < cap) {
        dst[o++] = ssid[i++];
    }
    dst[o] = '\0';
}

static void join_compose(void) {
    char row[JOIN_TEXT + 1u];
    size_t i;

    s_line_n = 0u;
    join_push((uint8_t)SrJoinRowInfo, 0xffu, "Wi-Fi");
    if(s_phase == PhBlocked) {
        join_push((uint8_t)SrJoinRowInfo, 0xffu, "Stop scan first");
    } else if(s_phase == PhSaved || s_phase == PhStopBack) {
        join_push((uint8_t)SrJoinRowInfo, 0xffu, s_phase == PhSaved ? "Reading" : "Stopping");
    } else if(s_phase == PhSavedList) {
        snprintf(row, sizeof(row), "Saved %u/%u", (unsigned)s_saved_n, (unsigned)JOIN_SAVED_CAP);
        join_push((uint8_t)SrJoinRowInfo, 0xffu, row);
        for(i = 0u; i < s_saved_n; i++) {
            join_label(row, sizeof(row), true, s_saved[i]);
            join_push((uint8_t)SrJoinRowSaved, (uint8_t)i, row);
        }
        join_push((uint8_t)SrJoinRowScan, 0xffu, "Scan nearby");
    } else if(s_phase == PhVerify || s_phase == PhJoin) {
        join_push(
            (uint8_t)SrJoinRowInfo, 0xffu, s_phase == PhVerify ? "Checking AP" : "Joining");
        if(s_cursor < s_shown_n) {
            join_push((uint8_t)SrJoinRowInfo, 0xffu, s_shown[s_cursor].ssid);
        }
    } else if(s_phase == PhJoined) {
        join_push((uint8_t)SrJoinRowInfo, 0xffu, "Connected");
        if(s_cursor < s_shown_n) {
            join_push((uint8_t)SrJoinRowInfo, 0xffu, s_shown[s_cursor].ssid);
        }
        join_push((uint8_t)SrJoinRowAdd, 0xffu, "Add");
    } else if(s_phase == PhFail) {
        join_push((uint8_t)SrJoinRowInfo, 0xffu, "Not connected");
        join_push((uint8_t)SrJoinRowInfo, 0xffu, s_why[0] != '\0' ? s_why : "failed");
    } else if(join_scan_phase(s_phase)) {
        join_push((uint8_t)SrJoinRowInfo, 0xffu, join_scan_status());
        for(i = 0u; i < s_shown_n; i++) {
            join_label(row, sizeof(row), join_known(s_shown[i].ssid), s_shown[i].ssid);
            join_push((uint8_t)SrJoinRowAp, (uint8_t)i, row);
        }
        if(s_phase == PhPick && s_shown_n == 0u) {
            join_push((uint8_t)SrJoinRowScan, 0xffu, "Scan nearby");
        }
    }
    join_place_cursor();
}

static uint32_t join_menu_sig(void) {
    uint32_t h = 2166136261u;
    size_t i;
    size_t c;

    h = (h ^ s_phase) * 16777619u;
    h = (h ^ s_fp) * 16777619u;
    h = (h ^ (uint32_t)s_saved_n) * 16777619u;
    for(i = 0u; i < s_saved_n; i++) {
        for(c = 0u; s_saved[i][c] != '\0'; c++) {
            h = (h ^ (uint8_t)s_saved[i][c]) * 16777619u;
        }
    }
    h = (h ^ (uint32_t)s_shown_n) * 16777619u;
    for(i = 0u; i < s_shown_n; i++) {
        h = (h ^ (uint8_t)s_shown[i].rssi) * 16777619u;
        h = (h ^ (s_shown[i].saved_only ? 1u : 0u)) * 16777619u;
        for(c = 0u; s_shown[i].ssid[c] != '\0'; c++) {
            h = (h ^ (uint8_t)s_shown[i].ssid[c]) * 16777619u;
        }
    }
    for(c = 0u; s_why[c] != '\0'; c++) {
        h = (h ^ (uint8_t)s_why[c]) * 16777619u;
    }
    return h;
}

static void join_enter(void* context, uint32_t index) {
    SigRoamApp* app = context;

    if(app == NULL) {
        return;
    }
    s_ok_index = (uint8_t)index;
    view_dispatcher_send_custom_event(app->view_dispatcher, (uint32_t)JoinEvList);
}

static void join_draw(SigRoamApp* app) {
    VariableItemList* list;
    uint8_t place;
    uint8_t selected;
    uint8_t i;
    uint8_t target;
    uint32_t sig;

    if(app == NULL || app->var_list == NULL || s_phase == PhPass) {
        return;
    }
    list = app->var_list;
    place = s_place;
    selected = variable_item_list_get_selected_item_index(list);
    sig = join_menu_sig();
    if(sig == s_menu_sig && place == (uint8_t)PlaceKeep) {
        return;
    }
    if(place == (uint8_t)PlaceKeep && selected < s_menu_n) {
        uint8_t line = s_menu_line[selected];

        if(line < s_line_n && s_line_kind[line] == (uint8_t)SrJoinRowAp &&
           s_line_ap[line] < s_shown_n) {
            size_t c = 0u;

            while(s_shown[s_line_ap[line]].ssid[c] != '\0' && c < (size_t)SR_JOIN_SSID_MAX) {
                s_hold[c] = s_shown[s_line_ap[line]].ssid[c];
                c++;
            }
            s_hold[c] = '\0';
            s_place = (uint8_t)PlaceAp;
        }
    }
    join_compose();
    s_menu_n = 0u;
    variable_item_list_reset(list);
    for(i = 0u; i < s_line_n; i++) {
        if(s_line_kind[i] == (uint8_t)SrJoinRowInfo && strcmp(s_line_text[i], "Wi-Fi") == 0) {
            continue;
        }
        if(s_menu_n >= (uint8_t)JOIN_ROWS) {
            break;
        }
        s_menu_line[s_menu_n] = i;
        s_menu_n++;
    }
    if(s_menu_n == 0u) {
        s_menu_line[0] = 0u;
        s_menu_n = 1u;
        variable_item_list_add(list, "Wi-Fi", 1u, NULL, app);
    } else {
        for(i = 0u; i < s_menu_n; i++) {
            uint8_t line = s_menu_line[i];
            VariableItem* item;
            const char* label = s_line_text[line];

            if(label[0] == '\0') {
                label = "Wi-Fi";
            }
            item = variable_item_list_add(list, label, 1u, NULL, app);
            if(s_line_kind[line] == (uint8_t)SrJoinRowAp && s_line_ap[line] < s_shown_n) {
                char rssi[8];

                snprintf(rssi, sizeof(rssi), "%d", (int)s_shown[s_line_ap[line]].rssi);
                variable_item_set_current_value_text(item, rssi);
            }
        }
    }
    variable_item_list_set_enter_callback(list, join_enter, app);
    target = 0u;
    for(i = 0u; i < s_menu_n; i++) {
        if(s_menu_line[i] == s_line_cur) {
            target = i;
        }
    }
    variable_item_list_set_selected_item(list, target);
    s_menu_sig = sig;
}

static void join_arm(unsigned phase, unsigned listen, const char* cmd, uint32_t now);

static void join_show_saved(SigRoamApp* app) {
    s_need_send = false;
    join_wipe(s_pending, sizeof(s_pending));
    sr_join_listen(SrJoinListenOff);
    s_phase = PhSavedList;
    s_place = (uint8_t)PlaceScan;
    join_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewVarList);
}

static void join_begin_scan(SigRoamApp* app) {
    s_shown_n = 0u;
    s_fp = 0u;
    s_hold[0] = '\0';
    s_place = (uint8_t)PlaceKeep;
    s_line_cur = 0u;
    s_stop_rev = app->model.wifi_stop_rev;
    s_hunt_ms = furi_get_tick();
    join_arm(PhStop1, SrJoinListenOff, sr_join_cmd_stop(), s_hunt_ms);
    join_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewVarList);
}

static void join_add_another(SigRoamApp* app) {
    s_shown_n = 0u;
    s_fp = 0u;
    s_hold[0] = '\0';
    s_cursor = 0u;
    s_place = (uint8_t)PlaceKeep;
    join_arm(PhSaved, SrJoinListenSaved, "sigroam wifi\n", furi_get_tick());
    join_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewVarList);
}

static bool join_psk_validator(const char* text, FuriString* error, void* context) {
    (void)context;
    if(!sr_join_password_ok(text)) {
        if(error != NULL) {
            furi_string_set(error, "bad password");
        }
        return false;
    }
    return true;
}

static void join_psk_done(void* context) {
    SigRoamApp* app = context;

    if(app != NULL) {
        view_dispatcher_send_custom_event(app->view_dispatcher, JoinEvPass);
    }
}

static void join_show_pass(SigRoamApp* app) {
    s_phase = PhPass;
    join_wipe(s_psk, sizeof(s_psk));
    text_input_reset(app->text_input);
    text_input_set_header_text(app->text_input, "WiFi password");
    text_input_set_minimum_length(app->text_input, (size_t)SR_JOIN_PASS_MIN);
    text_input_set_result_callback(
        app->text_input, join_psk_done, app, s_psk, sizeof(s_psk), false);
    text_input_set_validator(app->text_input, join_psk_validator, NULL);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewTextInput);
}

static void join_show_widget(SigRoamApp* app) {
    join_draw(app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SigRoamViewVarList);
}

static void join_arm(unsigned phase, unsigned listen, const char* cmd, uint32_t now) {
    s_phase = phase;
    s_phase_ms = now;
    sr_join_listen(listen);
    join_queue(cmd);
}

static bool join_aged(uint32_t now, uint32_t ms) {
    return (uint32_t)(now - s_phase_ms) >= ms;
}

void sigroam_scene_join_on_enter(void* context) {
    SigRoamApp* app = context;

    s_need_send = false;
    s_check_queued = false;
    s_check_sent = false;
    s_shown_n = 0u;
    s_cursor = 0u;
    s_fp = 0u;
    s_place = (uint8_t)PlaceKeep;
    s_line_n = 0u;
    s_line_cur = 0u;
    s_menu_sig = 0xffffffffu;
    s_menu_n = 0u;
    s_ok_index = 0u;
    s_hold[0] = '\0';
    s_saved_n = 0u;
    s_why[0] = '\0';
    join_wipe(s_psk, sizeof(s_psk));
    join_wipe(s_pending, sizeof(s_pending));
    sr_join_listen(SrJoinListenOff);
    if(!sr_dialect_is_generic_marauder(&app->model.firmware) ||
       app->model.session == SrSessionRunning) {
        s_phase = PhBlocked;
    } else {
        join_arm(PhSaved, SrJoinListenSaved, "sigroam wifi\n", furi_get_tick());
    }
    join_show_widget(app);
}

bool sigroam_scene_join_on_event(void* context, SceneManagerEvent event) {
    SigRoamApp* app = context;
    uint32_t now;

    if(event.type == SceneManagerEventTypeBack) {
        if(s_phase == PhPass) {
            s_phase = PhPick;
            s_place = (uint8_t)PlaceKeep;
            join_wipe(s_psk, sizeof(s_psk));
            join_show_widget(app);
            return true;
        }
        if(s_phase == PhSavedList || s_phase == PhBlocked || s_phase == PhJoined ||
           s_phase == PhSaved) {
            return false;
        }
        if(join_scan_phase(s_phase) || s_phase == PhStopBack || s_phase == PhVerify ||
           s_phase == PhJoin) {
            s_stop_rev = app->model.wifi_stop_rev;
            join_arm(PhStopBack, SrJoinListenOff, sr_join_cmd_stop(), furi_get_tick());
            join_show_widget(app);
            return true;
        }
        join_show_saved(app);
        return true;
    }
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == (uint32_t)JoinEvList || event.event == (uint32_t)JoinEvAdd) {
            uint8_t line = 0u;
            uint8_t kind = 0xffu;

            if(s_ok_index < s_menu_n) {
                line = s_menu_line[s_ok_index];
            }
            if(line < s_line_n) {
                kind = s_line_kind[line];
            }
            if(kind == (uint8_t)SrJoinRowAdd ||
               (s_phase == PhJoined && event.event == (uint32_t)JoinEvAdd)) {
                join_add_another(app);
                return true;
            }
            if(kind == (uint8_t)SrJoinRowScan &&
               (s_phase == PhSavedList || s_phase == PhPick)) {
                join_begin_scan(app);
                return true;
            }
            if(kind == (uint8_t)SrJoinRowAp && s_line_ap[line] < s_shown_n &&
               s_phase != PhBlocked && s_phase != PhSaved && s_phase != PhFail) {
                s_cursor = s_line_ap[line];
                join_show_pass(app);
                return true;
            }
            return true;
        }
        if(s_phase == PhPass && event.event == JoinEvPass) {
            if(!sr_join_password_ok(s_psk)) {
                join_fail(app, "Bad password");
                join_show_widget(app);
                return true;
            }
            join_arm(PhVerify, SrJoinListenList, sr_join_cmd_list(), furi_get_tick());
            join_show_widget(app);
            return true;
        }
        return false;
    }
    if(event.type != SceneManagerEventTypeTick) {
        return false;
    }
    now = furi_get_tick();
    if(s_phase == PhPass || s_phase == PhBlocked || s_phase == PhJoined || s_phase == PhFail ||
       s_phase == PhPick || s_phase == PhSavedList) {
        return true;
    }
    if(s_phase == PhJoin && sr_join_saw_using() &&
       !sr_join_using_matches(s_shown[s_cursor].ssid)) {
        join_fail(app, "AP changed");
        join_draw(app);
        return true;
    }
    if(s_phase == PhJoin && sr_join_receipt() == SrJoinReceiptFail) {
        join_fail(app, sr_join_fail_why(SrJoinReceiptFail));
        join_draw(app);
        return true;
    }
    if(s_phase == PhJoin && join_aged(now, (uint32_t)SR_JOIN_CHECK_TIMEOUT_MS)) {
        join_fail(app, "No join receipt");
        join_draw(app);
        return true;
    }
    if(!join_flush(app)) {
        join_draw(app);
        return true;
    }
    if(s_phase == PhSaved) {
        size_t i;
        size_t n;

        if(!join_flush(app)) {
            join_draw(app);
            return true;
        }
        if(sr_join_saved_end() || join_aged(now, 1500u)) {
            n = sr_join_saved_count();
            if(n > (size_t)JOIN_SAVED_CAP) {
                n = (size_t)JOIN_SAVED_CAP;
            }
            for(i = 0u; i < n; i++) {
                s_saved[i][0] = '\0';
                (void)sr_join_saved_at(i, s_saved[i], sizeof(s_saved[i]));
            }
            s_saved_n = n;
            sr_join_listen(SrJoinListenOff);
            s_need_send = false;
            s_phase = PhSavedList;
            s_place = (uint8_t)PlaceScan;
        }
        join_draw(app);
        return true;
    }
    if(s_phase == PhStopBack) {
        if(app->model.wifi_stop_rev != s_stop_rev || join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            join_show_saved(app);
            return true;
        }
        join_draw(app);
        return true;
    }
    if(s_phase == PhStop1) {
        if(app->model.wifi_stop_rev != s_stop_rev || join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            s_hunt_ms = now;
            join_arm(PhScan, SrJoinListenScan, sr_join_cmd_scan(), now);
        }
    } else if(s_phase == PhScan) {
        if(sr_join_saw_scan() || join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            join_arm(PhList1, SrJoinListenList, sr_join_cmd_list(), now);
        }
    } else if(s_phase == PhList1) {
        if(sr_join_saw_list_end() || join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            /* 2.4 GHz is the first 14 hops. Show that list; keep hopping 5 GHz. */
            if((uint32_t)(now - s_hunt_ms) >= (uint32_t)SR_JOIN_SHOW_MS && sr_join_ap_count() > 0u) {
                join_publish();
            }
            if((uint32_t)(now - s_hunt_ms) < (uint32_t)SR_JOIN_SWEEP_MS) {
                s_phase = PhScan;
                s_phase_ms = now;
                s_need_send = false;
                sr_join_listen(SrJoinListenOff);
            } else {
                s_stop_rev = app->model.wifi_stop_rev;
                join_arm(PhStop2, SrJoinListenOff, sr_join_cmd_stop(), now);
            }
        }
    } else if(s_phase == PhStop2) {
        if(app->model.wifi_stop_rev != s_stop_rev || join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            join_arm(PhList2, SrJoinListenList, sr_join_cmd_list(), now);
        }
    } else if(s_phase == PhList2) {
        if(sr_join_saw_list_end() || join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            if(sr_join_ap_count() == 0u && s_saved_n == 0u) {
                sr_join_listen(SrJoinListenOff);
                join_fail(app, "No fresh AP list");
            } else {
                join_publish();
                sr_join_listen(SrJoinListenOff);
                s_phase = PhPick;
            }
        }
    } else if(s_phase == PhVerify) {
        if(sr_join_saw_list_end()) {
            if(!sr_join_ap_matches(s_shown[s_cursor].index, s_shown[s_cursor].ssid)) {
                join_fail(app, "AP changed");
            } else if(sr_join_build(s_shown[s_cursor].index, s_psk, s_pending,
                                    sizeof(s_pending)) == 0u) {
                join_fail(app, "Bad password");
            } else {
                join_wipe(s_psk, sizeof(s_psk));
                s_need_send = true;
                s_check_queued = false;
                s_check_sent = false;
                s_phase = PhJoin;
                s_phase_ms = now;
                sr_uart_cap_arm(true);
                sr_uart_cap_mark("join");
                sr_join_listen(SrJoinListenJoin);
                sr_join_expect_index(s_shown[s_cursor].index);
            }
        } else if(join_aged(now, (uint32_t)SR_SCAN_CTL_TIMEOUT_MS)) {
            join_fail(app, "List timeout");
        }
    } else if(s_phase == PhJoin) {
        unsigned receipt = sr_join_receipt();

        if(!s_check_sent) {
            if(!s_check_queued) {
                sr_join_expect_check();
                join_queue(sr_join_cmd_check());
                s_check_queued = true;
            } else if(!s_need_send) {
                s_check_sent = true;
            }
        } else if(receipt == SrJoinReceiptJoined) {
            sr_join_listen(SrJoinListenOff);
            sr_uart_cap_mark("fap-connected");
            sr_mau_up_note_joined(s_shown[s_cursor].ssid);
            join_remember(s_shown[s_cursor].ssid);
            s_phase = PhJoined;
        }
    }
    join_draw(app);
    return true;
}

void sigroam_scene_join_on_exit(void* context) {
    SigRoamApp* app = context;

    if(app != NULL && app->mtx != NULL && furi_mutex_acquire(app->mtx, FuriWaitForever) == FuriStatusOk) {
        sr_join_listen(SrJoinListenOff);
        furi_mutex_release(app->mtx);
    } else {
        sr_join_listen(SrJoinListenOff);
    }
    s_need_send = false;
    join_wipe(s_psk, sizeof(s_psk));
    join_wipe(s_pending, sizeof(s_pending));
    if(app != NULL) {
        if(app->var_list != NULL) {
            variable_item_list_reset(app->var_list);
        }
        widget_reset(app->widget);
        if(app->submenu != NULL) {
            submenu_reset(app->submenu);
        }
        if(app->text_input != NULL) {
            text_input_reset(app->text_input);
        }
    }
}
