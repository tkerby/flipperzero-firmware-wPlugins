#include "../mizip_balance_editor_i.h"

static NfcCommand
    mizip_balance_editor_scene_scanner_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfClassic);
    MiZipBalanceEditorApp* app = context;
    NfcCommand command = NfcCommandContinue;

    const MfClassicPollerEvent* mfc_event = event.event_data;
    if(mfc_event->type == MfClassicPollerEventTypeCardDetected) {
        FURI_LOG_D("MiZipBalanceEditor", "Card detected");
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventCardDetected);
    } else if(mfc_event->type == MfClassicPollerEventTypeCardLost) {
        FURI_LOG_D("MiZipBalanceEditor", "Card lost");
        command = NfcCommandStop;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventCardLost);
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        FURI_LOG_D("MiZipBalanceEditor", "Poller request mode");
        mfc_event->data->poller_mode.mode = MfClassicPollerModeRead;
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        FURI_LOG_D("MiZipBalanceEditor", "Poller success");
        const MfClassicData* mfc_data = nfc_poller_get_data(app->poller);
        nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, mfc_data);
        memcpy(app->uid, mfc_data->iso14443_3a_data->uid, UID_LENGTH);
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventPollerDone);
        command = NfcCommandStop;
    } else if(mfc_event->type == MfClassicPollerEventTypeFail) {
        FURI_LOG_D("MiZipBalanceEditor", "Poller failed");
        command = NfcCommandStop;
    }
    return command;
}

void mizip_balance_editor_scene_scanner_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    if(event.type == NfcScannerEventTypeDetected) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventCardDetected);
        if(*event.data.protocols == NfcProtocolMfClassic) {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MiZipBalanceEditorCustomEventMfClassicCard);
        } else {
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MiZipBalanceEditorCustomEventWrongCard);
        }
    }
}

void mizip_balance_editor_scene_scanner_on_enter(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;

    popup_set_header(
        app->popup, "Apply MiZip\n  tag to the\n      back", 65, 30, AlignLeft, AlignTop);
    popup_set_icon(app->popup, 0, 3, &I_RFIDDolphinReceive_97x61);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdScanner);
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, mizip_balance_editor_scene_scanner_scan_callback, app);
    app->is_scan_active = true;
    notification_message(app->notifications, &sequence_blink_start_cyan);
    FURI_LOG_I("MiZipBalanceEditor", "Enter scanner scene");
}

bool mizip_balance_editor_scene_scanner_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MiZipBalanceEditorCustomEventCardDetected) {
            popup_set_header(app->popup, "Don't move!", 65, 40, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventCardLost) {
            popup_set_header(app->popup, "Tag lost...", 65, 40, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventWrongCard) {
            nfc_scanner_stop(app->scanner);
            nfc_scanner_free(app->scanner);
            app->is_scan_active = false;
            popup_set_header(app->popup, "Wrong tag", 65, 40, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventMfClassicCard) {
            nfc_scanner_stop(app->scanner);
            nfc_scanner_free(app->scanner);
            app->is_scan_active = false;
            app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
            nfc_poller_start(
                app->poller, mizip_balance_editor_scene_scanner_poller_callback, context);
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventPollerDone) {
            nfc_poller_stop(app->poller);
            nfc_poller_free(app->poller);
            scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorViewIdReader);
            consumed = true;
        }
    }
    return consumed;
}

void mizip_balance_editor_scene_scanner_on_exit(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    if(app->is_scan_active) {
        nfc_scanner_stop(app->scanner);
        nfc_scanner_free(app->scanner);
    }
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
