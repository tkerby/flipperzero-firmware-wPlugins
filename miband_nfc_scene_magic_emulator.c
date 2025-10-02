/**
 * @file miband_nfc_scene_magic_emulator.c
 * @brief Magic card emulation scene
 * 
 * This scene emulates a "blank" Mifare Classic card with all data blocks zeroed
 * and all keys set to 0xFF (magic card format). This prepares the Mi Band NFC
 * for writing by creating a template that can be authenticated with magic keys.
 * 
 * The emulation preserves the original UID from the dump file and sets up
 * sector trailers with magic access bits that allow easy read/write access.
 */

#include "miband_nfc_i.h"

#define TAG "MiBandNfc"

/**
 * @brief Emulation statistics tracker
 * 
 * Tracks authentication attempts, reader connections, and other metrics
 * during emulation to provide real-time feedback to the user.
 */
typedef struct {
    uint32_t auth_attempts; // Total authentication attempts
    uint32_t successful_auths; // Successful authentications
    uint32_t failed_auths; // Failed authentications
    uint32_t reader_connections; // Number of times reader connected
    uint32_t data_reads; // Number of data block reads
    FuriString* last_activity; // Description of last activity
    FuriString* status_message; // Current status message
    bool is_active; // True if emulation is running
    FuriTimer* update_timer; // Timer for periodic UI updates
} EmulationStats;

static EmulationStats* emulation_stats = NULL;

/**
 * @brief Timer callback for periodic UI updates
 * 
 * Called every second to refresh the emulation statistics display.
 * Shows connection count, authentication stats, and current status.
 * 
 * @param context Pointer to MiBandNfcApp instance
 */
static void emulation_timer_callback(void* context) {
    MiBandNfcApp* app = context;
    if(emulation_stats && emulation_stats->is_active) {
        popup_reset(app->popup);

        popup_set_header(app->popup, "Magic Template Active", 64, 2, AlignCenter, AlignTop);

        FuriString* stats_text = furi_string_alloc();
        furi_string_printf(stats_text, "UID+0xFF template\n");
        // furi_string_cat_printf(
        //     stats_text, "Connections: %lu\n", emulation_stats->reader_connections);
        furi_string_cat_printf(
            stats_text,
            "Auth: %lu | OK %lu\n",
            emulation_stats->auth_attempts,
            emulation_stats->successful_auths);
        // furi_string_cat_printf(stats_text, "  Success: %lu\n", emulation_stats->successful_auths);
        // furi_string_cat_printf(stats_text, "  Failed: %lu\n", emulation_stats->failed_auths);
        // furi_string_cat_printf(stats_text, "Data reads: %lu\n", emulation_stats->data_reads);

        if(furi_string_size(emulation_stats->last_activity) > 0) {
            furi_string_cat_printf(
                stats_text, "%s\n", furi_string_get_cstr(emulation_stats->last_activity));
        }

        if(furi_string_size(emulation_stats->status_message) > 0) {
            furi_string_cat_printf(
                stats_text, "%s\n", furi_string_get_cstr(emulation_stats->status_message));
        }

        furi_string_cat_str(stats_text, "Back=Stop");

        popup_set_text(app->popup, furi_string_get_cstr(stats_text), 2, 12, AlignLeft, AlignTop);
        popup_set_icon(app->popup, 68, 20, &I_NFC_dolphin_emulation_51x64);

        furi_string_free(stats_text);
    }
}

/**
 * @brief Initialize emulation statistics tracking
 * 
 * Allocates and initializes the statistics structure and starts
 * the periodic update timer.
 * 
 * @param app Pointer to MiBandNfcApp instance
 */
