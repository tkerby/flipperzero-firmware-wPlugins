// sli_writer.c
// Writes Magic ISO15693 SLIX cards from a Flipper .nfc file.
//
// Flow:
//   1) Parse .nfc file (UID + block count/size + data)
//   2) Detect card via ISO15693 inventory
//   3) Send Gen2 layout command (set block count)
//   4) Write data blocks (non-addressed, like Proxmark hf 15 wrbl)
//   5) Write UID last (non-addressed vendor cmd 0x40/0x41, like Proxmark hf 15 csetuid --v2)
//
// Based on the official iso15693_nfc_writer worker pattern (callback-driven, no custom thread).
// Card doc: https://www.aliexpress.com/item/... (Gen2 UID-changeable ISO15693 SLIX)

#include "sli_writer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <notification/notification.h>
#include <notification/notification_messages.h>

#define TAG "SLI_Writer"

/* ============================================================================
 *  ISO15693 / Magic command constants
 * ========================================================================== */

#define ISO15693_FLAG_HIGH_RATE   0x02   /* High data rate, non-addressed       */
#define ISO15693_CMD_WRITE_BLOCK  0x21   /* Write Single Block                  */

/* Gen2 magic UID commands (Proxmark: hf 15 csetuid --v2)
 * Frame format: 02 E0 09 <subcmd> <b0> <b1> <b2> <b3>
 *   hf 15 raw -c -d 03e00940<HIGH>  -> subcmd 0x40, bytes uid[7..4]
 *   hf 15 raw -c -d 03e00941<LOW>   -> subcmd 0x41, bytes uid[3..0]
 * Note: card stores UID LSB-first, so we send reversed bytes. */
#define SLI_MAGIC_SET_UID_HIGH    0x40
#define SLI_MAGIC_SET_UID_LOW     0x41

/* Gen2 layout command (Proxmark: hf 15 raw -acrk -d 02e00947 <code> 038b00)
 * Configures the number of exposed blocks on the magic card.
 * Block size is always 4 bytes on these cards (hardware fixed). */
#define GEN2_CFG_FLAGS            0x02
#define GEN2_CFG_CMD              0xE0
#define GEN2_CFG_OP_WRITE         0x09
#define GEN2_CFG_REG_BLOCKCFG     0x47
#define GEN2_CFG_B1               0x03   /* IC-specific config bytes (fixed)    */
#define GEN2_CFG_B2               0x8B
#define GEN2_CFG_B3               0x00

/* Layout codes — pattern is simply (block_count - 1):
 *   0x07 =  8 blocks, 0x0F = 16 blocks, 0x1F = 32 blocks, 0x3F = 64 blocks
 * Confirmed by Proxmark: hf 15 raw -acrk -d 02e00947 <code> 038b00 */
#define GEN2_LAYOUT_8_BLK         0x07
#define GEN2_LAYOUT_16_BLK        0x0F
#define GEN2_LAYOUT_32_BLK        0x1F
#define GEN2_LAYOUT_64_BLK        0x3F

/* FWT for iso15693_3_poller_send_frame(): value is in carrier cycles (fc).
 * 300000 fc / 13.56 MHz ≈ 22ms — enough for any ISO15693 card response. */
#define ISO15693_FWT_FC           500000  /* ~37ms — generous for slow magic cards */

/* ============================================================================
 *  Notification sequences (LED feedback)
 * ========================================================================== */

/* Blue blink while waiting for card */
static const NotificationSequence seq_blink_start = {
    &message_blue_255,
    &message_delay_50,
    &message_blue_0,
    NULL,
};

/* Green flash on success */
static const NotificationSequence seq_success = {
    &message_green_255,
    &message_delay_100,
    &message_green_0,
    &message_delay_50,
    &message_green_255,
    &message_delay_100,
    &message_green_0,
    NULL,
};

/* Red flash on error */
static const NotificationSequence seq_error = {
    &message_red_255,
    &message_delay_100,
    &message_red_0,
    NULL,
};

/* ============================================================================
 *  BitBuffer helpers
 * ========================================================================== */

