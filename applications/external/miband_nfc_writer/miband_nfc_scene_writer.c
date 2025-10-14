/**
 * @file miband_nfc_scene_writer.c
 * @brief Mi Band NFC data writing scene
 * 
 * This scene handles writing dump data to the Mi Band NFC chip. It implements
 * a robust writing strategy that:
 * 1. Detects the current state of the Mi Band (magic keys vs original keys)
 * 2. Preserves the UID in Block 0 (which is read-only on most cards)
 * 3. Writes all data blocks and sector trailers
 * 4. Uses appropriate authentication keys (0xFF for magic cards, dump keys for rewrites)
 * 5. Provides real-time progress feedback
 */

#include "miband_nfc_i.h"
#include <nfc/protocols/mf_classic/mf_classic_poller_sync.h>

#define TAG "MiBandNfc"

/**
 * @brief Write all blocks in a sector (excluding Block 0)
 * 
 * Helper function that writes all data blocks and the sector trailer
 * for a given sector. Uses retry logic for reliability.
 * 
 * @param app Pointer to MiBandNfcApp instance
 * @param sector Sector number to write
 * @param first_block First block index of the sector
 * @param blocks_in_sector Number of blocks in the sector
 * @param auth_key Authentication key to use
 * @param key_type Key type (A or B)
 * @return true if all blocks written successfully, false otherwise
 */
static bool write_sector_blocks(
    MiBandNfcApp* app,
    size_t sector,
    uint8_t first_block,
    uint8_t blocks_in_sector,
    MfClassicKey* auth_key,
    MfClassicKeyType key_type) {
    MfClassicError error;
    MfClassicAuthContext auth_context;

    // Write all data blocks (excluding the trailer)
    for(uint8_t block_in_sector = 0; block_in_sector < (blocks_in_sector - 1); block_in_sector++) {
        size_t block_idx = first_block + block_in_sector;

        // Re-authenticate before each block (except first)
        if(block_in_sector > 0) {
            error = mf_classic_poller_sync_auth(
                app->nfc, first_block, auth_key, key_type, &auth_context);
            if(error != MfClassicErrorNone) {
                FURI_LOG_E(TAG, "Re-auth failed for block %zu", block_idx);
                miband_logger_log(app->logger, LogLevelError, "Auth failed on sector %zu", sector);

                return false;
            }
        }

        // Write block with retry logic (up to 3 attempts)
        bool block_written = false;
        for(int retry = 0; retry < 3 && !block_written; retry++) {
            if(retry > 0) furi_delay_ms(100); // Delay between retries

            error = mf_classic_poller_sync_write_block(
                app->nfc, block_idx, auth_key, key_type, &app->mf_classic_data->block[block_idx]);

            if(error == MfClassicErrorNone) {
                block_written = true;
            } else if(error != MfClassicErrorTimeout) {
                break; // Non-timeout error, stop retrying
            }
        }

        if(!block_written) {
            FURI_LOG_E(TAG, "Failed to write block %zu", block_idx);
            return false;
        }
    }

    // Write the sector trailer
    size_t trailer_idx = first_block + blocks_in_sector - 1;
    bool trailer_written = false;

    for(int retry = 0; retry < 3 && !trailer_written; retry++) {
        if(retry > 0) {
            furi_delay_ms(100);
            // Re-authenticate before retry
            error = mf_classic_poller_sync_auth(
                app->nfc, first_block, auth_key, key_type, &auth_context);
            if(error != MfClassicErrorNone) {
                FURI_LOG_E(TAG, "Re-auth failed for trailer");
                miband_logger_log(app->logger, LogLevelError, "Auth failed on sector %zu", sector);

                return false;
            }
        }

        error = mf_classic_poller_sync_write_block(
            app->nfc, trailer_idx, auth_key, key_type, &app->mf_classic_data->block[trailer_idx]);

        if(error == MfClassicErrorNone) {
            trailer_written = true;
        } else if(error != MfClassicErrorTimeout) {
            break; // Non-timeout error, stop retrying
        }
    }

    if(!trailer_written) {
        FURI_LOG_E(TAG, "Failed to write trailer at block %zu", trailer_idx);
        return false;
    }

    FURI_LOG_I(TAG, "Sector %zu: Write SUCCESS", sector);
    return true;
}

