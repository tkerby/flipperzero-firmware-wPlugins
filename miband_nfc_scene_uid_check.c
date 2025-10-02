/**
 * @file miband_nfc_scene_uid_check.c
 * @brief Quick UID checker with disk scanner - FIXED
 * 
 * This scene quickly reads and displays the UID of a card, then scans
 * the /ext/nfc directory to find all dump files with matching UID.
 * Uses scanner to properly wait for card placement.
 */

#include "miband_nfc_i.h"

#define TAG "MiBandNfc"

/**
 * @brief Structure to store found file information
 */
typedef struct {
    char filename[256];
    char path[512];
} FoundFile;

/**
 * @brief Scan NFC directory recursively for matching UID
 */
static void scan_directory_for_uid(
    Storage* storage,
    const char* directory,
    const uint8_t* target_uid,
    FoundFile* found_files,
    size_t max_files,
    size_t* found_count,
    Popup* popup) {
    FURI_LOG_I(TAG, "Scanning directory: %s", directory);

    File* dir = storage_file_alloc(storage);
    if(!dir) {
        FURI_LOG_E(TAG, "Failed to alloc file");
        return;
    }

    if(!storage_dir_open(dir, directory)) {
        FURI_LOG_E(TAG, "Failed to open dir: %s", directory);
        storage_file_free(dir);
        return;
    }

    FileInfo file_info;
    char name[256];
    FuriString* full_path = furi_string_alloc();
    uint32_t files_checked = 0;

    FURI_LOG_I(TAG, "Starting directory scan loop");

    while(storage_dir_read(dir, &file_info, name, sizeof(name))) {
        if(name[0] == '.') continue;

        furi_string_printf(full_path, "%s/%s", directory, name);

        // SOLO ROOT - NO RICORSIONE per evitare stack overflow
        if(file_info.flags & FSF_DIRECTORY) {
            FURI_LOG_D(TAG, "Skipping subdirectory: %s", name);
            continue; // SALTA le sottocartelle
        }

        if(strstr(name, ".nfc") != NULL) {
            files_checked++;
            FURI_LOG_D(TAG, "Checking file %lu: %s", files_checked, name);

            // Update UI
            if(popup) {
                FuriString* msg = furi_string_alloc_printf(
                    "Scanning...\n%lu/%lu files", files_checked, files_checked);
                popup_set_text(popup, furi_string_get_cstr(msg), 64, 30, AlignCenter, AlignTop);
                furi_string_free(msg);
                furi_delay_ms(10);
            }

            // Limite massimo
            if(files_checked > 50) {
                FURI_LOG_W(TAG, "Reached 50 files limit, stopping");
                break;
            }

            // Carica file
            NfcDevice* temp_device = nfc_device_alloc();
            if(!temp_device) {
                FURI_LOG_E(TAG, "Failed to alloc NFC device");
                continue;
            }

            FURI_LOG_D(TAG, "Loading: %s", furi_string_get_cstr(full_path));

            if(nfc_device_load(temp_device, furi_string_get_cstr(full_path))) {
                if(nfc_device_get_protocol(temp_device) == NfcProtocolMfClassic) {
                    const MfClassicData* data =
                        nfc_device_get_data(temp_device, NfcProtocolMfClassic);

                    if(data) {
                        if(memcmp(data->block[0].data, target_uid, 4) == 0) {
                            FURI_LOG_I(TAG, "MATCH FOUND: %s", name);

                            if(*found_count < max_files) {
                                strncpy(found_files[*found_count].filename, name, 255);
                                found_files[*found_count].filename[255] = '\0';
                                strncpy(
                                    found_files[*found_count].path,
                                    furi_string_get_cstr(full_path),
                                    511);
                                found_files[*found_count].path[511] = '\0';
                                (*found_count)++;
                            }
                        }
                    } else {
                        FURI_LOG_W(TAG, "No data in file: %s", name);
                    }
                } else {
                    FURI_LOG_D(TAG, "Not MfClassic: %s", name);
                }
            } else {
                FURI_LOG_W(TAG, "Failed to load: %s", name);
            }

            nfc_device_free(temp_device);
            furi_delay_ms(10); // Previeni watchdog
        }
    }

    FURI_LOG_I(TAG, "Scan complete. Checked: %lu, Found: %zu", files_checked, *found_count);

    furi_string_free(full_path);
    storage_dir_close(dir);
    storage_file_free(dir);
}

/**
 * @brief Generate UID report with disk scan results
 */
