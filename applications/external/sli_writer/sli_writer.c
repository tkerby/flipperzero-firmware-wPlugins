// sli_writer.c - Version worker-only NFC: callback NfcCommand enrichi,
// INVENTORY robuste, BitBuffer OOM-safe, clamp+padding block_size,
// Magic SLI write with retries, worker interruptible, logs détaillés.

#include "sli_writer.h"
#include <furi.h>
#include <furi_hal.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define TAG "SLI_Writer_App"

// ============================================================================
// Helpers pour BitBuffer (Version sécurisée)
// ============================================================================
static BitBuffer* bb_from_bytes_const(const uint8_t* data, size_t len_bytes) {
    BitBuffer* bb = bit_buffer_alloc(len_bytes * 8);
    if(!bb) return NULL;
    bit_buffer_reset(bb);
    if(data && len_bytes) bit_buffer_append_bytes(bb, data, len_bytes);
    return bb;
}

static BitBuffer* bb_alloc_for_rx(size_t max_len_bytes) {
    BitBuffer* bb = bit_buffer_alloc(max_len_bytes * 8);
    if(!bb) return NULL;
    bit_buffer_set_size(bb, 0);
    return bb;
}

// ============================================================================
// Helpers de communication ISO15693
// ============================================================================
// Gestion robuste de la réponse INVENTORY (support 9 ou 10 octets)
static bool iso15693_inventory_get_uid(Iso15693_3Poller* iso, uint8_t out_uid[8]) {
    uint8_t tx[3] = {
        (uint8_t)(0x04 /*INVENTORY*/ | 0x02 /*HIGH RATE*/),
        0x01 /*INVENTORY*/,
        0x00 /*mask length 0*/
    };
    BitBuffer* txbb = bb_from_bytes_const(tx, sizeof(tx));
    if(!txbb) return false;
    BitBuffer* rxbb = bb_alloc_for_rx(24);
    if(!rxbb) {
        bit_buffer_free(txbb);
        return false;
    }

    bool ok = false;
    if(iso15693_3_poller_send_frame(iso, txbb, rxbb, 50) == Iso15693_3ErrorNone) {
        size_t bits = bit_buffer_get_size(rxbb);
        if(bits >= 80) { // Flags(1) + DSFID(1) + UID(8) = 10 bytes
            uint8_t buf10[10] = {0};
            bit_buffer_write_bytes(rxbb, buf10, 10);
            memcpy(out_uid, &buf10[2], 8);
            ok = true;
        } else if(bits >= 72) { // Flags(1) + UID(8) = 9 bytes
            uint8_t buf9[9] = {0};
            bit_buffer_write_bytes(rxbb, buf9, 9);
            memcpy(out_uid, &buf9[1], 8);
            ok = true;
        } else {
            FURI_LOG_W(TAG, "Inventory reply too short: %u bits", (unsigned)bits);
        }
    }

    bit_buffer_free(txbb);
    bit_buffer_free(rxbb);
    return ok;
}