static BitBuffer* bb_alloc_from(const uint8_t* data, size_t len) {
    BitBuffer* bb = bit_buffer_alloc(len * 8);
    if(!bb) return NULL;
    bit_buffer_reset(bb);
    if(data && len) bit_buffer_append_bytes(bb, data, len);
    return bb;
}

static BitBuffer* bb_alloc_rx(size_t max_bytes) {
    BitBuffer* bb = bit_buffer_alloc(max_bytes * 8);
    if(!bb) return NULL;
    bit_buffer_set_size(bb, 0);
    return bb;
}

/* ============================================================================
 *  Low-level ISO15693 helpers
 * ========================================================================== */

/* Returns true if the response flags byte has no error bit (bit 0 = 0) */
static bool iso_reply_ok(BitBuffer* rx) {
    if(bit_buffer_get_size_bytes(rx) < 1) return false;
    return (bit_buffer_get_byte(rx, 0) & 0x01) == 0;
}

/* Send a raw non-addressed frame and wait for response.
 * fwt_fc: frame wait time in carrier cycles (use ISO15693_FWT_FC). */
static bool iso_send_raw(
    Iso15693_3Poller* iso,
    const uint8_t* frame,
    size_t frame_len,
    uint32_t fwt_fc) {

    BitBuffer* tx = bb_alloc_from(frame, frame_len);
    if(!tx) return false;
    BitBuffer* rx = bb_alloc_rx(16);
    if(!rx) { bit_buffer_free(tx); return false; }

    Iso15693_3Error err = iso15693_3_poller_send_frame(
        iso, tx, rx, fwt_fc ? fwt_fc : ISO15693_FWT_FC);
    bool ok = (err == Iso15693_3ErrorNone) && iso_reply_ok(rx);

    if(!ok) FURI_LOG_W(TAG, "iso_send_raw: err=%d rxbytes=%u",
                       (int)err, (unsigned)bit_buffer_get_size_bytes(rx));
    bit_buffer_free(tx);
    bit_buffer_free(rx);
    return ok;
}

/* ============================================================================
 *  Gen2 magic: set block count layout
 * ========================================================================== */

/* Returns the layout code matching the block count from the .nfc file.
 * Pattern: layout_code = block_count - 1 (e.g. 8-1=0x07, 16-1=0x0F, ...) */
static uint8_t layout_code_for(uint8_t block_count) {
    if(block_count <= 8)  return GEN2_LAYOUT_8_BLK;
    if(block_count <= 16) return GEN2_LAYOUT_16_BLK;
    if(block_count <= 32) return GEN2_LAYOUT_32_BLK;
    return GEN2_LAYOUT_64_BLK;
}

/* Send the Gen2 vendor command to configure the number of exposed blocks.
 * Non-fatal: some readers don't care about this value, so we continue on failure. */
static bool magic_set_layout(Iso15693_3Poller* iso, uint8_t layout_code) {
    uint8_t frame[8] = {
        GEN2_CFG_FLAGS, GEN2_CFG_CMD,
        GEN2_CFG_OP_WRITE, GEN2_CFG_REG_BLOCKCFG,
        layout_code,
        GEN2_CFG_B1, GEN2_CFG_B2, GEN2_CFG_B3,
    };

    FURI_LOG_I(TAG, "Gen2 layout: 0x%02X (%u blocks)",
               layout_code, (layout_code + 1));

    for(int i = 0; i < 2; i++) {
        if(iso_send_raw(iso, frame, sizeof(frame), ISO15693_FWT_FC)) return true;
        furi_delay_ms(10);
    }

    FURI_LOG_W(TAG, "Gen2 layout cfg failed (continuing anyway)");
    return false;
}

/* ============================================================================
 *  Gen2 magic: write UID
 * ========================================================================== */