/**
 * @brief Main write function using synchronous approach
 * 
 * This function implements a robust writing strategy:
 * 1. Detects card state (magic keys or original keys)
 * 2. Preserves Block 0 UID
 * 3. Writes all sectors with appropriate authentication
 * 4. Provides progress feedback
 * 
 * @param app Pointer to MiBandNfcApp instance
 * @return true if write successful, false otherwise
 */
static bool miband_write_with_sync_approach(MiBandNfcApp* app) {
    if(!app->is_valid_nfc_data) {
        return false;
    }

    if(app->logger) {
        miband_logger_log(app->logger, LogLevelInfo, "Starting sync write approach");
    }

    popup_set_text(app->popup, "Detecting card state...", 64, 24, AlignCenter, AlignTop);
    furi_delay_ms(100);

    MfClassicError error = MfClassicErrorNone;
    bool write_success = true;
    size_t total_blocks = mf_classic_get_total_block_num(app->mf_classic_data->type);
    size_t total_sectors = mf_classic_get_total_sectors_num(app->mf_classic_data->type);

    FURI_LOG_I(
        TAG, "Starting sync write for %zu blocks in %zu sectors", total_blocks, total_sectors);

    MfClassicType type = MfClassicType1k;
    MfClassicError detect_error = mf_classic_poller_sync_detect_type(app->nfc, &type);
    if(detect_error != MfClassicErrorNone) {
        FURI_LOG_E(TAG, "Card detection failed before write: %d", detect_error);
        popup_set_text(
            app->popup, "Card not detected\nCheck position", 64, 24, AlignCenter, AlignTop);
        furi_delay_ms(2000);
        return false;
    }

    FURI_LOG_I(TAG, "Card detected successfully, type: %d", type);

    bool has_magic_keys = false;
    MfClassicKey test_key = {0};
    memset(test_key.data, 0xFF, sizeof(test_key.data));
    MfClassicAuthContext test_auth;

    MfClassicError test_error =
        mf_classic_poller_sync_auth(app->nfc, 4, &test_key, MfClassicKeyTypeA, &test_auth);

    if(test_error == MfClassicErrorNone) {
        has_magic_keys = true;
        FURI_LOG_I(TAG, "Mi Band has 0xFF keys - first write scenario");
        popup_set_text(
            app->popup, "Magic keys detected\nWriting...", 64, 24, AlignCenter, AlignTop);
    } else {
        FURI_LOG_I(TAG, "Mi Band has dump keys - rewrite scenario");
        popup_set_text(
            app->popup, "Original keys detected\nRewriting...", 64, 24, AlignCenter, AlignTop);
    }

    furi_delay_ms(1000);

    for(size_t sector = 0; sector < total_sectors; sector++) {
        FURI_LOG_I(TAG, "Processing sector %zu...", sector);

        FuriString* progress_msg = furi_string_alloc();
        furi_string_printf(progress_msg, "Sector %zu/%zu\n\n", sector + 1, total_sectors);

        uint32_t progress_percent = ((sector + 1) * 100) / total_sectors;
        furi_string_cat_str(progress_msg, "[");
        for(uint8_t i = 0; i < 20; i++) {
            if(i < (progress_percent / 5)) {
                furi_string_cat_str(progress_msg, "=");
            } else if(i == (progress_percent / 5)) {
                furi_string_cat_str(progress_msg, ">");
            } else {
                furi_string_cat_str(progress_msg, " ");
            }
        }
        furi_string_cat_printf(progress_msg, "]\n%lu%%", progress_percent);

        // SOLO text update, NO reset
        popup_set_text(
            app->popup, furi_string_get_cstr(progress_msg), 64, 18, AlignCenter, AlignTop);
        furi_string_free(progress_msg);

        furi_delay_ms(100);

        uint8_t first_block = mf_classic_get_first_block_num_of_sector(sector);
        uint8_t blocks_in_sector = mf_classic_get_blocks_num_in_sector(sector);
        bool sector_written = false;

        MfClassicSectorTrailer* sec_tr =
            mf_classic_get_sector_trailer_by_sector(app->mf_classic_data, sector);
        MfClassicKey auth_key = {0};
        MfClassicKeyType auth_key_type = MfClassicKeyTypeA;
        MfClassicAuthContext auth_context;

        if(sector == 0) {
            FURI_LOG_I(TAG, "Sector 0: Special handling - preserving UID");

            MfClassicBlock current_block0;
            bool block0_read = false;

            memset(auth_key.data, 0xFF, sizeof(auth_key.data));
            error = mf_classic_poller_sync_auth(
                app->nfc, 0, &auth_key, MfClassicKeyTypeA, &auth_context);

            if(error == MfClassicErrorNone) {
                error = mf_classic_poller_sync_read_block(
                    app->nfc, 0, &auth_key, MfClassicKeyTypeA, &current_block0);
                if(error == MfClassicErrorNone) {
                    block0_read = true;
                    FURI_LOG_I(TAG, "Block 0 read with 0xFF keys");
                }
            }

            if(!block0_read &&
               mf_classic_is_key_found(app->mf_classic_data, 0, MfClassicKeyTypeA)) {
                memcpy(auth_key.data, sec_tr->key_a.data, sizeof(auth_key.data));
                error = mf_classic_poller_sync_auth(
                    app->nfc, 0, &auth_key, MfClassicKeyTypeA, &auth_context);

                if(error == MfClassicErrorNone) {
                    error = mf_classic_poller_sync_read_block(
                        app->nfc, 0, &auth_key, MfClassicKeyTypeA, &current_block0);
                    if(error == MfClassicErrorNone) {
                        block0_read = true;
                        FURI_LOG_I(TAG, "Block 0 read with dump Key A");
                    }
                }
            }

            if(!block0_read &&
               mf_classic_is_key_found(app->mf_classic_data, 0, MfClassicKeyTypeB)) {
                memcpy(auth_key.data, sec_tr->key_b.data, sizeof(auth_key.data));
                error = mf_classic_poller_sync_auth(
                    app->nfc, 0, &auth_key, MfClassicKeyTypeB, &auth_context);

                if(error == MfClassicErrorNone) {
                    error = mf_classic_poller_sync_read_block(
                        app->nfc, 0, &auth_key, MfClassicKeyTypeB, &current_block0);
                    if(error == MfClassicErrorNone) {
                        block0_read = true;
                        auth_key_type = MfClassicKeyTypeB;
                        FURI_LOG_I(TAG, "Block 0 read with dump Key B");
                    }
                }
            }

            if(!block0_read) {
                FURI_LOG_E(TAG, "CRITICAL: Cannot read Block 0!");
                write_success = false;
                break;
            }

            FURI_LOG_I(
                TAG,
                "Block 0 preserved: UID %02X%02X%02X%02X",
                current_block0.data[0],
                current_block0.data[1],
                current_block0.data[2],
                current_block0.data[3]);

            FURI_LOG_D(TAG, "Writing sector 0 data blocks (1 and 2)");

            for(uint8_t block_in_sector = 1; block_in_sector < (blocks_in_sector - 1);
                block_in_sector++) {
                size_t block_idx = first_block + block_in_sector;

                if(block_in_sector > 1) {
                    error = mf_classic_poller_sync_auth(
                        app->nfc, first_block, &auth_key, auth_key_type, &auth_context);
                    if(error != MfClassicErrorNone) {
                        FURI_LOG_E(TAG, "Re-auth failed for block %zu", block_idx);
                        miband_logger_log(
                            app->logger, LogLevelError, "Auth failed on sector %zu", sector);

                        write_success = false;
                        break;
                    }
                }

                bool block_written = false;
                for(int retry = 0; retry < 3 && !block_written; retry++) {
                    if(retry > 0) furi_delay_ms(100);

                    error = mf_classic_poller_sync_write_block(
                        app->nfc,
                        block_idx,
                        &auth_key,
                        auth_key_type,
                        &app->mf_classic_data->block[block_idx]);

                    if(error == MfClassicErrorNone) {
                        block_written = true;
                        FURI_LOG_D(TAG, "Block %zu written (retry %d)", block_idx, retry);
                    } else if(error != MfClassicErrorTimeout) {
                        break;
                    }
                }

                if(!block_written) {
                    FURI_LOG_E(TAG, "Failed to write block %zu", block_idx);
                    write_success = false;
                    break;
                }
            }

            if(write_success) {
                size_t trailer_idx = first_block + blocks_in_sector - 1;
                FURI_LOG_D(TAG, "Writing sector 0 trailer block %zu", trailer_idx);

                bool trailer_written = false;
                for(int retry = 0; retry < 3 && !trailer_written; retry++) {
                    if(retry > 0) {
                        furi_delay_ms(100);
                        error = mf_classic_poller_sync_auth(
                            app->nfc, first_block, &auth_key, auth_key_type, &auth_context);
                        if(error != MfClassicErrorNone) break;
                    }

                    error = mf_classic_poller_sync_write_block(
                        app->nfc,
                        trailer_idx,
                        &auth_key,
                        auth_key_type,
                        &app->mf_classic_data->block[trailer_idx]);

                    if(error == MfClassicErrorNone) {
                        trailer_written = true;
                        sector_written = true;
                        FURI_LOG_I(TAG, "Sector 0 trailer written (retry %d)", retry);
                    } else if(error != MfClassicErrorTimeout) {
                        break;
                    }
                }

                if(!trailer_written) {
                    FURI_LOG_E(TAG, "Failed to write sector 0 trailer");
                    write_success = false;
                }
            }

            continue;
        }

        if(has_magic_keys) {
            if(app->logger) {
                miband_logger_log(app->logger, LogLevelInfo, "Card has magic keys (0xFF)");
            }
            memset(auth_key.data, 0xFF, sizeof(auth_key.data));
            error = mf_classic_poller_sync_auth(
                app->nfc, first_block, &auth_key, MfClassicKeyTypeA, &auth_context);

            if(error == MfClassicErrorNone) {
                FURI_LOG_I(TAG, "Sector %zu: Auth with 0xFF", sector);
                sector_written = write_sector_blocks(
                    app, sector, first_block, blocks_in_sector, &auth_key, MfClassicKeyTypeA);
            }
        }
        if(app->logger) {
            miband_logger_log(app->logger, LogLevelInfo, "Card has original keys");
        }
        if(!sector_written) {
            if(mf_classic_is_key_found(app->mf_classic_data, sector, MfClassicKeyTypeA)) {
                memcpy(auth_key.data, sec_tr->key_a.data, sizeof(auth_key.data));

                for(int retry = 0; retry < 2 && !sector_written; retry++) {
                    if(retry > 0) furi_delay_ms(200);

                    error = mf_classic_poller_sync_auth(
                        app->nfc, first_block, &auth_key, MfClassicKeyTypeA, &auth_context);

                    if(error == MfClassicErrorNone) {
                        FURI_LOG_I(TAG, "Sector %zu: Auth with dump Key A", sector);
                        sector_written = write_sector_blocks(
                            app,
                            sector,
                            first_block,
                            blocks_in_sector,
                            &auth_key,
                            MfClassicKeyTypeA);
                        break;
                    }
                }
            }

            if(!sector_written &&
               mf_classic_is_key_found(app->mf_classic_data, sector, MfClassicKeyTypeB)) {
                memcpy(auth_key.data, sec_tr->key_b.data, sizeof(auth_key.data));

                for(int retry = 0; retry < 2 && !sector_written; retry++) {
                    if(retry > 0) furi_delay_ms(200);

                    error = mf_classic_poller_sync_auth(
                        app->nfc, first_block, &auth_key, MfClassicKeyTypeB, &auth_context);

                    if(error == MfClassicErrorNone) {
                        FURI_LOG_I(TAG, "Sector %zu: Auth with dump Key B", sector);
                        sector_written = write_sector_blocks(
                            app,
                            sector,
                            first_block,
                            blocks_in_sector,
                            &auth_key,
                            MfClassicKeyTypeB);
                        break;
                    }
                }
            }

            if(!sector_written && !has_magic_keys) {
                FURI_LOG_W(TAG, "Sector %zu: Trying 0xFF as last resort", sector);
                memset(auth_key.data, 0xFF, sizeof(auth_key.data));
                error = mf_classic_poller_sync_auth(
                    app->nfc, first_block, &auth_key, MfClassicKeyTypeA, &auth_context);

                if(error == MfClassicErrorNone) {
                    sector_written = write_sector_blocks(
                        app, sector, first_block, blocks_in_sector, &auth_key, MfClassicKeyTypeA);
                }
            }
        }

        if(!sector_written) {
            FURI_LOG_E(TAG, "Sector %zu: ALL authentication methods FAILED", sector);
            if(app->logger) {
                miband_logger_log(
                    app->logger,
                    LogLevelError,
                    "Failed to write sector %zu - all auth methods failed",
                    sector);
            }
            write_success = false;
            break;
        }
    }
    if(app->logger) {
        miband_logger_log(
            app->logger,
            write_success ? LogLevelInfo : LogLevelError,
            "Write operation %s",
            write_success ? "completed" : "failed");
    }
    FURI_LOG_I(TAG, "Sync write completed: %s", write_success ? "SUCCESS" : "FAILED");
    return write_success;
}

