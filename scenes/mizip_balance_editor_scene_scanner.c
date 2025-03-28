#include "../mizip_balance_editor.h"

void mizip_balance_editor_scene_scanner_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    FURI_LOG_T("SCANNER", "Scanner event raised");
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
    FURI_LOG_T("SCANNER", "Scanner view initiated");
}

bool mizip_balance_editor_scene_scanner_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MiZipBalanceEditorCustomEventCardDetected) {
            FURI_LOG_T("SCANNER", "Reading in progress");
            popup_set_header(app->popup, "Reading...", 65, 40, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventWrongCard) {
            FURI_LOG_T("SCANNER", "Wrong card");
            nfc_scanner_stop(app->scanner);
            nfc_scanner_free(app->scanner);

            popup_set_header(app->popup, "Wrong tag", 65, 40, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventMfClassicCard) {
            FURI_LOG_T("SCANNER", "MfClassic Card");
            nfc_scanner_stop(app->scanner);
            nfc_scanner_free(app->scanner);

            app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
            mf_classic_copy(app->mf_classic_data, nfc_poller_get_data(app->poller));
            nfc_poller_free(app->poller);
            app->is_valid_mizip_data = mizip_verify(context);
            scene_manager_next_scene(app->scene_manager, MiZipBalanceEditorViewIdShowResult);
            consumed = true;
        }
    }
    return consumed;
}

void mizip_balance_editor_scene_scanner_on_exit(void* context) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    popup_reset(app->popup);
}