/* Write the UID using the Gen2 vendor command sequence (Proxmark: hf 15 csetuid --v2).
 *
 * The card stores UID LSB-first internally, so we send bytes in reverse order:
 *   HIGH frame: 02 E0 09 40 uid[7] uid[6] uid[5] uid[4]
 *   LOW  frame: 02 E0 09 41 uid[3] uid[2] uid[1] uid[0]
 *
 * IMPORTANT (from card vendor): data blocks must be written BEFORE changing
 * the UID. After UID change, the original UID is required to modify blocks again.
 */
static bool magic_write_uid(Iso15693_3Poller* iso, const uint8_t uid_new[8]) {
    uint8_t frame_high[8] = {
        0x02, 0xE0, 0x09, SLI_MAGIC_SET_UID_HIGH,
        uid_new[7], uid_new[6], uid_new[5], uid_new[4]
    };
    uint8_t frame_low[8] = {
        0x02, 0xE0, 0x09, SLI_MAGIC_SET_UID_LOW,
        uid_new[3], uid_new[2], uid_new[1], uid_new[0]
    };

    FURI_LOG_I(TAG, "Magic UID HIGH: %02X%02X%02X%02X",
               uid_new[0], uid_new[1], uid_new[2], uid_new[3]);
    bool ok = false;
    for(int i = 0; i < 3; i++) {
        if(iso_send_raw(iso, frame_high, sizeof(frame_high), ISO15693_FWT_FC)) {
            ok = true; break;
        }
        furi_delay_ms(10);
    }
    if(!ok) { FURI_LOG_E(TAG, "SET_UID_HIGH failed"); return false; }
    furi_delay_ms(20);

    FURI_LOG_I(TAG, "Magic UID LOW:  %02X%02X%02X%02X",
               uid_new[4], uid_new[5], uid_new[6], uid_new[7]);
    ok = false;
    for(int i = 0; i < 3; i++) {
        if(iso_send_raw(iso, frame_low, sizeof(frame_low), ISO15693_FWT_FC)) {
            ok = true; break;
        }
        furi_delay_ms(10);
    }
    if(!ok) { FURI_LOG_E(TAG, "SET_UID_LOW failed"); return false; }

    return true;
}

/* ============================================================================
 *  Write data blocks
 * ========================================================================== */

/* Write all blocks using non-addressed Write Single Block (like Proxmark hf 15 wrbl).
 * Block size is clamped to 4 bytes (hardware maximum for SLIX cards).
 * Each block is retried up to 3 times on failure. */
static bool write_blocks(
    Iso15693_3Poller* iso,
    const uint8_t* data,
    uint8_t block_count,
    uint8_t block_size) {

    if(block_size == 0) block_size = 4;
    if(block_size > 4)  block_size = 4;  /* SLIX hardware: 4 bytes/block max */

    size_t blocks = (block_count > SLI_MAGIC_MAX_BLOCKS) ?
                     SLI_MAGIC_MAX_BLOCKS : block_count;

    FURI_LOG_I(TAG, "Writing %u blocks x %u bytes", (unsigned)blocks, (unsigned)block_size);

    for(size_t b = 0; b < blocks; b++) {
        /* Frame: flags(0x02) | cmd(0x21) | block_num | 4 data bytes */
        uint8_t wframe[7];
        wframe[0] = 0x02;
        wframe[1] = ISO15693_CMD_WRITE_BLOCK;
        wframe[2] = (uint8_t)b;
        memcpy(&wframe[3], &data[b * block_size], block_size);
        if(block_size < 4) memset(&wframe[3 + block_size], 0x00, 4 - block_size);

        bool wrote = false;
        for(int attempt = 0; attempt < 5 && !wrote; attempt++) {
            wrote = iso_send_raw(iso, wframe, sizeof(wframe), ISO15693_FWT_FC);
            if(!wrote) furi_delay_ms(20);
        }

        if(!wrote) {
            FURI_LOG_E(TAG, "Write block %u failed", (unsigned)b);
            return false;
        }

        furi_delay_ms(20); /* inter-block delay for card NVM write cycle */
    }

    return true;
}

/* ============================================================================
 *  .nfc file parser
 * ========================================================================== */