/**
 * @brief Scanner callback for card detection
 * 
 * Called when NFC scanner detects a card. Validates that it's a
 * Mifare Classic card before proceeding to write.
 * 
 * @param event NFC scanner event
 * @param context Pointer to MiBandNfcApp instance
 */
static void miband_nfc_scene_writer_scan_callback(NfcScannerEvent event, void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    if(event.type == NfcScannerEventTypeDetected) {
        // Log all detected protocols for debugging
        FURI_LOG_D(TAG, "Card detected with %zu protocols:", event.data.protocol_num);
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            FURI_LOG_D(TAG, "  Protocol %zu: %d", i, event.data.protocols[i]);
        }

        // Check if Mifare Classic is among detected protocols
        bool has_mf_classic = false;
        for(size_t i = 0; i < event.data.protocol_num; i++) {
            if(event.data.protocols[i] == NfcProtocolMfClassic) {
                has_mf_classic = true;
                FURI_LOG_I(TAG, "MfClassic found at index %zu", i);
                break;
            }
        }

        if(has_mf_classic) {
            FURI_LOG_I(TAG, "Mi Band valid for write operation");
            view_dispatcher_send_custom_event(
                app->view_dispatcher, MiBandNfcCustomEventCardDetected);
        } else {
            FURI_LOG_W(TAG, "No MfClassic protocol found");
            view_dispatcher_send_custom_event(app->view_dispatcher, MiBandNfcCustomEventWrongCard);
        }
    }
}

