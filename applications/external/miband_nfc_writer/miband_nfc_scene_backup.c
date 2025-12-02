/**
 * @file miband_nfc_scene_backup.c
 * @brief Automatic backup before write operations
 * 
 * This scene automatically backs up the current Mi Band data before
 * performing write operations. Creates backups in /ext/nfc/backups/
 * with timestamp for easy restoration.
 */

#include "miband_nfc_i.h"
#include <datetime/datetime.h>

#define TAG           "MiBandNfc"
#define BACKUP_FOLDER EXT_PATH("nfc/backups")

/**
 * @brief Create backup directory if it doesn't exist
 * 
 * @param storage Storage API instance
 * @return true if directory exists or was created successfully
 */
static bool ensure_backup_directory(Storage* storage) {
    if(!storage_simply_mkdir(storage, BACKUP_FOLDER)) {
        // Check if it already exists
        FileInfo file_info;
        if(storage_common_stat(storage, BACKUP_FOLDER, &file_info) == FSE_OK) {
            return file_info.flags & FSF_DIRECTORY;
        }
        return false;
    }
    return true;
}

/**
 * @brief Read all data from Mi Band using multiple key strategies
 * 
 * @param app Pointer to MiBandNfcApp instance
 * @return true if all sectors read successfully
 */
static bool backup_read_all_data(MiBandNfcApp* app) {
    size_t total_sectors = mf_classic_get_total_sectors_num(app->mf_classic_data->type);

    popup_set_text(app->popup, "Reading Mi Band...\n\nSector 0/16", 64, 22, AlignCenter, AlignTop);

    FuriString* progress = furi_string_alloc();
    for(size_t sector = 0; sector < total_sectors; sector++) {
        uint8_t first_block = mf_classic_get_first_block_num_of_sector(sector);
        uint8_t blocks_in_sector = mf_classic_get_blocks_num_in_sector(sector);

        if(sector % 2 == 0) {
            furi_string_reset(progress);
            furi_string_printf(
                progress, "Reading...\nSector %zu/%zu\n", sector + 1, total_sectors);

            uint32_t percent = ((sector + 1) * 100) / total_sectors;
            furi_string_cat_str(progress, "[");
            for(uint8_t i = 0; i < 20; i++) {
                if(i < (percent / 5))
                    furi_string_cat_str(progress, "=");
                else
                    furi_string_cat_str(progress, " ");
            }
            furi_string_cat_printf(progress, "]\n%lu%%", percent);

            popup_set_text(
                app->popup, furi_string_get_cstr(progress), 64, 18, AlignCenter, AlignTop);
        }

        furi_delay_ms(50);

        MfClassicKey keys_to_try[3];
        MfClassicKeyType key_types[3];
        int keys_count = 0;

        MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(app->mf_classic_data, sector);

        if(sec_tr && mf_classic_is_key_found(app->mf_classic_data, sector, MfClassicKeyTypeA)) {
            memcpy(keys_to_try[keys_count].data, sec_tr->key_a.data, 6);
            key_types[keys_count] = MfClassicKeyTypeA;
            keys_count++;
        }

        if(sec_tr && mf_classic_is_key_found(app->mf_classic_data, sector, MfClassicKeyTypeB)) {
            memcpy(keys_to_try[keys_count].data, sec_tr->key_b.data, 6);
            key_types[keys_count] = MfClassicKeyTypeB;
            keys_count++;
        }

        memset(keys_to_try[keys_count].data, 0xFF, 6);
        key_types[keys_count] = MfClassicKeyTypeA;
        keys_count++;

        bool sector_read = false;
        for(int key_idx = 0; key_idx < keys_count && !sector_read; key_idx++) {
            MfClassicAuthContext auth_context;
            MfClassicError error = mf_classic_poller_sync_auth(
                app->nfc, first_block, &keys_to_try[key_idx], key_types[key_idx], &auth_context);

            if(error == MfClassicErrorNone) {
                bool all_read = true;
                for(uint8_t block_in_sector = 0; block_in_sector < blocks_in_sector;
                    block_in_sector++) {
                    size_t block_idx = first_block + block_in_sector;

                    error = mf_classic_poller_sync_read_block(
                        app->nfc,
                        block_idx,
                        &keys_to_try[key_idx],
                        key_types[key_idx],
                        &app->target_data->block[block_idx]);

                    if(error != MfClassicErrorNone) {
                        all_read = false;
                        break;
                    }
                }

                if(all_read) {
                    sector_read = true;
                    FURI_BIT_SET(app->target_data->key_a_mask, sector);
                    FURI_BIT_SET(app->target_data->key_b_mask, sector);
                }
            }
        }

        if(!sector_read) {
            FURI_LOG_W(TAG, "Failed to read sector %zu for backup", sector);
            furi_string_free(progress);
            return false;
        }

        furi_delay_ms(50);
    }
    furi_string_free(progress);
    return true;
}