/* Parse a Flipper .nfc file and fill app->nfc_data.
 * Supports SLIX / ISO15693-3 format. Uses strtol for robust hex parsing.
 *
 * Expected fields:
 *   UID: <8 hex bytes space-separated>
 *   Block Count: <decimal>
 *   Block Size: <decimal>
 *   Data Content: <hex bytes space-separated>
 */
bool sli_writer_parse_nfc_file(SliWriterApp* app, const char* path) {
    File* f = storage_file_alloc(app->storage);
    bool ok = false;

    if(storage_file_open(f, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t sz = storage_file_size(f);
        char* buf = malloc(sz + 1);
        if(buf) {
            size_t n = storage_file_read(f, buf, sz);
            buf[n] = '\0';

            /* Safe defaults */
            memset(&app->nfc_data, 0x00, sizeof(app->nfc_data));
            app->nfc_data.block_size  = 4;
            app->nfc_data.block_count = 0;

            char* p;

            if((p = strstr(buf, "UID: "))) {
                p += 5;
                /* Parse 8 UID bytes, skipping spaces between hex pairs */
                char* q = p;
                for(int i = 0; i < 8; i++) {
                    while(*q == ' ') q++;
                    app->nfc_data.uid[i] = (uint8_t)strtol(q, &q, 16);
                }

                if((p = strstr(buf, "Block Count: ")))
                    sscanf(p + 13, "%hhu", &app->nfc_data.block_count);

                if((p = strstr(buf, "Block Size: ")))
                    sscanf(p + 12, "%hhu", &app->nfc_data.block_size);

                if((p = strstr(buf, "Data Content: "))) {
                    p += 14;
                    size_t total = (size_t)app->nfc_data.block_count *
                                   app->nfc_data.block_size;
                    if(total > sizeof(app->nfc_data.data))
                        total = sizeof(app->nfc_data.data);
                    char* q2 = p;
                    for(size_t i = 0; i < total; i++) {
                        while(*q2 == ' ') q2++;
                        app->nfc_data.data[i] = (uint8_t)strtol(q2, &q2, 16);
                    }
                }

                FURI_LOG_I(TAG, "Parsed: blocks=%u size=%u uid=%02X%02X%02X%02X%02X%02X%02X%02X",
                           app->nfc_data.block_count, app->nfc_data.block_size,
                           app->nfc_data.uid[0], app->nfc_data.uid[1],
                           app->nfc_data.uid[2], app->nfc_data.uid[3],
                           app->nfc_data.uid[4], app->nfc_data.uid[5],
                           app->nfc_data.uid[6], app->nfc_data.uid[7]);
                ok = true;
            }
            free(buf);
        }
        storage_file_close(f);
    }

    storage_file_free(f);
    return ok;
}

/* ============================================================================
 *  Full write sequence: layout -> blocks -> UID
 * ========================================================================== */

static bool do_write_operations(SliWriterApp* app, Iso15693_3Poller* iso) {

    /* Step 1: Configure Gen2 block count layout to match the .nfc file.
     * This is non-fatal — many readers only check the first N blocks anyway. */
    magic_set_layout(iso, layout_code_for(app->nfc_data.block_count));
    furi_delay_ms(50); /* let card settle after layout change */

    /* Step 2: Write data blocks FIRST using the card's current (original) UID.
     * Per vendor doc: "data blocks must be written before changing the uid.
     * After changing the uid, the original uid is required for future modifications." */
    FURI_LOG_I(TAG, "Step 1: write %u data blocks", app->nfc_data.block_count);
    if(!write_blocks(iso,
                     app->nfc_data.data,
                     app->nfc_data.block_count,
                     app->nfc_data.block_size)) {
        furi_string_set(app->error_message, "Block write failed");
        return false;
    }
    FURI_LOG_I(TAG, "Blocks written OK");

    /* Step 3: Write UID LAST (only if it differs from the card's current UID) */
    static const uint8_t zero_uid[8] = {0};
    bool uid_is_zero = (memcmp(app->nfc_data.uid, zero_uid, 8) == 0);
    bool uid_differs = (memcmp(app->nfc_data.uid, app->detected_uid, 8) != 0);

    if(!uid_is_zero && uid_differs) {
        FURI_LOG_I(TAG, "Step 2: write UID");
        if(!magic_write_uid(iso, app->nfc_data.uid)) {
            furi_string_set(app->error_message, "UID write failed");
            return false;
        }
        FURI_LOG_I(TAG, "UID written OK");
    } else {
        FURI_LOG_I(TAG, "Step 2: UID skip (zero or same as card)");
    }

    return true;
}

/* ============================================================================
 *  NFC poller callback (official pattern from iso15693_nfc_writer)
 *  All RF work happens here. Returns NfcCommandStop when done.
 * ========================================================================== */
static NfcCommand sli_poller_callback(NfcGenericEvent event, void* context) {
    SliWriterApp* app = context;

    if(event.protocol != NfcProtocolIso15693_3)
        return NfcCommandContinue;

    Iso15693_3PollerEvent* iso_event = event.event_data;

    /* Ignore non-Ready events (e.g. errors during activation) */
    if(iso_event->type != Iso15693_3PollerEventTypeReady)
        return NfcCommandContinue;

    Iso15693_3Poller* iso = event.instance;
    bool success = false;

    /* Get card UID via inventory.
     * iso15693_3_poller_inventory() returns UID in LSB-first (wire) order.
     * We reverse it to MSB-first to match the .nfc file format. */
    uint8_t uid_lsb[8];
    Iso15693_3Error uid_err = iso15693_3_poller_inventory(iso, uid_lsb);
    if(uid_err == Iso15693_3ErrorNone) {
        for(int i = 0; i < 8; i++) app->detected_uid[i] = uid_lsb[7 - i];
        app->have_uid = true;

        FURI_LOG_I(TAG, "Card UID (MSB first): %02X%02X%02X%02X%02X%02X%02X%02X",
                   app->detected_uid[0], app->detected_uid[1],
                   app->detected_uid[2], app->detected_uid[3],
                   app->detected_uid[4], app->detected_uid[5],
                   app->detected_uid[6], app->detected_uid[7]);

        furi_delay_ms(20); /* short delay after inventory before sending commands */
        success = do_write_operations(app, iso);
    } else {
        FURI_LOG_E(TAG, "Inventory failed: err=%d", (int)uid_err);
        furi_string_set(app->error_message, "Card inventory failed");
    }

    /* LED feedback */
    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);
    if(success) {
        notification_message(notif, &seq_success);
    } else {
        notification_message(notif, &seq_error);
    }
    furi_record_close(RECORD_NOTIFICATION);

    /* Notify UI (thread-safe via view_dispatcher queue) */
    view_dispatcher_send_custom_event(
        app->view_dispatcher,
        success ? SliWriterCustomEventWriteSuccess : SliWriterCustomEventWriteError);

    return NfcCommandStop;
}