/**
 * @brief Scene entry point
 * 
 * Initializes scanner and waits for Mi Band to be placed near Flipper.
 * 
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_writer_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    if(!app->is_valid_nfc_data) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }
    if(app->logger) {
        miband_logger_log(
            app->logger,
            LogLevelInfo,
            "Write operation started for: %s",
            furi_string_get_cstr(app->file_path));
    }
    popup_reset(app->popup);

    if(app->current_operation == OperationTypeWriteOriginal) {
        popup_set_header(app->popup, "Write Original Data", 64, 4, AlignCenter, AlignTop);
    } else {
        popup_set_header(app->popup, "Write Data", 64, 4, AlignCenter, AlignTop);
    }

    popup_set_text(
        app->popup,
        "Place Mi Band near\nthe Flipper Zero\n\nWaiting for card...",
        64,
        20,
        AlignCenter,
        AlignTop);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdWriter);
    notification_message(app->notifications, &sequence_blink_start_cyan);

    app->poller = NULL;
    app->scanner = nfc_scanner_alloc(app->nfc);
    nfc_scanner_start(app->scanner, miband_nfc_scene_writer_scan_callback, app);
    app->is_scan_active = true;

    FURI_LOG_I(TAG, "Waiting for Mi Band to be placed...");
}

/**
 * @brief Scene event handler
 * 
 * Handles card detection and write completion events.
 * 
 * @param context Pointer to MiBandNfcApp instance
 * @param event Scene manager event
 * @return true if event was consumed, false otherwise
 */