/**
 * @brief Scene entry point - performs backup operation
 * 
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_backup_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;
    if(app->logger) {
        miband_logger_log(app->logger, LogLevelInfo, "Backup operation started");
    }
    popup_reset(app->popup);
    popup_set_header(app->popup, "Creating Backup", 64, 4, AlignCenter, AlignTop);
    popup_set_text(
        app->popup, "Place Mi Band near\nFlipper Zero...", 64, 22, AlignCenter, AlignTop);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdScanner);
    notification_message(app->notifications, &sequence_blink_start_cyan);

    if(!ensure_backup_directory(app->storage)) {
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelError, "Cannot create backup directory");
        }
        notification_message(app->notifications, &sequence_error);
        popup_set_header(app->popup, "Backup Failed", 64, 4, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Cannot create\nbackup folder", 64, 22, AlignCenter, AlignTop);
        furi_delay_ms(2000);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    mf_classic_reset(app->target_data);
    app->target_data->type = app->mf_classic_data->type;

    popup_set_text(app->popup, "Detecting card...", 64, 30, AlignCenter, AlignTop);

    MfClassicType detected_type;
    if(mf_classic_poller_sync_detect_type(app->nfc, &detected_type) != MfClassicErrorNone) {
        notification_message(app->notifications, &sequence_error);
        popup_set_header(app->popup, "Card Not Found", 64, 4, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Place card near\nFlipper Zero", 64, 22, AlignCenter, AlignTop);
        furi_delay_ms(2000);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    if(!backup_read_all_data(app)) {
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelError, "Backup: cannot read all sectors");
        }
        notification_message(app->notifications, &sequence_error);
        popup_set_header(app->popup, "Read Failed", 64, 4, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Cannot read\nall sectors", 64, 22, AlignCenter, AlignTop);
        furi_delay_ms(2000);
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    DateTime datetime;
    furi_hal_rtc_get_datetime(&datetime);

    FuriString* backup_path = furi_string_alloc_printf(
        "%s/backup_%04d%02d%02d_%02d%02d%02d.nfc",
        BACKUP_FOLDER,
        datetime.year,
        datetime.month,
        datetime.day,
        datetime.hour,
        datetime.minute,
        datetime.second);

    popup_set_text(app->popup, "Saving backup...", 64, 11, AlignCenter, AlignTop);

    nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, app->target_data);
    bool save_success = nfc_device_save(app->nfc_device, furi_string_get_cstr(backup_path));
    nfc_device_set_data(app->nfc_device, NfcProtocolMfClassic, app->mf_classic_data);

    popup_reset(app->popup);

    if(save_success) {
        if(app->logger) {
            miband_logger_log(
                app->logger, LogLevelInfo, "Backup saved: %s", furi_string_get_cstr(backup_path));
        }
        notification_message(app->notifications, &sequence_success);
        popup_set_header(app->popup, "Backup Created!", 64, 4, AlignCenter, AlignTop);

        FuriString* msg = furi_string_alloc_printf(
            "Saved to:\nbackups/backup_%04d%02d%02d\n_%02d%02d%02d.nfc",
            datetime.year,
            datetime.month,
            datetime.day,
            datetime.hour,
            datetime.minute,
            datetime.second);

        popup_set_text(app->popup, furi_string_get_cstr(msg), 4, 18, AlignLeft, AlignTop);
        popup_set_icon(app->popup, 90, 20, &I_DolphinSuccess_91x55);

        furi_string_free(msg);
        FURI_LOG_I(TAG, "Backup saved: %s", furi_string_get_cstr(backup_path));
    } else {
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelError, "Failed to save backup file");
        }
        notification_message(app->notifications, &sequence_error);
        popup_set_header(app->popup, "Save Failed", 64, 4, AlignCenter, AlignTop);
        popup_set_text(app->popup, "Cannot save\nbackup file", 64, 22, AlignCenter, AlignTop);
        popup_set_icon(app->popup, 40, 28, &I_WarningDolphinFlip_45x42);
        FURI_LOG_E(TAG, "Failed to save backup");
    }

    furi_string_free(backup_path);
    furi_delay_ms(3000);

    scene_manager_next_scene(app->scene_manager, MiBandNfcSceneWriter);
}

/**
 * @brief Scene event handler
 */
bool miband_nfc_scene_backup_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context);
    UNUSED(event);
    return false;
}

/**
 * @brief Scene exit handler
 */
void miband_nfc_scene_backup_on_exit(void* context) {
    MiBandNfcApp* app = context;
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