/* ============================================================================
 *  UI Callbacks
 * ========================================================================== */
void sli_writer_submenu_callback(void* context, uint32_t index) {
    SliWriterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void sli_writer_dialog_ex_callback(DialogExResult result, void* context) {
    SliWriterApp* app = context;
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

bool sli_writer_custom_event_callback(void* context, uint32_t event) {
    SliWriterApp* app = context;
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

bool sli_writer_back_event_callback(void* context) {
    SliWriterApp* app = context;
    return scene_manager_handle_back_event(app->scene_manager);
}

/* ============================================================================
 *  Scenes
 * ========================================================================== */

/* --- Start --- */
void sli_writer_scene_start_on_enter(void* context) {
    SliWriterApp* app = context;
    submenu_reset(app->submenu);
    submenu_add_item(app->submenu, "Write NFC File",
        SliWriterSubmenuIndexWrite, sli_writer_submenu_callback, app);
    submenu_add_item(app->submenu, "About",
        SliWriterSubmenuIndexAbout, sli_writer_submenu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewSubmenu);
}

bool sli_writer_scene_start_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SliWriterSubmenuIndexWrite) {
            app->test_mode = false;
            scene_manager_next_scene(app->scene_manager, SliWriterSceneFileSelect);
            return true;
        } else if(event.event == SliWriterSubmenuIndexAbout) {
            dialog_ex_reset(app->dialog_ex);
            dialog_ex_set_header(app->dialog_ex, "SLI Writer", 64, 0, AlignCenter, AlignTop);
            dialog_ex_set_text(app->dialog_ex,
                "Write Magic ISO15693\nSLI/SLI-S/SLI-L cards\nSLIX support coming soon",
                64, 32, AlignCenter, AlignCenter);
            dialog_ex_set_left_button_text(app->dialog_ex, "Back");
            view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
            return true;
        }
    }
    return false;
}