bool miband_nfc_scene_writer_on_event(void* context, SceneManagerEvent event) {
    furi_assert(context);
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case MiBandNfcCustomEventCardDetected:
            if(app->scanner) {
                nfc_scanner_stop(app->scanner);
                nfc_scanner_free(app->scanner);
                app->scanner = NULL;
                app->is_scan_active = false;
            }

            popup_reset(app->popup);
            popup_set_header(app->popup, "Writing Data", 64, 4, AlignCenter, AlignTop);

            popup_set_text(app->popup, "Starting write...", 64, 18, AlignCenter, AlignTop);
            furi_delay_ms(500);

            if(app->logger) {
                miband_logger_log(app->logger, LogLevelInfo, "Card detected, starting write");
            }

            notification_message(app->notifications, &sequence_blink_stop);
            notification_message(app->notifications, &sequence_blink_start_magenta);

            FURI_LOG_I(TAG, "Starting write operation");
            bool write_result = miband_write_with_sync_approach(app);

            popup_reset(app->popup);

            if(write_result) {
                if(app->logger) {
                    miband_logger_log(
                        app->logger,
                        LogLevelInfo,
                        "Write successful for: %s",
                        furi_string_get_cstr(app->file_path));
                }
                notification_message(app->notifications, &sequence_success);
                popup_set_header(app->popup, "Write Success!", 64, 4, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup, "Data written\nsuccessfully", 64, 20, AlignCenter, AlignTop);
                popup_set_icon(app->popup, 32, 24, &I_DolphinSuccess_91x55);
                furi_delay_ms(2000);

                if(app->verify_after_write) {
                    popup_set_text(app->popup, "Auto-verifying...", 64, 20, AlignCenter, AlignTop);
                    furi_delay_ms(500);
                    scene_manager_next_scene(app->scene_manager, MiBandNfcSceneVerify);
                } else {
                    scene_manager_search_and_switch_to_another_scene(
                        app->scene_manager, MiBandNfcSceneMainMenu);
                }
            } else {
                if(app->logger) {
                    miband_logger_log(
                        app->logger,
                        LogLevelError,
                        "Write failed for: %s",
                        furi_string_get_cstr(app->file_path));
                }
                notification_message(app->notifications, &sequence_error);
                popup_set_header(app->popup, "Write Failed", 64, 4, AlignCenter, AlignTop);
                popup_set_text(
                    app->popup,
                    "Could not write\ndata to Mi Band\n\nCheck position",
                    64,
                    20,
                    AlignCenter,
                    AlignTop);
                popup_set_icon(app->popup, 40, 28, &I_WarningDolphinFlip_45x42);
                FURI_LOG_E(TAG, "Write failed");
            }

            furi_delay_ms(3000);
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, MiBandNfcSceneMainMenu);
            consumed = true;
            break;

        case MiBandNfcCustomEventWrongCard:
            if(app->logger) {
                miband_logger_log(app->logger, LogLevelWarning, "Wrong card type detected");
            }
            if(app->scanner) {
                nfc_scanner_stop(app->scanner);
                nfc_scanner_free(app->scanner);
                app->scanner = NULL;
                app->is_scan_active = false;
            }

            popup_set_header(app->popup, "Wrong Card Type", 64, 4, AlignCenter, AlignTop);
            popup_set_text(
                app->popup,
                "This is not a\nMifare Classic card\n\nTry again",
                64,
                20,
                AlignCenter,
                AlignTop);
            popup_set_icon(app->popup, 40, 28, &I_WarningDolphinFlip_45x42);
            notification_message(app->notifications, &sequence_error);

            furi_delay_ms(2000);
            scene_manager_search_and_switch_to_another_scene(
                app->scene_manager, MiBandNfcSceneMainMenu);
            consumed = true;
            break;

        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        scene_manager_search_and_switch_to_another_scene(
            app->scene_manager, MiBandNfcSceneMainMenu);
        consumed = true;
    }

    return consumed;
}
/**
 * @brief Scene exit handler
 * 
 * Stops scanner if active and cleans up resources.
 * 
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_writer_on_exit(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    if(app->scanner) {
        nfc_scanner_stop(app->scanner);
        nfc_scanner_free(app->scanner);
        app->scanner = NULL;
    }
    app->is_scan_active = false;

    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);
}