static void emulation_stats_init(MiBandNfcApp* app) {
    if(emulation_stats) {
        return; // Already initialized
    }

    emulation_stats = malloc(sizeof(EmulationStats));
    emulation_stats->auth_attempts = 0;
    emulation_stats->successful_auths = 0;
    emulation_stats->failed_auths = 0;
    emulation_stats->reader_connections = 0;
    emulation_stats->data_reads = 0;
    emulation_stats->last_activity = furi_string_alloc();
    emulation_stats->status_message = furi_string_alloc();
    emulation_stats->is_active = true;

    // Create timer for UI updates every second
    emulation_stats->update_timer =
        furi_timer_alloc(emulation_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(emulation_stats->update_timer, 1000);

    furi_string_set_str(emulation_stats->status_message, "Template ready");
    furi_string_set_str(emulation_stats->last_activity, "Emulation started");
}

/**
 * @brief Free emulation statistics and stop timer
 */
static void emulation_stats_free(void) {
    if(!emulation_stats) {
        return;
    }

    emulation_stats->is_active = false;

    if(emulation_stats->update_timer) {
        furi_timer_stop(emulation_stats->update_timer);
        furi_timer_free(emulation_stats->update_timer);
    }

    furi_string_free(emulation_stats->last_activity);
    furi_string_free(emulation_stats->status_message);
    free(emulation_stats);
    emulation_stats = NULL;
}

/**
 * @brief Prepare blank magic card template from loaded dump
 * 
 * This function creates a magic card template by:
 * 1. Preserving the original UID from Block 0
 * 2. Zeroing all data blocks
 * 3. Setting all keys to 0xFF (magic keys)
 * 4. Configuring magic access bits in sector trailers
 * 
 * @param app Pointer to MiBandNfcApp instance
 */
static void miband_nfc_magic_emulator_prepare_blank_template(MiBandNfcApp* app) {
    if(!app->is_valid_nfc_data) {
        return;
    }

    if(emulation_stats) {
        furi_string_set_str(emulation_stats->status_message, "Preparing template...");
        furi_string_set_str(emulation_stats->last_activity, "Template generation");
    }

    // Save original Block 0 for UID preservation
    uint8_t original_block0_data[16];
    memcpy(original_block0_data, app->mf_classic_data->block[0].data, 16);

    // Extract UID (first 4 bytes)
    uint8_t uid[4];
    memcpy(uid, app->mf_classic_data->block[0].data, 4);

    // Calculate BCC (XOR of all UID bytes)
    uint8_t bcc = uid[0] ^ uid[1] ^ uid[2] ^ uid[3];

    // Get card structure information
    size_t total_sectors = mf_classic_get_total_sectors_num(app->mf_classic_data->type);
    size_t total_blocks = mf_classic_get_total_block_num(app->mf_classic_data->type);

    // Zero all blocks completely
    for(size_t block_idx = 0; block_idx < total_blocks; block_idx++) {
        memset(app->mf_classic_data->block[block_idx].data, 0x00, 16);
    }

    // Reconstruct Block 0 with proper Mifare Classic structure
    memcpy(app->mf_classic_data->block[0].data, uid, 4); // UID
    app->mf_classic_data->block[0].data[4] = bcc; // BCC
    app->mf_classic_data->block[0].data[5] = 0x08; // SAK for Classic 1K
    app->mf_classic_data->block[0].data[6] = 0x04; // ATQA byte 1
    app->mf_classic_data->block[0].data[7] = 0x00; // ATQA byte 2
    // Preserve bytes 8-15 from original (manufacturer data)
    memcpy(&app->mf_classic_data->block[0].data[8], &original_block0_data[8], 8);

    // Configure all sector trailers with magic card format
    for(size_t sector_idx = 0; sector_idx < total_sectors; sector_idx++) {
        // Calculate sector trailer block index
        uint8_t sector_trailer_block_idx;
        if(sector_idx < 32) {
            // First 32 sectors: 4 blocks each
            sector_trailer_block_idx = sector_idx * 4 + 3;
        } else {
            // Remaining sectors: 16 blocks each
            sector_trailer_block_idx = 32 * 4 + (sector_idx - 32) * 16 + 15;
        }

        MfClassicBlock* sector_trailer = &app->mf_classic_data->block[sector_trailer_block_idx];

        // Set magic card sector trailer format:
        memset(&sector_trailer->data[0], 0xFF, 6); // Key A: FF FF FF FF FF FF
        sector_trailer->data[6] = 0xFF; // Access bits byte 0
        sector_trailer->data[7] = 0x07; // Access bits byte 1
        sector_trailer->data[8] = 0x80; // Access bits byte 2
        sector_trailer->data[9] = 0x69; // User byte
        memset(&sector_trailer->data[10], 0xFF, 6); // Key B: FF FF FF FF FF FF

        // Enable key masks to indicate keys are available
        FURI_BIT_SET(app->mf_classic_data->key_a_mask, sector_idx);
        FURI_BIT_SET(app->mf_classic_data->key_b_mask, sector_idx);
    }

    if(emulation_stats) {
        furi_string_printf(
            emulation_stats->status_message,
            "UID %02X%02X%02X%02X",
            uid[0],
            uid[1],
            uid[2],
            uid[3]);
        furi_string_set_str(emulation_stats->last_activity, "Template ready");
    }

    FURI_LOG_I(
        TAG,
        "Prepared blank Mi Band template - UID: %02X%02X%02X%02X",
        uid[0],
        uid[1],
        uid[2],
        uid[3]);
}

/**
 * @brief NFC listener callback for emulation events
 * 
 * Handles authentication events during emulation and updates statistics.
 * Provides feedback after multiple authentications to show progress.
 * 
 * @param event NFC generic event
 * @param context Pointer to MiBandNfcApp instance
 * @return NfcCommand to continue or stop emulation
 */
static NfcCommand miband_nfc_magic_emulator_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    if(event.protocol == NfcProtocolMfClassic) {
        const MfClassicListenerEvent* mfc_event = event.event_data;

        if(mfc_event->type == MfClassicListenerEventTypeAuthContextPartCollected) {
            // Partial authentication data collected
            if(emulation_stats) {
                furi_string_set_str(emulation_stats->last_activity, "Auth data collected");
            }
            FURI_LOG_D(TAG, "Blank template: Auth part collected");

        } else if(mfc_event->type == MfClassicListenerEventTypeAuthContextFullCollected) {
            // Full authentication completed successfully
            if(emulation_stats) {
                emulation_stats->auth_attempts++;
                emulation_stats->successful_auths++;
                furi_string_printf(
                    emulation_stats->last_activity,
                    "Auth #%lu complete",
                    emulation_stats->auth_attempts);
                furi_string_set_str(emulation_stats->status_message, "Authentication successful");
            }

            FURI_LOG_D(
                TAG,
                "Blank template: Auth complete #%lu",
                emulation_stats ? emulation_stats->auth_attempts : 0);

            // Provide feedback every 5 authentications
            if(emulation_stats && emulation_stats->auth_attempts % 5 == 0) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, MiBandNfcCustomEventCardDetected);
            }

            // Safety limit: Stop if too many authentications (indicates potential issue)
            if(emulation_stats && emulation_stats->auth_attempts > 100) {
                furi_string_set_str(
                    emulation_stats->status_message, "Too many auth attempts - stopping");
                furi_string_set_str(emulation_stats->last_activity, "Safety stop triggered");
                FURI_LOG_W(
                    TAG, "Too many auth attempts (%lu), stopping", emulation_stats->auth_attempts);
                return NfcCommandStop;
            }
        }
    }

    return NfcCommandContinue;
}

