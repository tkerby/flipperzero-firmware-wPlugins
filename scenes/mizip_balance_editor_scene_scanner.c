#include "../mizip_balance_editor_i.h"
#include "adapted_from_ofw/mizip.h"
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

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
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventCardLost);
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestMode) {
        FURI_LOG_D("MiZipBalanceEditor", "Poller request mode");
        const MfClassicData* mfc_data = nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic);
        mfc_event->data->poller_mode.mode = MfClassicPollerModeDictAttackStandard;
        mfc_event->data->poller_mode.data = mfc_data;
        app->nfc_dict_context.sectors_total = mf_classic_get_total_sectors_num(mfc_data->type);
        mf_classic_get_read_sectors_and_keys(
            mfc_data, &app->nfc_dict_context.sectors_read, &app->nfc_dict_context.keys_found);
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventCardLost);
    } else if(mfc_event->type == MfClassicPollerEventTypeRequestKey) {
        MfClassicKey key = {};
        if(keys_dict_get_next_key(app->nfc_dict_context.dict, key.data, sizeof(MfClassicKey))) {
            mfc_event->data->key_request_data.key = key;
            mfc_event->data->key_request_data.key_provided = true;
        } else {
            mfc_event->data->key_request_data.key_provided = false;
        }
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeDataUpdate) {
        FURI_LOG_D("MiZipBalanceEditor", "Data update");
        MfClassicPollerEventDataUpdate* data_update = &mfc_event->data->data_update;
        app->nfc_dict_context.sectors_read = data_update->sectors_read;
        app->nfc_dict_context.keys_found = data_update->keys_found;
        app->nfc_dict_context.current_sector = data_update->current_sector;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeNextSector) {
        FURI_LOG_D("MiZipBalanceEditor", "Next sector");
        keys_dict_rewind(app->nfc_dict_context.dict);
        app->nfc_dict_context.current_sector = mfc_event->data->next_sector_data.current_sector;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeFoundKeyA) {
        FURI_LOG_D("MiZipBalanceEditor", "Found key A");
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeFoundKeyB) {
        FURI_LOG_D("MiZipBalanceEditor", "Found key B");
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeKeyAttackStart) {
        FURI_LOG_D("MiZipBalanceEditor", "Attack start");
        app->nfc_dict_context.current_sector = mfc_event->data->key_attack_data.current_sector;
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeKeyAttackStop) {
        FURI_LOG_D("MiZipBalanceEditor", "Attack stop");
        keys_dict_rewind(app->nfc_dict_context.dict);
        view_dispatcher_send_custom_event(
            app->view_dispatcher, MiZipBalanceEditorCustomEventDictAttackDataUpdate);
    } else if(mfc_event->type == MfClassicPollerEventTypeSuccess) {
        FURI_LOG_D("MiZipBalanceEditor", "Poller success");
        nfc_device_set_data(
            app->nfc_device, NfcProtocolMfClassic, nfc_poller_get_data(app->poller));
        mf_classic_copy(
            app->mf_classic_data, nfc_device_get_data(app->nfc_device, NfcProtocolMfClassic));
        memcpy(app->uid, app->mf_classic_data->iso14443_3a_data->uid, UID_LENGTH);
        if(mf_classic_is_card_read(app->mf_classic_data)) {
            dolphin_deed(DolphinDeedNfcReadSuccess);
            FURI_LOG_D("MiZipBalanceEditor", "Card readed");
        } else {
            FURI_LOG_D("MiZipBalanceEditor", "Card not readed");
        }
        app->is_valid_mizip_data = mizip_parse(context);
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

    app->nfc_dict_context.sectors_total = 0;
    app->nfc_dict_context.sectors_read = 0;
    app->nfc_dict_context.current_sector = 0;

    view_dispatcher_switch_to_view(app->view_dispatcher, MiZipBalanceEditorViewIdScanner);
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, mizip_balance_editor_scene_scanner_scan_callback, app);
    app->is_scan_active = true;
    FURI_LOG_I("MiZipBalanceEditor", "Enter scanner scene");
}

bool mizip_balance_editor_scene_scanner_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    MiZipBalanceEditorApp* app = context;
    bool consumed = false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == MiZipBalanceEditorCustomEventCardDetected) {
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventCardLost) {
            popup_set_header(app->popup, "Card lost...", 65, 40, AlignLeft, AlignTop);
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

            //Init key dict
            popup_set_header(app->popup, "Loading dict...", 65, 40, AlignLeft, AlignTop);
            app->nfc_dict_context.dict = keys_dict_alloc(
                NFC_APP_MF_CLASSIC_DICT_SYSTEM_PATH, KeysDictModeOpenAlways, sizeof(MfClassicKey));
            if(keys_dict_get_total_keys(app->nfc_dict_context.dict) == 0) {
                keys_dict_free(app->nfc_dict_context.dict);
                return false;
            }

            //Start poller
            popup_set_header(app->popup, "Loading poller...", 65, 40, AlignLeft, AlignTop);
            app->is_scan_active = false;
            app->poller = nfc_poller_alloc(app->nfc, NfcProtocolMfClassic);
            nfc_poller_start(
                app->poller, mizip_balance_editor_scene_scanner_poller_callback, context);
            popup_set_header(app->popup, "Poller loaded!", 65, 40, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventDictAttackDataUpdate) {
            popup_set_header(app->popup, "Attacking...", 65, 40, AlignLeft, AlignTop);
            consumed = true;
        } else if(event.event == MiZipBalanceEditorCustomEventPollerSuccess) {
            nfc_poller_stop(app->poller);
            nfc_poller_free(app->poller);
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
        keys_dict_free(app->nfc_dict_context.dict);
    }
    popup_reset(app->popup);
}
