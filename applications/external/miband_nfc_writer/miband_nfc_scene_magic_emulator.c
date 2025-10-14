/**
 * @file miband_nfc_scene_magic_emulator.c
 * @brief Magic card emulation scene - FIXED VERSION
 */

#include "miband_nfc_i.h"

#define TAG "MiBandNfc"

typedef struct {
    uint32_t auth_attempts;
    uint32_t successful_auths;
    uint32_t failed_auths;
    uint32_t reader_connections;
    uint32_t data_reads;
    FuriString* last_activity;
    FuriString* status_message;
    bool is_active;
    FuriTimer* update_timer;
} EmulationStats;

static EmulationStats* emulation_stats = NULL;

static void emulation_stats_free(void) {
    if(!emulation_stats) {
        return;
    }

    emulation_stats->is_active = false;

    if(emulation_stats->update_timer) {
        furi_timer_stop(emulation_stats->update_timer);
        furi_timer_free(emulation_stats->update_timer);
    }

    if(emulation_stats->last_activity) {
        furi_string_free(emulation_stats->last_activity);
    }
    if(emulation_stats->status_message) {
        furi_string_free(emulation_stats->status_message);
    }

    free(emulation_stats);
    emulation_stats = NULL;
}

static void emulation_timer_callback(void* context) {
    MiBandNfcApp* app = context;
    if(emulation_stats && emulation_stats->is_active) {
        popup_reset(app->popup);
        popup_set_header(app->popup, "Magic Template Active", 64, 2, AlignCenter, AlignTop);

        FuriString* stats_text = furi_string_alloc();
        furi_string_printf(stats_text, "UID+0xFF template\n");
        furi_string_cat_printf(
            stats_text,
            "Auth: %lu | OK %lu\n",
            emulation_stats->auth_attempts,
            emulation_stats->successful_auths);

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

static void emulation_stats_init(MiBandNfcApp* app) {
    // FIX: Libera risorse esistenti prima di reinizializzare
    if(emulation_stats) {
        emulation_stats_free();
    }

    emulation_stats = malloc(sizeof(EmulationStats));
    if(!emulation_stats) {
        FURI_LOG_E(TAG, "Failed to allocate emulation_stats");
        return;
    }

    emulation_stats->auth_attempts = 0;
    emulation_stats->successful_auths = 0;
    emulation_stats->failed_auths = 0;
    emulation_stats->reader_connections = 0;
    emulation_stats->data_reads = 0;
    emulation_stats->last_activity = furi_string_alloc();
    emulation_stats->status_message = furi_string_alloc();
    emulation_stats->is_active = true;

    emulation_stats->update_timer =
        furi_timer_alloc(emulation_timer_callback, FuriTimerTypePeriodic, app);
    furi_timer_start(emulation_stats->update_timer, 1000);

    furi_string_set_str(emulation_stats->status_message, "Template ready");
    furi_string_set_str(emulation_stats->last_activity, "Emulation started");
}

static void miband_nfc_magic_emulator_prepare_blank_template(MiBandNfcApp* app) {
    if(!app->is_valid_nfc_data) {
        return;
    }

    if(emulation_stats) {
        furi_string_set_str(emulation_stats->status_message, "Preparing template...");
        furi_string_set_str(emulation_stats->last_activity, "Template generation");
    }

    uint8_t original_block0_data[16];
    memcpy(original_block0_data, app->mf_classic_data->block[0].data, 16);

    uint8_t uid[4];
    memcpy(uid, app->mf_classic_data->block[0].data, 4);

    uint8_t bcc = uid[0] ^ uid[1] ^ uid[2] ^ uid[3];

    size_t total_sectors = mf_classic_get_total_sectors_num(app->mf_classic_data->type);
    size_t total_blocks = mf_classic_get_total_block_num(app->mf_classic_data->type);

    for(size_t block_idx = 0; block_idx < total_blocks; block_idx++) {
        memset(app->mf_classic_data->block[block_idx].data, 0x00, 16);
    }

    memcpy(app->mf_classic_data->block[0].data, uid, 4);
    app->mf_classic_data->block[0].data[4] = bcc;
    app->mf_classic_data->block[0].data[5] = 0x08;
    app->mf_classic_data->block[0].data[6] = 0x04;
    app->mf_classic_data->block[0].data[7] = 0x00;
    memcpy(&app->mf_classic_data->block[0].data[8], &original_block0_data[8], 8);

    for(size_t sector_idx = 0; sector_idx < total_sectors; sector_idx++) {
        uint8_t sector_trailer_block_idx;
        if(sector_idx < 32) {
            sector_trailer_block_idx = sector_idx * 4 + 3;
        } else {
            sector_trailer_block_idx = 32 * 4 + (sector_idx - 32) * 16 + 15;
        }

        MfClassicBlock* sector_trailer = &app->mf_classic_data->block[sector_trailer_block_idx];

        memset(&sector_trailer->data[0], 0xFF, 6);
        sector_trailer->data[6] = 0xFF;
        sector_trailer->data[7] = 0x07;
        sector_trailer->data[8] = 0x80;
        sector_trailer->data[9] = 0x69;
        memset(&sector_trailer->data[10], 0xFF, 6);

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

static NfcCommand miband_nfc_magic_emulator_callback(NfcGenericEvent event, void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    if(event.protocol == NfcProtocolMfClassic) {
        const MfClassicListenerEvent* mfc_event = event.event_data;

        if(mfc_event->type == MfClassicListenerEventTypeAuthContextPartCollected) {
            if(emulation_stats) {
                furi_string_set_str(emulation_stats->last_activity, "Auth data collected");
            }
            FURI_LOG_D(TAG, "Blank template: Auth part collected");

        } else if(mfc_event->type == MfClassicListenerEventTypeAuthContextFullCollected) {
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

            if(emulation_stats && emulation_stats->auth_attempts % 5 == 0) {
                view_dispatcher_send_custom_event(
                    app->view_dispatcher, MiBandNfcCustomEventCardDetected);
            }
            if(app->logger && emulation_stats && emulation_stats->auth_attempts % 10 == 0) {
                miband_logger_log(
                    app->logger,
                    LogLevelDebug,
                    "Magic emulation: %lu authentications completed",
                    emulation_stats->auth_attempts);
            }
            if(emulation_stats && emulation_stats->auth_attempts > 100) {
                furi_string_set_str(
                    emulation_stats->status_message, "Too many auth attempts - stopping");
                furi_string_set_str(emulation_stats->last_activity, "Safety stop triggered");
                FURI_LOG_W(
                    TAG, "Too many auth attempts (%lu), stopping", emulation_stats->auth_attempts);
                if(app->logger) {
                    miband_logger_log(
                        app->logger,
                        LogLevelWarning,
                        "Magic emulation stopped: too many auth attempts (%lu)",
                        emulation_stats->auth_attempts);
                }
                return NfcCommandStop;
            }
        }
    }

    return NfcCommandContinue;
}

void miband_nfc_scene_magic_emulator_on_enter(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;
    if(app->logger) {
        miband_logger_log(app->logger, LogLevelInfo, "Magic emulation started");
    }
    if(!app->is_valid_nfc_data) {
        scene_manager_previous_scene(app->scene_manager);
        return;
    }

    emulation_stats_init(app);
    miband_nfc_magic_emulator_prepare_blank_template(app);

    view_dispatcher_switch_to_view(app->view_dispatcher, MiBandNfcViewIdMagicEmulator);

    app->listener = nfc_listener_alloc(app->nfc, NfcProtocolMfClassic, app->mf_classic_data);
    nfc_listener_start(app->listener, miband_nfc_magic_emulator_callback, app);

    notification_message(app->notifications, &sequence_blink_start_cyan);
    emulation_timer_callback(app);
    if(app->logger) {
        uint8_t* uid = app->mf_classic_data->block[0].data;
        miband_logger_log(
            app->logger,
            LogLevelInfo,
            "Magic template prepared with UID: %02X %02X %02X %02X",
            uid[0],
            uid[1],
            uid[2],
            uid[3]);
    }
    FURI_LOG_I(TAG, "Started blank template emulation for Mi Band");
}

bool miband_nfc_scene_magic_emulator_on_event(void* context, SceneManagerEvent event) {
    MiBandNfcApp* app = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeCustom) {
        switch(event.event) {
        case MiBandNfcCustomEventCardDetected:
            if(emulation_stats) {
                furi_string_set_str(emulation_stats->status_message, "Mi Band actively reading");
                notification_message(app->notifications, &sequence_single_vibro);
            }
            consumed = true;
            break;
        default:
            break;
        }
    } else if(event.type == SceneManagerEventTypeBack) {
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

void miband_nfc_scene_magic_emulator_on_exit(void* context) {
    furi_assert(context);
    MiBandNfcApp* app = context;

    if(app->listener) {
        nfc_listener_stop(app->listener);
        nfc_listener_free(app->listener);
        app->listener = NULL;
    }
    if(app->logger && emulation_stats) {
        miband_logger_log(
            app->logger,
            LogLevelInfo,
            "Magic emulation ended: %lu total authentications",
            emulation_stats->auth_attempts);
    }
    emulation_stats_free();
    notification_message(app->notifications, &sequence_blink_stop);
    popup_reset(app->popup);

    FURI_LOG_I(TAG, "Stopped blank template emulation");
}
