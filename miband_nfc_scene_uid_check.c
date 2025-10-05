/**
 * @file miband_nfc_scene_uid_check.c
 * @brief Quick UID checker - FIXED
 */

#include "miband_nfc_i.h"

#define TAG                "MiBandNfc"
#define MAX_FILES_TO_CHECK 20
#define MAX_SCAN_TIME_MS   5000

typedef struct {
    char filename[128];
    char path[256];
} FoundFile;

static void scan_directory_for_uid(
    Storage* storage,
    const char* directory,
    const uint8_t* target_uid,
    FoundFile* found_files,
    size_t max_files,
    size_t* found_count,
    Popup* popup) {
    File* dir = storage_file_alloc(storage);
    if(!dir) return;

    if(!storage_dir_open(dir, directory)) {
        storage_file_free(dir);
        return;
    }

    FileInfo file_info;
    char name[128];
    FuriString* full_path = furi_string_alloc();
    uint32_t files_checked = 0;
    uint32_t start_time = furi_get_tick();

    while(storage_dir_read(dir, &file_info, name, sizeof(name))) {
        if(furi_get_tick() - start_time > MAX_SCAN_TIME_MS) break;
        if(files_checked % 10 == 0) furi_delay_ms(10);
        if(name[0] == '.') continue;
        if(file_info.flags & FSF_DIRECTORY) continue;
        if(!strstr(name, ".nfc")) continue;

        files_checked++;
        if(files_checked > MAX_FILES_TO_CHECK) break;

        if(files_checked % 5 == 0 && popup) {
            FuriString* msg = furi_string_alloc_printf("Scanning...\n%lu files", files_checked);
            popup_set_text(popup, furi_string_get_cstr(msg), 64, 30, AlignCenter, AlignTop);
            furi_string_free(msg);
        }

        furi_string_printf(full_path, "%s/%s", directory, name);
        NfcDevice* temp_device = nfc_device_alloc();
        if(!temp_device) continue;

        if(nfc_device_load(temp_device, furi_string_get_cstr(full_path))) {
            if(nfc_device_get_protocol(temp_device) == NfcProtocolMfClassic) {
                const MfClassicData* data = nfc_device_get_data(temp_device, NfcProtocolMfClassic);
                if(data && memcmp(data->block[0].data, target_uid, 4) == 0) {
                    if(*found_count < max_files) {
                        strncpy(found_files[*found_count].filename, name, 127);
                        found_files[*found_count].filename[127] = '\0';
                        strncpy(
                            found_files[*found_count].path, furi_string_get_cstr(full_path), 255);
                        found_files[*found_count].path[255] = '\0';
                        (*found_count)++;
                    }
                }
            }
        }
        nfc_device_free(temp_device);
        furi_delay_ms(5);
    }

    furi_string_free(full_path);
    storage_dir_close(dir);
    storage_file_free(dir);
}

static FuriString* generate_uid_report(
    const MfClassicBlock* block0,
    const FoundFile* found_files,
    size_t found_count) {
    FuriString* report = furi_string_alloc();
    if(!report) return NULL;

    furi_string_printf(
        report,
        "Card Info\n"
        "=========\n\n"
        "UID: %02X %02X %02X %02X\n"
        "BCC: %02X %s\n"
        "SAK: %02X\n"
        "ATQA: %02X %02X\n\n",
        block0->data[0],
        block0->data[1],
        block0->data[2],
        block0->data[3],
        block0->data[4],
        (block0->data[4] ==
         (block0->data[0] ^ block0->data[1] ^ block0->data[2] ^ block0->data[3])) ?
            "(OK)" :
            "(BAD)",
        block0->data[5],
        block0->data[6],
        block0->data[7]);

    furi_string_cat_str(report, "Manufacturer:\n  ");
    for(int i = 8; i < 16; i++) {
        furi_string_cat_printf(report, "%02X ", block0->data[i]);
        if(i == 11) furi_string_cat_str(report, "\n  ");
    }

    furi_string_cat_str(report, "\n\nMatching Files:\n===============\n");

    if(found_count == 0) {
        furi_string_cat_str(report, "No matches found\n");
    } else {
        furi_string_cat_printf(report, "%zu file(s):\n\n", found_count);
        for(size_t i = 0; i < found_count; i++) {
            const char* rel_path = strstr(found_files[i].path, "/nfc/");
            furi_string_cat_printf(
                report, "%zu. %s\n", i + 1, rel_path ? rel_path + 5 : found_files[i].filename);
        }
    }

    furi_string_cat_str(report, "\nPress Back");
    return report;
}

