#include "../debug_mode.h"
#include <nfc/helpers/iso13239_crc.h>
#include <string.h>

#define DEBUG_MODE_NFC_FWT_FC (100000U)
#define DEBUG_MODE_PICOPASS_UID_LEN (8U)
#define DEBUG_MODE_PICOPASS_CMD_ACTALL (0x0AU)
#define DEBUG_MODE_PICOPASS_CMD_READ_OR_IDENTIFY (0x0CU)
#define DEBUG_MODE_PICOPASS_CMD_SELECT (0x81U)

typedef struct {
    DebugMode* app;
    BitBuffer* tx_buffer;
    BitBuffer* rx_buffer;
    bool stop_requested;
    bool cancelled;
    bool nfc_running;
} DebugModeNfcWorkerContext;

static void debug_mode_nfc_uid_to_string(
    const char* prefix,
    const uint8_t* uid,
    size_t uid_len,
    char* out,
    size_t out_size) {
    size_t pos = snprintf(out, out_size, "%s:", prefix);
    for(size_t i = 0; i < uid_len && pos + 2 < out_size; i++) {
        pos += snprintf(out + pos, out_size - pos, "%02X", uid[i]);
    }
    out[pos] = '\0';
}

static bool debug_mode_nfc_picopass_send_crc_frame(
    DebugModeNfcWorkerContext* worker,
    BitBuffer* tx_buffer,
    BitBuffer* rx_buffer) {
    NfcError error = nfc_poller_trx(worker->app->nfc, tx_buffer, rx_buffer, DEBUG_MODE_NFC_FWT_FC);
    if(error != NfcErrorNone) return false;
    if(!iso13239_crc_check(Iso13239CrcTypePicopass, rx_buffer)) return false;
    iso13239_crc_trim(rx_buffer);
    return true;
}

static bool debug_mode_nfc_try_picopass_csn(DebugModeNfcWorkerContext* worker) {
    uint8_t col_res[DEBUG_MODE_PICOPASS_UID_LEN] = {};
    uint8_t serial[DEBUG_MODE_PICOPASS_UID_LEN] = {};

    bit_buffer_reset(worker->tx_buffer);
    bit_buffer_reset(worker->rx_buffer);
    bit_buffer_append_byte(worker->tx_buffer, DEBUG_MODE_PICOPASS_CMD_ACTALL);

    NfcError error = nfc_poller_trx(
        worker->app->nfc, worker->tx_buffer, worker->rx_buffer, DEBUG_MODE_NFC_FWT_FC);
    if(error != NfcErrorIncompleteFrame) return false;

    bit_buffer_reset(worker->tx_buffer);
    bit_buffer_reset(worker->rx_buffer);
    bit_buffer_append_byte(worker->tx_buffer, DEBUG_MODE_PICOPASS_CMD_READ_OR_IDENTIFY);
    if(!debug_mode_nfc_picopass_send_crc_frame(worker, worker->tx_buffer, worker->rx_buffer)) {
        return false;
    }
    if(bit_buffer_get_size_bytes(worker->rx_buffer) != DEBUG_MODE_PICOPASS_UID_LEN) return false;
    bit_buffer_write_bytes(worker->rx_buffer, col_res, sizeof(col_res));

    bit_buffer_reset(worker->tx_buffer);
    bit_buffer_reset(worker->rx_buffer);
    bit_buffer_append_byte(worker->tx_buffer, DEBUG_MODE_PICOPASS_CMD_SELECT);
    bit_buffer_append_bytes(worker->tx_buffer, col_res, sizeof(col_res));
    if(!debug_mode_nfc_picopass_send_crc_frame(worker, worker->tx_buffer, worker->rx_buffer)) {
        return false;
    }
    if(bit_buffer_get_size_bytes(worker->rx_buffer) != DEBUG_MODE_PICOPASS_UID_LEN) return false;
    bit_buffer_write_bytes(worker->rx_buffer, serial, sizeof(serial));

    debug_mode_nfc_uid_to_string("PICO", serial, sizeof(serial), worker->app->nfc_uid, sizeof(worker->app->nfc_uid));
    return true;
}

static NfcCommand debug_mode_nfc_picopass_callback(NfcEvent event, void* context) {
    DebugModeNfcWorkerContext* worker = context;
    if(worker->stop_requested) return NfcCommandStop;

    if(event.type == NfcEventTypePollerReady) {
        if(debug_mode_nfc_try_picopass_csn(worker)) {
            worker->app->nfc_scan_found = true;
            worker->app->nfc_scan_active = false;
            if(!worker->cancelled) {
                view_dispatcher_send_custom_event(
                    worker->app->view_dispatcher, DebugModeCustomEventNfcFound);
            }
            return NfcCommandStop;
        }
        furi_delay_ms(100);
    }

    return NfcCommandContinue;
}

static void debug_mode_nfc_start_picopass(DebugModeNfcWorkerContext* worker) {
    worker->stop_requested = false;
    worker->tx_buffer = bit_buffer_alloc(32);
    worker->rx_buffer = bit_buffer_alloc(32);

    nfc_config(worker->app->nfc, NfcModePoller, NfcTechIso15693);
    nfc_set_guard_time_us(worker->app->nfc, 10000);
    nfc_set_fdt_poll_fc(worker->app->nfc, 5000);
    nfc_set_fdt_poll_poll_us(worker->app->nfc, 1000);
    nfc_start(worker->app->nfc, debug_mode_nfc_picopass_callback, worker);
    worker->nfc_running = true;
}