// Envoi générique ISO15693 en mode "addressed" + parsing du FLAGS d'erreur
static bool iso15693_send_addressed_raw(
    Iso15693_3Poller* iso,
    uint8_t cmd,
    const uint8_t uid[8],
    const uint8_t* payload,
    size_t payload_len,
    uint32_t fwt_ms) {
    uint8_t frame[64];
    size_t len = 0;
    frame[len++] = (uint8_t)(0x20 /*ADDRESSED*/ | 0x02 /*HIGH RATE*/);
    frame[len++] = cmd;
    memcpy(&frame[len], uid, 8);
    len += 8;

    if(payload && payload_len) {
        if(len + payload_len > sizeof(frame)) return false;
        memcpy(&frame[len], payload, payload_len);
        len += payload_len;
    }

    BitBuffer* txbb = bb_from_bytes_const(frame, len);
    if(!txbb) return false;
    BitBuffer* rxbb = bb_alloc_for_rx(24);
    if(!rxbb) {
        bit_buffer_free(txbb);
        return false;
    }

    bool ok = false;
    Iso15693_3Error err = iso15693_3_poller_send_frame(iso, txbb, rxbb, fwt_ms ? fwt_ms : 60);
    if(err == Iso15693_3ErrorNone) {
        const size_t rx_bits = bit_buffer_get_size(rxbb);
        const size_t rx_bytes = (rx_bits + 7U) / 8U;
        if(rx_bytes >= 1) {
            uint8_t rxb[2] = {0};
            bit_buffer_write_bytes(rxbb, rxb, rx_bytes >= 2 ? 2 : 1);
            uint8_t flags = rxb[0];
            if(flags & 0x01) {
                uint8_t err_code = (rx_bytes >= 2) ? rxb[1] : 0xFF;
                FURI_LOG_E(TAG, "ISO15693 error: flags=0x%02X code=0x%02X", flags, err_code);
            } else {
                ok = true;
            }
        } else {
            FURI_LOG_E(TAG, "ISO15693 short reply: %u bits", (unsigned)rx_bits);
        }
    } else {
        FURI_LOG_E(TAG, "send_frame err=%d", (int)err);
    }

    bit_buffer_free(txbb);
    bit_buffer_free(rxbb);
    return ok;
}

// ============================================================================
// Helpers pour commandes Magic (avec retries)
// ============================================================================
static bool magic_send(
    Iso15693_3Poller* iso,
    const uint8_t uid_addr[8],
    uint8_t subcmd,
    const uint8_t* data,
    size_t len) {
    uint8_t payload[16];
    payload[0] = subcmd;
    if(data && len > 0) {
        if(1 + len > sizeof(payload)) return false;
        memcpy(&payload[1], data, len);
    }
    for(int attempt = 0; attempt < 3; attempt++) {
        if(iso15693_send_addressed_raw(iso, 0xA0 /*CUSTOM*/, uid_addr, payload, len + 1, 60)) {
            return true;
        }
        furi_delay_ms(8);
    }
    return false;
}

static bool
    write_uid_magic(Iso15693_3Poller* iso, const uint8_t uid_new[8], const uint8_t uid_addr[8]) {
    uint8_t uid_high[4] = {uid_new[7], uid_new[6], uid_new[5], uid_new[4]};
    uint8_t uid_low[4] = {uid_new[3], uid_new[2], uid_new[1], uid_new[0]};
    if(!magic_send(iso, uid_addr, 0x36 /*INIT*/, NULL, 0)) return false;
    furi_delay_ms(30);
    if(!magic_send(iso, uid_addr, 0x37 /*SET_UID_HIGH*/, uid_high, sizeof(uid_high))) return false;
    furi_delay_ms(30);
    if(!magic_send(iso, uid_addr, 0x38 /*SET_UID_LOW*/, uid_low, sizeof(uid_low))) return false;
    furi_delay_ms(30);
#if SLI_FINALIZE_WITH_PAYLOAD
    if(!magic_send(iso, uid_addr, 0x39 /*FINALIZE*/, uid_low, sizeof(uid_low))) return false;
#else
    if(!magic_send(iso, uid_addr, 0x39 /*FINALIZE*/, NULL, 0)) return false;
#endif
    return true;
}

static bool write_blocks(
    Iso15693_3Poller* iso,
    const uint8_t uid_addr[8],
    const uint8_t* data,
    uint8_t block_count,
    uint8_t block_size) {
    size_t blocks = (block_count > SLI_MAGIC_MAX_BLOCKS) ? SLI_MAGIC_MAX_BLOCKS : block_count;
    if(block_size == 0) block_size = 4; // défaut
    if(block_size > 4) block_size = 4; // clamp pour SLIX
    for(size_t b = 0; b < blocks; b++) {
        uint8_t payload[1 + 4];
        payload[0] = (uint8_t)b;
        size_t have = (block_size < 4) ? block_size : 4;
        memcpy(&payload[1], &data[b * block_size], have);
        if(have < 4) memset(&payload[1 + have], 0x00, 4 - have); // padding
        bool wrote = false;
        for(int attempt = 0; attempt < 3 && !wrote; attempt++) {
            wrote = iso15693_send_addressed_raw(
                iso, 0x21 /*WRITE_SINGLE_BLOCK*/, uid_addr, payload, 1 + 4, 80);
            if(!wrote) furi_delay_ms(10);
        }
        if(!wrote) {
            FURI_LOG_E(TAG, "Write block %zu failed", b);
            return false;
        }
        furi_delay_ms(12);
    }
    return true;
}