void miband_nfc_scene_uid_check_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    FURI_LOG_I(TAG, "UID Check: Starting");

    // Reset view
    text_box_reset(app->text_box_report);
    dialog_ex_reset(app->dialog_ex);

    // Mostra popup BREVEMENTE poi nascondi
    popup_reset(app->popup);
    popup_set_header(app->popup, "UID Check", 64, 4, AlignCenter, AlignTop);
    popup_set_text(app->popup, "Place card and\nhold steady...", 64, 22, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdScanner);

    notification_message(app->notifications, &sequence_blink_start_cyan);

    // DAI TEMPO ALL'UTENTE DI POSIZIONARE LA CARTA
    furi_delay_ms(1500);

    // Rileva carta
    MfClassicType detected_type = MfClassicType1k;
    MfClassicError detect_error = mf_classic_poller_sync_detect_type(app->nfc, &detected_type);

    if(detect_error != MfClassicErrorNone) {
        FURI_LOG_E(TAG, "Detection failed: %d", detect_error);
        notification_message(app->notifications, &sequence_blink_stop);
        notification_message(app->notifications, &sequence_error);
        popup_set_text(app->popup, "No card detected", 64, 30, AlignCenter, AlignTop);
        furi_delay_ms(2000);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    FURI_LOG_I(TAG, "Card detected: type %d", detected_type);

    // Leggi blocco 0 - NESSUN UPDATE UI QUI
    MfClassicBlock block0;
    MfClassicKey magic_key;
    memset(magic_key.data, 0xFF, 6);
    MfClassicAuthContext auth_context;

    MfClassicError auth_error =
        mf_classic_poller_sync_auth(app->nfc, 0, &magic_key, MfClassicKeyTypeA, &auth_context);

    if(auth_error != MfClassicErrorNone) {
        FURI_LOG_E(TAG, "Auth failed: %d", auth_error);

        // Prova con scanner invece di poller diretto
        notification_message(app->notifications, &sequence_blink_stop);
        furi_delay_ms(500);
        notification_message(app->notifications, &sequence_blink_start_cyan);

        // Secondo tentativo
        detect_error = mf_classic_poller_sync_detect_type(app->nfc, &detected_type);
        if(detect_error == MfClassicErrorNone) {
            auth_error = mf_classic_poller_sync_auth(
                app->nfc, 0, &magic_key, MfClassicKeyTypeA, &auth_context);
        }

        if(auth_error != MfClassicErrorNone) {
            notification_message(app->notifications, &sequence_blink_stop);
            notification_message(app->notifications, &sequence_error);
            popup_set_text(app->popup, "Auth failed\nTry again", 64, 30, AlignCenter, AlignTop);
            furi_delay_ms(2000);
            scene_manager_previous_scene(app->scene_manager);
            return;
        }
    }

    MfClassicError read_error =
        mf_classic_poller_sync_read_block(app->nfc, 0, &magic_key, MfClassicKeyTypeA, &block0);

    notification_message(app->notifications, &sequence_blink_stop);

    if(read_error != MfClassicErrorNone) {
        FURI_LOG_E(TAG, "Read failed: %d", read_error);
        notification_message(app->notifications, &sequence_error);
        popup_set_text(app->popup, "Read failed", 64, 30, AlignCenter, AlignTop);
        furi_delay_ms(2000);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    FURI_LOG_I(
        TAG,
        "UID: %02X%02X%02X%02X",
        block0.data[0],
        block0.data[1],
        block0.data[2],
        block0.data[3]);

    notification_message(app->notifications, &sequence_success);

    // ORA puoi aggiornare UI
    popup_set_text(app->popup, "Success!\nScanning disk...", 64, 30, AlignCenter, AlignTop);
    furi_delay_ms(300);

    // Scan disco
    FoundFile* found_files = malloc(sizeof(FoundFile) * MAX_FILES_TO_CHECK);
    if(!found_files) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    size_t found_count = 0;
    scan_directory_for_uid(
        app->storage,
        NFC_APP_FOLDER,
        block0.data,
        found_files,
        MAX_FILES_TO_CHECK,
        &found_count,
        app->popup);

    FuriString* report = generate_uid_report(&block0, found_files, found_count);
    free(found_files);

    if(!report) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    if(app->uid_check_text) {
        free(app->uid_check_text);
    }

    app->uid_check_text = malloc(furi_string_size(report) + 1);
    if(!app->uid_check_text) {
        furi_string_free(report);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    strcpy(app->uid_check_text, furi_string_get_cstr(report));
    furi_string_free(report);

    // Reset e switch
    popup_reset(app->popup);
    furi_delay_ms(50);

    text_box_reset(app->text_box_report);
    text_box_set_text(app->text_box_report, app->uid_check_text);
    text_box_set_font(app->text_box_report, TextBoxFontText);
    text_box_set_focus(app->text_box_report, TextBoxFocusStart);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdUidReport);
}

bool miband_nfc_scene_uid_check_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        return true;
    }
    return false;
}

void miband_nfc_scene_uid_check_on_exit(void* context) {
    MiBandNfcApp* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
    text_box_reset(app->text_box_report);

    if(app->uid_check_text) {
        free(app->uid_check_text);
        app->uid_check_text = NULL;
    }
}