static void debug_mode_scene_nfc_scan_start_worker(DebugMode* app) {
    app->nfc_scan_active = true;
    app->nfc_scan_found = false;
    app->nfc_scan_frame = 0;
    app->nfc_uid[0] = '\0';
    notification_message(app->notifications, &sequence_blink_start_cyan);

    DebugModeNfcWorkerContext* worker = malloc(sizeof(DebugModeNfcWorkerContext));
    furi_assert(worker);
    worker->app = app;
    worker->tx_buffer = NULL;
    worker->rx_buffer = NULL;
    worker->stop_requested = false;
    worker->cancelled = false;
    worker->nfc_running = false;
    app->nfc_worker_context = worker;
    debug_mode_nfc_start_picopass(worker);
}

static void debug_mode_scene_nfc_scan_draw(DebugMode* app) {
    static const char* dots[] = {"Scanning.", "Scanning..", "Scanning..."};
    widget_reset(app->widget);
    widget_add_string_element(
        app->widget,
        64,
        12,
        AlignCenter,
        AlignCenter,
        FontPrimary,
        app->nfc_mode == DebugModeNfcModeLink ? "Link Card" : "Scan Card");
    widget_add_string_element(
        app->widget,
        64,
        30,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        dots[app->nfc_scan_frame % COUNT_OF(dots)]);
    widget_add_string_multiline_element(
        app->widget,
        64,
        48,
        AlignCenter,
        AlignCenter,
        FontSecondary,
        app->nfc_mode == DebugModeNfcModeLink ? "Hold card to back\nBack cancels" :
                                                "Hold linked card\nBack cancels");
}

static void debug_mode_scene_nfc_scan_stop_worker(DebugMode* app) {
    DebugModeNfcWorkerContext* worker = app->nfc_worker_context;

    if(furi_timer_is_running(app->nfc_scan_timer)) {
        furi_timer_stop(app->nfc_scan_timer);
    }
    app->nfc_scan_active = false;
    notification_message(app->notifications, &sequence_blink_stop);

    if(worker) {
        worker->cancelled = true;
        worker->stop_requested = true;
        if(worker->nfc_running) {
            nfc_stop(app->nfc);
            worker->nfc_running = false;
        }
        if(worker->tx_buffer) {
            bit_buffer_free(worker->tx_buffer);
            worker->tx_buffer = NULL;
        }
        if(worker->rx_buffer) {
            bit_buffer_free(worker->rx_buffer);
            worker->rx_buffer = NULL;
        }
        free(worker);
        app->nfc_worker_context = NULL;
    }
}

void debug_mode_scene_nfc_scan_on_enter(void* context) {
    DebugMode* app = context;

    debug_mode_scene_nfc_scan_start_worker(app);
    debug_mode_scene_nfc_scan_draw(app);
    furi_timer_start(app->nfc_scan_timer, furi_ms_to_ticks(350));
    view_dispatcher_switch_to_view(app->view_dispatcher, DebugModeViewIdWidget);
}

bool debug_mode_scene_nfc_scan_on_event(void* context, SceneManagerEvent event) {
    DebugMode* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom &&
       event.event == DebugModeCustomEventNfcFound) {
        debug_mode_scene_nfc_scan_stop_worker(app);
        notification_message(app->notifications, &sequence_success);

        if(app->nfc_mode == DebugModeNfcModeLink) {
            bool ok = case_store_link_nfc_uid(app->storage, app->nfc_uid, app->current_case_id);
            snprintf(
                app->message_title,
                sizeof(app->message_title),
                "%s",
                ok ? "Tag Linked" : "Link Failed");
            snprintf(
                app->message_body,
                sizeof(app->message_body),
                "%s",
                ok ? "Card linked.\n\nThis card now opens this case." :
                     "Could not save the card link.");
            app->message_return_scene = DebugModeSceneCaseView;
            scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
        } else {
            uint32_t linked_case_id = 0;
            if(case_store_find_case_by_nfc_uid(app->storage, app->nfc_uid, &linked_case_id) &&
               case_store_read_header(app->storage, linked_case_id, &(CaseHeader){0})) {
                app->current_case_id = linked_case_id;
                scene_manager_next_scene(app->scene_manager, DebugModeSceneCaseView);
            } else {
                snprintf(app->message_title, sizeof(app->message_title), "No Link Found");
                snprintf(
                    app->message_body,
                    sizeof(app->message_body),
                    "This card is not linked to a Debug Mode case.");
                app->message_return_scene = DebugModeSceneMainMenu;
                scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
            }
        }

        consumed = true;
    } else if(
        event.type == SceneManagerEventTypeCustom &&
        event.event == DebugModeCustomEventNfcTimeout) {
        debug_mode_scene_nfc_scan_stop_worker(app);

        snprintf(app->message_title, sizeof(app->message_title), "No Tag Read");
        snprintf(
            app->message_body,
            sizeof(app->message_body),
            "No card was detected.\n\nHold it flat against the back and try again.");
        app->message_return_scene =
            app->nfc_mode == DebugModeNfcModeLink ? DebugModeSceneCaseView : DebugModeSceneMainMenu;
        scene_manager_next_scene(app->scene_manager, DebugModeSceneMessage);
        consumed = true;
    } else if(
        event.type == SceneManagerEventTypeCustom &&
        event.event == DebugModeCustomEventNfcScanTick) {
        if(app->nfc_worker_context) {
            app->nfc_scan_frame++;
            if(app->nfc_scan_frame > 30) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, DebugModeCustomEventNfcTimeout);
            } else {
                debug_mode_scene_nfc_scan_draw(app);
            }
        }
        consumed = true;
    }

    return consumed;
}

void debug_mode_scene_nfc_scan_on_exit(void* context) {
    DebugMode* app = context;
    debug_mode_scene_nfc_scan_stop_worker(app);
    widget_reset(app->widget);
}