// ============================================================================
// PARSING DE FICHIER (NFC Parser)
// ============================================================================
bool sli_writer_parse_nfc_file(SliWriterApp* app, const char* file_path) {
    File* file = storage_file_alloc(app->storage);
    bool parsed = false;
    if(storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        size_t file_size = storage_file_size(file);
        char* buf = malloc(file_size + 1);
        if(buf) {
            size_t read_len = storage_file_read(file, buf, file_size);
            buf[read_len] = '\0';
            char* p;
            // Defaults
            app->nfc_data.block_count = 0;
            app->nfc_data.block_size = 4;
            if((p = strstr(buf, "UID: "))) {
                p += 5;
                for(int i = 0; i < 8; i++)
                    sscanf(p + i * 3, "%2hhx", &app->nfc_data.uid[i]);
                if((p = strstr(buf, "Block Count: ")))
                    sscanf(p + 13, "%hhu", &app->nfc_data.block_count);
                if((p = strstr(buf, "Block Size: ")))
                    sscanf(p + 12, "%hhu", &app->nfc_data.block_size);
                if((p = strstr(buf, "Data Content: "))) {
                    p += 14;
                    for(size_t i = 0;
                        i < (size_t)app->nfc_data.block_count * app->nfc_data.block_size;
                        i++) {
                        sscanf(p + i * 3, "%2hhx", &app->nfc_data.data[i]);
                    }
                }
                parsed = true;
            }
            free(buf);
        }
    }
    storage_file_close(file);
    storage_file_free(file);
    return parsed;
}

static bool slix_writer_do_write_operations(SliWriterApp* app, Iso15693_3Poller* iso) {
    if(!sli_writer_parse_nfc_file(app, furi_string_get_cstr(app->file_path))) {
        furi_string_set(app->error_message, "Parse failed");
        return false;
    }
    uint8_t current_uid[8];
    memcpy(current_uid, app->detected_uid, 8);
    if(memcmp(app->nfc_data.uid, "\x00\x00\x00\x00\x00\x00\x00\x00", 8) != 0) {
        if(!write_uid_magic(iso, app->nfc_data.uid, current_uid)) {
            furi_string_set(app->error_message, "Failed to write UID");
            return false;
        }
        memcpy(current_uid, app->nfc_data.uid, 8);
    }
    if(!write_blocks(
           iso,
           current_uid,
           app->nfc_data.data,
           app->nfc_data.block_count,
           app->nfc_data.block_size)) {
        furi_string_set(app->error_message, "Failed to write blocks");
        return false;
    }
    return true;
}

// ============================================================================
// Callback (BY VALUE, correct SDK signature; add error handling, no NfcEventTypeError case)
// ============================================================================
static NfcCommand nfc_event_cb(NfcEvent event, void* context) {
    SliWriterApp* app = context;
    furi_assert(app);

    FURI_LOG_D(TAG, "cb: event type %d", event.type);

    switch(event.type) {
    case NfcEventTypePollerReady: {
        if(!app->poller_ready) {
            app->poller_ready = true;
            FURI_LOG_I(TAG, "cb: PollerReady");
            view_dispatcher_send_custom_event(
                app->view_dispatcher, SliWriterCustomEventPollerReady);
        }
        break;
    }
    case NfcEventTypeFieldOn: {
        app->field_present = true;
        FURI_LOG_D(TAG, "cb: Field ON");
        break;
    }
    case NfcEventTypeFieldOff: {
        app->field_present = false;
        FURI_LOG_D(TAG, "cb: Field OFF");
        break;
    }
    default: {
        FURI_LOG_D(TAG, "cb: unknown type %d", event.type);
        break;
    }
    }
    return NfcCommandContinue;
}