static FuriString* generate_uid_report(
    const MfClassicBlock* block0,
    const FoundFile* found_files,
    size_t found_count) {
    FURI_LOG_I(TAG, "Generating report for %zu found files", found_count);

    FuriString* report = furi_string_alloc();
    if(!report) {
        FURI_LOG_E(TAG, "Failed to alloc report string");
        return NULL;
    }

    FURI_LOG_D(TAG, "Adding header...");
    furi_string_printf(
        report,
        "Card Information\n"
        "================\n\n");

    FURI_LOG_D(TAG, "Adding UID info...");
    furi_string_cat_printf(
        report,
        "UID: %02X %02X %02X %02X\n",
        block0->data[0],
        block0->data[1],
        block0->data[2],
        block0->data[3]);

    furi_string_cat_printf(
        report,
        "BCC: %02X %s\n",
        block0->data[4],
        (block0->data[4] ==
         (block0->data[0] ^ block0->data[1] ^ block0->data[2] ^ block0->data[3])) ?
            "(OK)" :
            "(BAD)");

    furi_string_cat_printf(report, "SAK: %02X\n", block0->data[5]);

    furi_string_cat_printf(report, "ATQA: %02X %02X\n\n", block0->data[6], block0->data[7]);

    FURI_LOG_D(TAG, "Adding manufacturer data...");
    furi_string_cat_str(report, "Manufacturer Data:\n");
    for(int i = 8; i < 16; i += 8) {
        furi_string_cat_str(report, "  ");
        for(int j = 0; j < 8 && (i + j) < 16; j++) {
            furi_string_cat_printf(report, "%02X ", block0->data[i + j]);
        }
        furi_string_cat_str(report, "\n");
    }

    FURI_LOG_D(TAG, "Adding disk scan results...");
    furi_string_cat_str(report, "\nDisk Scan Results:\n");
    furi_string_cat_str(report, "==================\n");

    if(found_count == 0) {
        FURI_LOG_D(TAG, "No matches found");
        furi_string_cat_str(report, "No matching dumps found\n\n");
    } else {
        FURI_LOG_I(TAG, "Adding %zu matching files", found_count);
        furi_string_cat_printf(report, "%zu file(s) found:\n\n", found_count);

        for(size_t i = 0; i < found_count; i++) {
            FURI_LOG_D(TAG, "Adding file %zu: %s", i, found_files[i].filename);

            const char* path = found_files[i].path;
            const char* rel_path = strstr(path, "/ext/nfc/");
            if(rel_path) {
                rel_path += 9;
            } else {
                rel_path = found_files[i].filename;
            }

            furi_string_cat_printf(report, "%zu. %s\n", i + 1, rel_path);
        }
        furi_string_cat_str(report, "\n");
    }

    furi_string_cat_str(report, "Press Back to return");

    FURI_LOG_I(TAG, "Report generation complete, length: %zu", furi_string_size(report));
    return report;
}

/**
 * @brief Scene entry point
 */
void miband_nfc_scene_uid_check_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    popup_reset(app->popup);
    popup_set_header(app->popup, "UID Check", 64, 4, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Place card near Flipper\nReading...", 64, 22, AlignCenter, AlignTop);
    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdScanner);
    notification_message(app->notifications, &sequence_blink_start_cyan);

    MfClassicBlock block0;
    MfClassicKey magic_key;
    memset(magic_key.data, 0xFF, 6);
    MfClassicAuthContext auth_context;
    bool read_success = false;

    for(int attempt = 0; attempt < 20 && !read_success; attempt++) {
        FuriString* attempt_msg =
            furi_string_alloc_printf("Reading card...\nAttempt %d/20", attempt + 1);
        popup_set_text(
            app->popup, furi_string_get_cstr(attempt_msg), 64, 22, AlignCenter, AlignTop);
        furi_string_free(attempt_msg);

        furi_delay_ms(500);

        MfClassicError error =
            mf_classic_poller_sync_auth(app->nfc, 0, &magic_key, MfClassicKeyTypeA, &auth_context);

        if(error == MfClassicErrorNone) {
            error = mf_classic_poller_sync_read_block(
                app->nfc, 0, &magic_key, MfClassicKeyTypeA, &block0);
            if(error == MfClassicErrorNone) {
                read_success = true;
                break;
            }
        }
    }

    if(!read_success) {
        notification_message(app->notifications, &sequence_error);
        popup_set_text(app->popup, "Card not found", 64, 30, AlignCenter, AlignTop);
        furi_delay_ms(2000);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    notification_message(app->notifications, &sequence_success);
    notification_message(app->notifications, &sequence_blink_stop);

    popup_set_text(app->popup, "Scanning disk...\nPlease wait", 64, 30, AlignCenter, AlignTop);

    FoundFile* found_files = malloc(sizeof(FoundFile) * 50);
    size_t found_count = 0;

    scan_directory_for_uid(
        app->storage, NFC_APP_FOLDER, block0.data, found_files, 50, &found_count, app->popup);

    FuriString* report = generate_uid_report(&block0, found_files, found_count);

    text_box_set_text(app->text_box, furi_string_get_cstr(report));
    text_box_set_font(app->text_box, TextBoxFontText);
    text_box_set_focus(app->text_box, TextBoxFocusStart);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdAbout);

    furi_string_free(report);
    free(found_files);
}
/**
 * @brief Scene event handler
 */
bool miband_nfc_scene_uid_check_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeBack) {
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

/**
 * @brief Scene exit handler
 */
void miband_nfc_scene_uid_check_on_exit(void* context) {
    MiBandNfcApp* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
    text_box_reset(app->text_box);
}
