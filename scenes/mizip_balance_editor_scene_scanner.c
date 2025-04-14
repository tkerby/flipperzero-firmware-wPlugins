#include "../mizip_balance_editor_i.h"

static NfcCommand
    mizip_balance_editor_scene_scanner_poller_callback(NfcGenericEvent event, void* context) {
    furi_assert(event.protocol == NfcProtocolMfClassic);
    MiZipBalanceEditorApp* app = context;
    NfcCommand command = NfcCommandContinue;

    const MfClassicPollerEvent* mf_classic_event = event.event_data;
    if(mf_classic_event->type == MfClassicPollerEventTypeSuccess) {
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));
        const MfClassicData* mf_classic_data =
            nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        mf_classic_copy(app->mf_classic_data, mf_classic_data);
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventPollerSuccess);
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
}

bool mizip_balance_editor_scene_scanner_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MiZipBalanceEditorCustomEventCardDetected) {
            popup_set_header(app->popup, "Reading...", 65, 40, AlignLeft, AlignTop);
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
        } else if(event.event == MiZipBalanceEditorCustomEventPollerSuccess) {
            nfc_poller_stop(app->poller);
            nfc_poller_free(app->poller);
            app->is_valid_mizip_data = mizip_verify(context);
            scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorViewIdShowBalance);
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
    popup_reset(app->popup);
}