// ============================================================================
// WORKER THREAD (gère tout le cycle NFC)
// ============================================================================
int32_t slix_writer_work_thread(void* context) {
    SliWriterApp* app = context;
    furi_assert(app);
    bool success = false;
    FURI_LOG_I(TAG, "worker: enter");
    // Remet l'état RF au propre (au cas où une app précédente a laissé le front-end ON)
    // furi_hal_nfc_abort();
    // furi_delay_ms(20);
    // 1) Alloc NFC
    app->nfc = nfc_alloc();
    if(!app->nfc) {
        FURI_LOG_E(TAG, "worker: nfc_alloc FAILED");
        furi_string_set(app->error_message, "NFC alloc failed");
        goto done_send_event;
    }
    FURI_LOG_I(TAG, "worker: nfc_alloc ok");
    // 2) Config NFC
    nfc_config(app->nfc, NfcModePoller, NfcTechIso15693);
    FURI_LOG_I(TAG, "worker: nfc_config ok");
    // 3) Démarre le core NFC AVEC LE CALLBACK
    FURI_LOG_I(TAG, "worker: nfc_start()");
    nfc_start(app->nfc, nfc_event_cb, app);
    // Petite pause pour laisser le driver s'initialiser
    furi_delay_ms(80);
    /* Also wait for the callback to signal PollerReady, but cap it */
    for(uint32_t spins = 0; spins < 10 && !app->poller_ready; spins++) {
        furi_delay_ms(20);
    }
    FURI_LOG_I(TAG, "worker: poller_alloc");
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso15693_3);
    if(!app->poller) {
        FURI_LOG_E(TAG, "worker: poller_alloc FAILED");
        furi_string_set(app->error_message, "Poller alloc failed");
        goto stop_nfc_and_free;
    }
    FURI_LOG_I(TAG, "worker: poller_start");
    nfc_poller_start(app->poller, NULL, NULL);
    furi_delay_ms(20);
    // 5) Boucle de détection
    FURI_LOG_I(TAG, "worker: detect loop");
    bool detected = false;
    const uint32_t max_attempts = 50; // ~5 s
    for(uint32_t i = 0; i < max_attempts && app->running; ++i) {
        // (optionnel) si tu utilises nfc_start + callback, profite du flag field_present
        if(app->field_present) {
            if(nfc_poller_detect(app->poller)) {
                detected = true;
                break;
            }
        }
        // petit log toutes les 10 itérations pour tracer sans spammer
        if((i % 10) == 0) {
            FURI_LOG_D(TAG, "detect... (%lu/%lu)", (unsigned long)i, (unsigned long)max_attempts);
        }
        furi_delay_ms(100);
    }
    FURI_LOG_I(TAG, "worker: detect=%d", detected);
    if(detected) {
        Iso15693_3Poller* iso = (Iso15693_3Poller*)nfc_poller_get_data(app->poller);
        if(iso && iso15693_inventory_get_uid(iso, app->detected_uid)) {
            app->have_uid = true;
            if(app->test_mode) {
                char uid_str[64];
                snprintf(
                    uid_str,
                    sizeof(uid_str),
                    "UID: %02X%02X%02X%02X\n%02X%02X%02X%02X",
                    app->detected_uid[7],
                    app->detected_uid[6],
                    app->detected_uid[5],
                    app->detected_uid[4],
                    app->detected_uid[3],
                    app->detected_uid[2],
                    app->detected_uid[1],
                    app->detected_uid[0]);
                furi_string_set(app->error_message, uid_str);
                success = true;
            } else {
                success = slix_writer_do_write_operations(app, iso);
            }
        } else {
            FURI_LOG_E(TAG, "worker: Read UID failed");
            furi_string_set(app->error_message, "Read UID failed");
            success = false;
        }
    } else {
        furi_string_set(app->error_message, "No card detected");
        success = false;
    }
    // 6) Teardown du poller
    FURI_LOG_I(TAG, "worker: poller_stop/free");
    nfc_poller_stop(app->poller);
    nfc_poller_free(app->poller);
    app->poller = NULL;