/**
 * @brief Scene entry point
 * 
 * Initializes emulation, prepares the magic card template, and starts
 * the NFC listener to emulate the card.
 * 
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_magic_emulator_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    if(!app->is_valid_nfc_data) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    // Initialize statistics tracking
    emulation_stats_init(app);

    // Prepare the blank magic template
    miband_nfc_magic_emulator_prepare_blank_template(app);

    // Switch to emulator view
    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdMagicEmulator);

    // Start NFC listener for emulation
    app->listener = nfc_listener_alloc(app->nfc, NfcProtocolMfClassic, app->mf_classic_data);
    nfc_listener_start(app->listener, miband_nfc_magic_emulator_callback, app);

    // Start visual notification (cyan blinking LED)
    notification_message(app->notifications, &sequence_blink_start_cyan);

    // Initial UI update
    emulation_timer_callback(app);

    FURI_LOG_I(TAG, "Started blank template emulation for Mi Band");
}

/**
 * @brief Scene event handler
 * 
 * Handles card detection events and back button press to stop emulation.
 * 
 * @param context Pointer to MiBandNfcApp instance
 * @param event Scene manager event
 * @return true if event was consumed, false otherwise
 */
bool miband_nfc_scene_magic_emulator_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case MiBandNfcCustomEventCardDetected:
            // Periodic feedback during active reading
            if(emulation_stats) {
                furi_string_set_str(emulation_stats->status_message, "Mi Band actively reading");
                // Provide tactile feedback
                notification_message(app->notifications, &sequence_single_vibro);
            }
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
        // User pressed back - stop emulation
        if(app->listener) {
            nfc_listener_stop(app->listener);
            nfc_listener_free(app->listener);
            app->listener = NULL;
        }

        emulation_stats_free();
        scene_manager_previous_scene(app->scene_manager);
        consumed = true;
    }

    return consumed;
}

/**
 * @brief Scene exit handler
 * 
 * Stops emulation, cleans up resources, and stops LED notifications.
 * 
 * @param context Pointer to MiBandNfcApp instance
 */
void miband_nfc_scene_magic_emulator_on_exit(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    if(app->listener) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        app->listener = NULL;
    }

    emulation_stats_free();
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);

    FURI_LOG_I(TAG, "Stopped blank template emulation");
}