void sli_writer_scene_start_on_exit(void* context) {
    SliWriterApp* app = context;
    submenu_reset(app->submenu);
}

/* --- File Select --- */
void sli_writer_scene_file_select_on_enter(void* context) {
    SliWriterApp* app = context;
    furi_string_set(app->file_path, "/ext/nfc");

    DialogsFileBrowserOptions opts;
    dialog_file_browser_set_basic_options(&opts, SLI_WRITER_FILE_EXTENSION, NULL);

    if(dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &opts)) {
        /* Parse the file before starting NFC — show error immediately if invalid */
        if(sli_writer_parse_nfc_file(app, furi_string_get_cstr(app->file_path))) {
            scene_manager_next_scene(app->scene_manager, SliWriterSceneWrite);
        } else {
            furi_string_set(app->error_message, "Cannot parse .nfc file");
            scene_manager_next_scene(app->scene_manager, SliWriterSceneError);
        }
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool sli_writer_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    (void)context; (void)event;
    return false;
}

void sli_writer_scene_file_select_on_exit(void* context) {
    (void)context;
}

/* --- Write: starts NFC poller and shows "Approach card" loading screen --- */
void sli_writer_scene_write_on_enter(void* context) {
    SliWriterApp* app = context;

    /* Show "Approach card" message via DialogEx (Loading widget has no text) */
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "SLI Writer", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog_ex, "Approach card\nto Flipper...",
                       64, 32, AlignCenter, AlignCenter);
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);

    /* Blue LED blink to indicate ready-to-scan */
    NotificationApp* notif = furi_record_open(RECORD_NOTIFICATION);
    notification_message(notif, &seq_blink_start);
    furi_record_close(RECORD_NOTIFICATION);

    app->have_uid = false;
    furi_string_reset(app->error_message);

    /* Start NFC poller (official pattern: alloc -> alloc poller -> start) */
    app->nfc    = nfc_alloc();
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso15693_3);
    app->nfc_started = true;

    nfc_poller_start(app->poller, sli_poller_callback, app);

    FURI_LOG_I(TAG, "scene_write: poller started, waiting for card...");
}

bool sli_writer_scene_write_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SliWriterCustomEventWriteSuccess) {
            scene_manager_next_scene(app->scene_manager, SliWriterSceneSuccess);
            return true;
        } else if(event.event == SliWriterCustomEventWriteError) {
            scene_manager_next_scene(app->scene_manager, SliWriterSceneError);
            return true;
        }
    }
    return false;
}

void sli_writer_scene_write_on_exit(void* context) {
    SliWriterApp* app = context;

    if(app->nfc_started) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        nfc_free(app->nfc);
        app->poller      = NULL;
        app->nfc         = NULL;
        app->nfc_started = false;
    }
}

/* --- Success --- */
void sli_writer_scene_success_on_enter(void* context) {
    SliWriterApp* app = context;
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Write Success!", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog_ex, "Card written successfully",
                       64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "OK");
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
}

bool sli_writer_scene_success_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type == SceneManagerEventTypeBack ||
       (event.type == SceneManagerEventTypeCustom &&
        event.event == DialogExResultLeft)) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SliWriterSceneStart);
        return true;
    }
    return false;
}

void sli_writer_scene_success_on_exit(void* context) {
    SliWriterApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}