stop_nfc_and_free:
    // 7) Stop + free NFC
    FURI_LOG_I(TAG, "worker: nfc_stop/free");
    nfc_stop(app->nfc);
    nfc_free(app->nfc);
    app->nfc = NULL;
done_send_event:
    if(furi_thread_get_state(furi_thread_get_current()) != FuriThreadStateStopped) {
        view_dispatcher_send_custom_event(
            app->view_dispatcher,
            success ? SliWriterCustomEventWriteSuccess : SliWriterCustomEventWriteError);
    }
    FURI_LOG_I(TAG, "worker: exit (success=%d)", success);
    return 0;
}

// ============================================================================
// Fonctions de Callback pour l'UI
// ============================================================================
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

// ============================================================================
// Définitions des Scènes
// ============================================================================
void sli_writer_scene_start_on_enter(void* context) {
    SliWriterApp* app = context;
    FURI_LOG_I(TAG, "scene_start: enter");
    submenu_reset(app->submenu);
    submenu_add_item(
        app->submenu,
        "Write from File",
        SliWriterSubmenuIndexWrite,
        sli_writer_submenu_callback,
        app);
    submenu_add_item(
        app->submenu,
        "Read ISO15693 UID",
        SliWriterSubmenuIndexTestMagic,
        sli_writer_submenu_callback,
        app);
    submenu_add_item(
        app->submenu, "About", SliWriterSubmenuIndexAbout, sli_writer_submenu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewSubmenu);
}
bool sli_writer_scene_start_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SliWriterSubmenuIndexWrite) {
            app->test_mode = false;
            scene_manager_next_scene(app->scene_manager, SliWriterSceneFileSelect);
            return true;
        } else if(event.event == SliWriterSubmenuIndexTestMagic) {
            app->test_mode = true;
            scene_manager_next_scene(app->scene_manager, SliWriterSceneWrite);
            return true;
        } else if(event.event == SliWriterSubmenuIndexAbout) {
            dialog_ex_set_header(app->dialog_ex, "SLI Writer", 64, 0, AlignCenter, AlignTop);
            dialog_ex_set_text(
                app->dialog_ex,
                "Write to Magic ISO15693\nSLI/SLIX Cards",
                64,
                32,
                AlignCenter,
                AlignCenter);
            dialog_ex_set_left_button_text(app->dialog_ex, "Back");
            view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
            return true;
        }
    }
    return false;
}
void sli_writer_scene_start_on_exit(void* context) {
    SliWriterApp* app = context;
    FURI_LOG_I(TAG, "scene_start: exit");
    submenu_reset(app->submenu);
}
// Scène: File Select
void sli_writer_scene_file_select_on_enter(void* context) {
    SliWriterApp* app = context;
    FURI_LOG_I(TAG, "scene_file_select: enter");
    furi_string_set(app->file_path, "/ext/nfc");
    DialogsFileBrowserOptions options;
    dialog_file_browser_set_basic_options(&options, SLI_WRITER_FILE_EXTENSION, NULL);
    if(dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &options)) {
        scene_manager_next_scene(app->scene_manager, SliWriterSceneWrite);
    } else {
        scene_manager_previous_scene(app->scene_manager);
    }
}
bool sli_writer_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    (void)context;
    (void)event;
    return false;
}
void sli_writer_scene_file_select_on_exit(void* context) {
    FURI_LOG_I(TAG, "scene_file_select: exit");
    (void)context;
}
// Scène: Write (lance seulement le worker)
void sli_writer_scene_write_on_enter(void* context) {
    SliWriterApp* app = context;
    FURI_LOG_I(TAG, "scene_write: enter");
    // UI: écran de chargement
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewLoading);
    // Réinit état
    app->have_uid = false;
    furi_string_reset(app->error_message);
    // Arme le flag de boucle du worker (assure-toi d'avoir `bool running;` dans le .h)
    app->running = true;
    // Démarre le worker si pas déjà lancé
    if(!app->worker_thread) {
        app->worker_thread =
            furi_thread_alloc_ex("sli_writer_worker", 4096, slix_writer_work_thread, app);
        furi_thread_start(app->worker_thread);
        FURI_LOG_I(TAG, "scene_write: worker started");
    } else {
        FURI_LOG_W(TAG, "scene_write: worker already running (ignored)");
    }
}
bool sli_writer_scene_write_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SliWriterCustomEventPollerReady) {
            if(!app->worker_thread) {
                app->worker_thread =
                    furi_thread_alloc_ex("sli_writer_worker", 4096, slix_writer_work_thread, app);
                furi_thread_start(app->worker_thread);
            }
            return true;
        } else if(event.event == SliWriterCustomEventWriteSuccess) { // <<< ADD THIS
            FURI_LOG_I(TAG, "scene_write: success");
            scene_manager_next_scene(app->scene_manager, SliWriterSceneSuccess);
            return true;
        } else if(event.event == SliWriterCustomEventWriteError) {
            FURI_LOG_I(TAG, "scene_write: error");
            scene_manager_next_scene(app->scene_manager, SliWriterSceneError);
            return true;
        }
    }
    return false;
}
void sli_writer_scene_write_on_exit(void* context) {
    SliWriterApp* app = context;
    FURI_LOG_I(TAG, "scene_write: exit");
    // Demande l'arrêt propre du worker
    app->running = false;
    // Joindre/libérer le worker si présent
    if(app->worker_thread) {
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
        FURI_LOG_I(TAG, "scene_write: worker joined & freed");
    }
    // Le worker libère déjà poller + nfc ; on s’assure juste de ne rien laisser traîner
    app->poller = NULL;
    app->nfc = NULL;
}
// Scène: Success
void sli_writer_scene_success_on_enter(void* context) {
    SliWriterApp* app = context;
    FURI_LOG_I(TAG, "scene_success: enter");
    dialog_ex_reset(app->dialog_ex);
    if(app->test_mode) {
        dialog_ex_set_header(app->dialog_ex, "Read Success", 64, 0, AlignCenter, AlignTop);
        dialog_ex_set_text(
            app->dialog_ex,
            furi_string_get_cstr(app->error_message),
            64,
            22,
            AlignCenter,
            AlignTop);
    } else {
        dialog_ex_set_header(app->dialog_ex, "Write Success!", 64, 0, AlignCenter, AlignTop);
        dialog_ex_set_text(
            app->dialog_ex, "Card written successfully", 64, 32, AlignCenter, AlignCenter);
    }
    dialog_ex_set_left_button_text(app->dialog_ex, "OK");
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
}
bool sli_writer_scene_success_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type == SceneManagerEventTypeBack ||
       (event.type == SceneManagerEventTypeCustom && event.event == DialogExResultLeft)) {
        FURI_LOG_I(TAG, "scene_success: back");
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SliWriterSceneStart);
        return true;
    }
    return false;
}
void sli_writer_scene_success_on_exit(void* context) {
    SliWriterApp* app = context;
    FURI_LOG_I(TAG, "scene_success: exit");
    dialog_ex_reset(app->dialog_ex);
}
// Scène: Error
void sli_writer_scene_error_on_enter(void* context) {
    SliWriterApp* app = context;
    FURI_LOG_I(TAG, "scene_error: enter");
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Error", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(
        app->dialog_ex, furi_string_get_cstr(app->error_message), 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "OK");
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
}
bool sli_writer_scene_error_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = context;
    if(event.type == SceneManagerEventTypeBack ||
       (event.type == SceneManagerEventTypeCustom && event.event == DialogExResultLeft)) {
        FURI_LOG_I(TAG, "scene_error: back");
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SliWriterSceneStart);
        return true;
    }
    return false;
}
void sli_writer_scene_error_on_exit(void* context) {
    SliWriterApp* app = context;
    FURI_LOG_I(TAG, "scene_error: exit");
    dialog_ex_reset(app->dialog_ex);
}
// ============================================================================
// GESTIONNAIRE DE SCENES
// ============================================================================
static void (*const sli_writer_scene_on_enter_handlers[])(void*) = {
    sli_writer_scene_start_on_enter,
    sli_writer_scene_file_select_on_enter,
    sli_writer_scene_write_on_enter,
    sli_writer_scene_success_on_enter,
    sli_writer_scene_error_on_enter,
};
static bool (*const sli_writer_scene_on_event_handlers[])(void*, SceneManagerEvent) = {
    sli_writer_scene_start_on_event,
    sli_writer_scene_file_select_on_event,
    sli_writer_scene_write_on_event,
    sli_writer_scene_success_on_event,
    sli_writer_scene_error_on_event,
};
static void (*const sli_writer_scene_on_exit_handlers[])(void*) = {
    sli_writer_scene_start_on_exit,
    sli_writer_scene_file_select_on_exit,
    sli_writer_scene_write_on_exit,
    sli_writer_scene_success_on_exit,
    sli_writer_scene_error_on_exit,
};
static const SceneManagerHandlers sli_writer_scene_handlers = {
    .on_enter_handlers = sli_writer_scene_on_enter_handlers,
    .on_event_handlers = sli_writer_scene_on_event_handlers,
    .on_exit_handlers = sli_writer_scene_on_exit_handlers,
    .scene_num = SliWriterSceneNum,
};
// ============================================================================
// Cycle de vie de l'application
// ============================================================================
SliWriterApp* sli_writer_app_alloc(void) {
    SliWriterApp* app = malloc(sizeof(SliWriterApp));
    furi_check(app);
    app->gui = furi_record_open(RECORD_GUI);
    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);
    app->view_dispatcher = view_dispatcher_alloc();
    view_dispatcher_enable_queue(app->view_dispatcher);
    app->scene_manager = scene_manager_alloc(&sli_writer_scene_handlers, app);
    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(
        app->view_dispatcher, sli_writer_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(
        app->view_dispatcher, sli_writer_back_event_callback);
    app->submenu = submenu_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SliWriterViewSubmenu, submenu_get_view(app->submenu));
    app->dialog_ex = dialog_ex_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SliWriterViewDialogEx, dialog_ex_get_view(app->dialog_ex));
    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, sli_writer_dialog_ex_callback);
    app->loading = loading_alloc();
    view_dispatcher_add_view(
        app->view_dispatcher, SliWriterViewLoading, loading_get_view(app->loading));
    app->file_path = furi_string_alloc();
    app->error_message = furi_string_alloc();
    // États runtime
    app->nfc = NULL;
    app->poller = NULL;
    app->worker_thread = NULL;
    app->have_uid = false;
    app->test_mode = false;
    /* AJOUTS */
    app->poller_ready = false;
    app->field_present = false;
    app->fatal_error = false;
    app->running = false;
    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);
    scene_manager_next_scene(app->scene_manager, SliWriterSceneStart);
    return app;
}
void sli_writer_app_free(SliWriterApp* app) {
    furi_assert(app);
    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewSubmenu);
    submenu_free(app->submenu);
    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewDialogEx);
    dialog_ex_free(app->dialog_ex);
    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewLoading);
    loading_free(app->loading);
    view_dispatcher_free(app->view_dispatcher);
    scene_manager_free(app->scene_manager);
    furi_string_free(app->file_path);
    furi_string_free(app->error_message);
    // Le worker libère NFC/poller ; ici on vérifie juste qu'ils sont bien NULL
    furi_assert(app->nfc == NULL);
    furi_assert(app->poller == NULL);
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_STORAGE);
    furi_record_close(RECORD_DIALOGS);
    free(app);
}

// ============================================================================
// Point d'entrée de l'application
// ============================================================================
int32_t sli_writer_app(void* p) {
    UNUSED(p);
    SliWriterApp* app = sli_writer_app_alloc();
    view_dispatcher_run(app->view_dispatcher);
    sli_writer_app_free(app);
    return 0;
}