/* --- Error --- */
void sli_writer_scene_error_on_enter(void* context) {
    SliWriterApp* app = context;
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Error", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog_ex, furi_string_get_cstr(app->error_message),
                       64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "OK");
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
}

bool sli_writer_scene_error_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type == SceneManagerEventTypeBack ||
       (event.type == SceneManagerEventTypeCustom &&
        event.event == DialogExResultLeft)) {
        scene_manager_search_and_switch_to_previous_scene(
            app->scene_manager, SliWriterSceneStart);
        return true;
    }
    return false;
}

void sli_writer_scene_error_on_exit(void* context) {
    SliWriterApp* app = context;
    dialog_ex_reset(app->dialog_ex);
}

/* ============================================================================
 *  Scene manager dispatch tables
 * ========================================================================== */
static void (*const sli_scene_on_enter[])(void*) = {
    sli_writer_scene_start_on_enter,
    sli_writer_scene_file_select_on_enter,
    sli_writer_scene_write_on_enter,
    sli_writer_scene_success_on_enter,
    sli_writer_scene_error_on_enter,
};

static bool (*const sli_scene_on_event[])(void*, SceneManagerEvent) = {
    sli_writer_scene_start_on_event,
    sli_writer_scene_file_select_on_event,
    sli_writer_scene_write_on_event,
    sli_writer_scene_success_on_event,
    sli_writer_scene_error_on_event,
};

static void (*const sli_scene_on_exit[])(void*) = {
    sli_writer_scene_start_on_exit,
    sli_writer_scene_file_select_on_exit,
    sli_writer_scene_write_on_exit,
    sli_writer_scene_success_on_exit,
    sli_writer_scene_error_on_exit,
};

static const SceneManagerHandlers sli_scene_handlers = {
    .on_enter_handlers = sli_scene_on_enter,
    .on_event_handlers = sli_scene_on_event,
    .on_exit_handlers  = sli_scene_on_exit,
    .scene_num         = SliWriterSceneNum,
};

/* ============================================================================
 *  App lifecycle
 * ========================================================================== */
SliWriterApp* sli_writer_app_alloc(void) {
    SliWriterApp* app = malloc(sizeof(SliWriterApp));
    furi_check(app);
    memset(app, 0x00, sizeof(SliWriterApp));

    app->gui     = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->view_dispatcher = view_dispatcher_alloc();
    app->scene_manager   = scene_manager_alloc(&sli_scene_handlers, app);

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, sli_writer_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, sli_writer_back_event_callback);

    app->submenu = submenu_alloc();
    view_dispatcher_add_view(app->view_dispatcher,
        SliWriterViewSubmenu, submenu_get_view(app->submenu));

    app->dialog_ex = dialog_ex_alloc();
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, sli_writer_dialog_ex_callback);
    view_dispatcher_add_view(app->view_dispatcher,
        SliWriterViewDialogEx, dialog_ex_get_view(app->dialog_ex));

    app->loading = loading_alloc();
    view_dispatcher_add_view(app->view_dispatcher,
        SliWriterViewLoading, loading_get_view(app->loading));

    app->file_path     = furi_string_alloc();
    app->error_message = furi_string_alloc();

    view_dispatcher_attach_to_gui(
        app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    scene_manager_next_scene(app->scene_manager, SliWriterSceneStart);

    return app;
}

void sli_writer_app_free(SliWriterApp* app) {
    furi_assert(app);

    /* Safety: stop NFC if app is closed during a write */
    if(app->nfc_started) {
        nfc_poller_stop(app->poller);
        nfc_poller_free(app->poller);
        nfc_free(app->nfc);
    }

    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewDialogEx);
    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewLoading);

    submenu_free(app->submenu);
    dialog_ex_free(app->dialog_ex);
    loading_free(app->loading);

    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);

    furi_string_free(app->file_path);
    furi_string_free(app->error_message);

    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);

    free(app);
}

/* ============================================================================
 *  Entry point
 * ========================================================================== */
int32_t sli_writer_app(void* p) {
    UNUSED(p);
    SliWriterApp* app = sli_writer_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    sli_writer_app_free(app);
    return 0;
}
