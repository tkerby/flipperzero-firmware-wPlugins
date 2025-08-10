#include "sli_writer.h"

#define TAG "SLI_Writer"

/*
Résumé :
- Pas de sous-thread pour nfc_start : tout est fait dans le worker.
- Logs ultra verbeux à chaque étape (alloc, start, poller, inventory, magic, etc.).
- Sélection directe du dossier /ext/nfc pour le choix du fichier.
- Menu racine : "Write NFC File" et "Test Magic INIT".
- Utilise l’API 0.82 (Unleashed) : nfc_alloc() / nfc_start(cb) / nfc_poller_alloc(NfcProtocolIso15693_3)
- Envoi des trames via iso15693_3_poller_send_frame() avec BitBuffer.

Si nfc_start() bloque toujours, les logs s’arrêteront juste après "nfc_start (worker)..." : 
=> c’est précisément ce qu’on veut montrer au Discord.
*/

// ------------------------------------------------------------
// ISO15693 octets standard (définis ici pour ne pas dépendre
// d'éventuels alias d'en-têtes qui diffèrent entre firmwares)
// ------------------------------------------------------------
#define ISO15693_FLAG_RATE_HIGH   0x02
#define ISO15693_FLAG_INVENTORY   0x04
#define ISO15693_FLAG_ADDRESSED   0x20

#define ISO15693_CMD_INVENTORY            0x01
#define ISO15693_CMD_WRITE_SINGLE_BLOCK   0x21
#define ISO15693_CMD_CUSTOM               0xA0   // "Custom Command"

// ------------------------------------------------------------
// Magic SLI subcommands (style Proxmark3)
// ------------------------------------------------------------
#define SLI_MAGIC_CMD_INIT         0x36
#define SLI_MAGIC_CMD_SET_UID_HIGH 0x37
#define SLI_MAGIC_CMD_SET_UID_LOW  0x38
#define SLI_MAGIC_CMD_FINALIZE     0x39
#define SLI_MAGIC_CMD_SET_PASSWORD 0x3A

// ==============================
// BitBuffer helpers
// ==============================
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

// ==============================
// ISO15693: Inventory -> UID (via Iso15693_3Poller*)
// ==============================
static bool iso15693_inventory_get_uid(Iso15693_3Poller* iso, uint8_t out_uid[8]) {
    if(!iso || !out_uid) return false;

    uint8_t tx[2] = {
        (uint8_t)(ISO15693_FLAG_INVENTORY | ISO15693_FLAG_RATE_HIGH),
        ISO15693_CMD_INVENTORY,
    };
    BitBuffer* txbb = bb_from_bytes_const(tx, sizeof(tx));
    if(!txbb) return false;

    BitBuffer* rxbb = bb_alloc_for_rx(32);
    if(!rxbb) { bit_buffer_free(txbb); return false; }

    FURI_LOG_I(TAG, "Inventory send (timeout=30ms)");
    Iso15693_3Error err = iso15693_3_poller_send_frame(iso, txbb, rxbb, 30);
    size_t rx_bits = bit_buffer_get_size(rxbb);
    FURI_LOG_I(TAG, "Inventory ret=%d rx_bits=%u", err, (unsigned)rx_bits);

    bool ok = false;
    if(err == Iso15693_3ErrorNone) {
        size_t rx_bytes = rx_bits / 8;
        if(rx_bytes >= 9) {
            uint8_t buf[32] = {0};
            size_t copy = (rx_bytes > sizeof(buf)) ? sizeof(buf) : rx_bytes;
            bit_buffer_write_bytes(rxbb, buf, copy);
            // buf[0] = DSFID ; buf[1..8] = UID (LSB->MSB dans la réponse ISO)
            for(size_t i = 0; i < 8; i++) out_uid[i] = buf[1 + i];
            ok = true;
            FURI_LOG_I(TAG, "Inventory UID: %02X %02X %02X %02X %02X %02X %02X %02X",
                out_uid[0], out_uid[1], out_uid[2], out_uid[3],
                out_uid[4], out_uid[5], out_uid[6], out_uid[7]);
        } else {
            FURI_LOG_W(TAG, "Inventory response too short: %u bytes", (unsigned)rx_bytes);
        }
    }

    bit_buffer_free(txbb);
    bit_buffer_free(rxbb);
    return ok;
}

// ==============================
// ISO15693 addressed raw sender (via Iso15693_3Poller*)
// ==============================
static bool iso15693_send_addressed_raw(
    Iso15693_3Poller* iso,
    uint8_t cmd,
    const uint8_t uid[8],
    const uint8_t* payload,
    size_t payload_len,
    uint32_t fwt_ms /* 0 = défaut */
) {
    if(!iso || !uid) return false;

    uint8_t frame[64];
    size_t len = 0;

    frame[len++] = (uint8_t)(ISO15693_FLAG_ADDRESSED | ISO15693_FLAG_RATE_HIGH);
    frame[len++] = cmd;
    for(size_t i = 0; i < 8; i++) frame[len++] = uid[i];
    if(payload && payload_len) {
        if(len + payload_len > sizeof(frame)) {
            FURI_LOG_E(TAG, "TX frame overflow (%u+%u)", (unsigned)len, (unsigned)payload_len);
            return false;
        }
        memcpy(&frame[len], payload, payload_len);
        len += payload_len;
    }

    BitBuffer* txbb = bb_from_bytes_const(frame, len);
    if(!txbb) return false;
    BitBuffer* rxbb = bb_alloc_for_rx(24);
    if(!rxbb) { bit_buffer_free(txbb); return false; }

    uint32_t to = fwt_ms ? fwt_ms : 50;
    FURI_LOG_D(TAG, "Send cmd=0x%02X len=%u to=%ums", cmd, (unsigned)len, (unsigned)to);
    Iso15693_3Error err = iso15693_3_poller_send_frame(iso, txbb, rxbb, to);
    FURI_LOG_D(TAG, "Send ret=%d rx_bits=%u", err, (unsigned)bit_buffer_get_size(rxbb));

    bit_buffer_free(txbb);
    bit_buffer_free(rxbb);
    return (err == Iso15693_3ErrorNone);
}

// ==============================
// Parser de fichier .NFC (format Flipper .nfc)
// ==============================
bool sli_writer_parse_nfc_file(SliWriterApp* app, const char* file_path) {
    if(!app || !file_path || !app->storage) return false;

    FURI_LOG_I(TAG, "Parse file: %s", file_path);

    File* file = storage_file_alloc(app->storage);
    if(!file) return false;

    bool ok = false;

    if(!storage_file_open(file, file_path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(file);
        return false;
    }

    size_t file_size = storage_file_size(file);
    if(file_size == 0) { storage_file_close(file); storage_file_free(file); return false; }
    if(file_size > 8192) file_size = 8192;

    char* buf = malloc(file_size + 1);
    if(!buf) { storage_file_close(file); storage_file_free(file); return false; }

    size_t rd = storage_file_read(file, buf, file_size);
    buf[rd] = '\0';
    storage_file_close(file);
    storage_file_free(file);

    memset(&app->nfc_data, 0, sizeof(app->nfc_data));
    app->nfc_data.block_size  = SLI_MAGIC_BLOCK_SIZE; // défaut 4
    app->nfc_data.block_count = 8;

    // UID
    char* p = strstr(buf, "UID: ");
    if(!p) { free(buf); return false; }
    p += 5;
    for(int i = 0; i < 8; i++) {
        unsigned v; if(sscanf(p + i*3, "%02X", &v) != 1) { free(buf); return false; }
        app->nfc_data.uid[i] = (uint8_t)v;
    }
    FURI_LOG_I(TAG, "File UID: %02X %02X %02X %02X %02X %02X %02X %02X",
        app->nfc_data.uid[0], app->nfc_data.uid[1], app->nfc_data.uid[2], app->nfc_data.uid[3],
        app->nfc_data.uid[4], app->nfc_data.uid[5], app->nfc_data.uid[6], app->nfc_data.uid[7]);

    // Block Count
    p = strstr(buf, "Block Count: ");
    if(p) {
        p += 13; int bc = 0;
        if(sscanf(p, "%d", &bc) == 1 && bc > 0 && bc <= SLI_MAGIC_MAX_BLOCKS) app->nfc_data.block_count = (uint8_t)bc;
    }

    // Block Size (hex)
    p = strstr(buf, "Block Size: ");
    if(p) {
        p += 12; unsigned bs = 0;
        if(sscanf(p, "%02X", &bs) == 1 && bs >= 1 && bs <= 0x20) app->nfc_data.block_size = (uint8_t)bs;
    }

    // Data Content (ligne d’octets hex séparés par espaces)
    size_t di = 0;
    p = strstr(buf, "Data Content: ");
    if(p) {
        p += 14;
        char* eol = strchr(p, '\n'); if(eol) *eol = '\0';
        while(*p && di < sizeof(app->nfc_data.data)) {
            unsigned v; int n = 0;
            if(sscanf(p, "%02X%n", &v, &n) != 1) break;
            app->nfc_data.data[di++] = (uint8_t)v;
            p += n; while(*p == ' ') p++;
        }
        if(eol) *eol = '\n';
    }

    // Cohérence taille
    size_t needed = (size_t)app->nfc_data.block_count * app->nfc_data.block_size;
    if(needed > sizeof(app->nfc_data.data)) needed = sizeof(app->nfc_data.data);
    if(needed && di < needed) {
        FURI_LOG_E(TAG, "Data too short: have=%u need=%u", (unsigned)di, (unsigned)needed);
        free(buf);
        return false;
    }

    // Password Privacy
    p = strstr(buf, "Password Privacy: ");
    if(p) {
        p += 18;
        for(int i = 0; i < 4; i++) { unsigned v; if(sscanf(p + i*3, "%02X", &v) == 1) app->nfc_data.password_privacy[i] = (uint8_t)v; }
        FURI_LOG_I(TAG, "Privacy pwd: %02X %02X %02X %02X",
            app->nfc_data.password_privacy[0], app->nfc_data.password_privacy[1],
            app->nfc_data.password_privacy[2], app->nfc_data.password_privacy[3]);
    }

    ok = true;
    free(buf);
    return ok;
}

// ==============================
// Magic commands helpers
// ==============================
static bool magic_send(Iso15693_3Poller* iso, const uint8_t uid_addr[8],
                       uint8_t subcmd, const uint8_t* data, size_t len) {
    if(!iso || !uid_addr) return false;
    uint8_t payload[16]; size_t p = 0;
    payload[p++] = subcmd;
    if(data && len) {
        if(p + len > sizeof(payload)) return false;
        memcpy(&payload[p], data, len);
        p += len;
    }
    FURI_LOG_I(TAG, "Magic subcmd=0x%02X len=%u", subcmd, (unsigned)len);
    return iso15693_send_addressed_raw(iso, ISO15693_CMD_CUSTOM, uid_addr, payload, p, 50);
}

static bool write_uid_magic(Iso15693_3Poller* iso, const uint8_t uid_new[8], const uint8_t uid_addr[8]) {
    uint8_t uid_high[4] = { uid_new[7], uid_new[6], uid_new[5], uid_new[4] };
    uint8_t uid_low [4] = { uid_new[3], uid_new[2], uid_new[1], uid_new[0] };

    FURI_LOG_I(TAG, "Write UID (INIT)");
    if(!magic_send(iso, uid_addr, SLI_MAGIC_CMD_INIT, NULL, 0)) return false;
    furi_delay_ms(30);

    FURI_LOG_I(TAG, "Write UID HIGH: %02X %02X %02X %02X", uid_high[0], uid_high[1], uid_high[2], uid_high[3]);
    if(!magic_send(iso, uid_addr, SLI_MAGIC_CMD_SET_UID_HIGH, uid_high, sizeof(uid_high))) return false;
    furi_delay_ms(30);

    FURI_LOG_I(TAG, "Write UID LOW : %02X %02X %02X %02X", uid_low[0], uid_low[1], uid_low[2], uid_low[3]);
    if(!magic_send(iso, uid_addr, SLI_MAGIC_CMD_SET_UID_LOW, uid_low, sizeof(uid_low))) return false;
    furi_delay_ms(30);

    FURI_LOG_I(TAG, "Finalize UID");
    if(!magic_send(iso, uid_addr, SLI_MAGIC_CMD_FINALIZE, uid_low, sizeof(uid_low))) return false;

    return true;
}

static bool write_passwords_magic(Iso15693_3Poller* iso, const uint8_t uid_addr[8], const uint8_t pwd[4]) {
    bool any = false; for(int i=0;i<4;i++) if(pwd[i]) { any = true; break; }
    if(!any) { FURI_LOG_I(TAG, "No privacy password to write"); return true; }
    FURI_LOG_I(TAG, "Write privacy password");
    return magic_send(iso, uid_addr, SLI_MAGIC_CMD_SET_PASSWORD, pwd, 4);
}

// ==============================
// Write data blocks (addressed Write Single Block)
// ==============================
static bool write_blocks(Iso15693_3Poller* iso, const uint8_t uid_addr[8],
                         const uint8_t* data, uint8_t block_count, uint8_t block_size) {
    size_t blocks = block_count;
    if(blocks > SLI_MAGIC_MAX_BLOCKS) blocks = SLI_MAGIC_MAX_BLOCKS;

    uint8_t eff_size = (block_size == 0) ? 4 : block_size;
    if(eff_size != 4) eff_size = 4;

    FURI_LOG_I(TAG, "Write %u blocks (size=%u)", (unsigned)blocks, (unsigned)eff_size);

    for(size_t b = 0; b < blocks; b++) {
        uint8_t payload[1 + 4];
        payload[0] = (uint8_t)b;

        size_t base = b * eff_size;
        for(size_t i=0;i<4;i++) {
            size_t idx = base + i;
            payload[1+i] = data ? data[idx] : 0x00;
        }

        FURI_LOG_D(TAG, "Write block %u: %02X %02X %02X %02X",
            (unsigned)b, payload[1], payload[2], payload[3], payload[4]);

        if(!iso15693_send_addressed_raw(iso, ISO15693_CMD_WRITE_SINGLE_BLOCK, uid_addr, payload, sizeof(payload), 60)) {
            FURI_LOG_E(TAG, "Write block %u failed", (unsigned)b);
            return false;
        }
        furi_delay_ms(12);
    }
    return true;
}

// ==============================
// NFC event callback (0.82: event est une struct, switch sur event.type)
// ==============================
static NfcCommand nfc_event_cb(NfcEvent event, void* ctx) {
    UNUSED(ctx);
    switch(event.type) {
        case NfcEventTypePollerReady:
            FURI_LOG_D(TAG, "NFC cb: PollerReady");
            break;
        default:
            FURI_LOG_D(TAG, "NFC cb: type=%u", (unsigned)event.type);
            break;
    }
    return NfcCommandContinue;
}

// ==============================
// Detect + Activate + Inventory (worker)
// ==============================
static bool detect_activate_inventory(SliWriterApp* app, Iso15693_3Poller** out_iso) {
    if(!app || !app->poller) return false;

    // detect avec retries
    bool present = false;
    for(int i=0;i<10;i++) {
        if(nfc_poller_detect(app->poller)) { present = true; break; }
        furi_delay_ms(60);
    }
    if(!present) {
        furi_string_set(app->error_message, "No tag detected");
        return false;
    }

    Iso15693_3Poller* iso = (Iso15693_3Poller*)nfc_poller_get_data(app->poller);
    if(!iso) { furi_string_set(app->error_message, "ISO poller NULL"); return false; }

    Iso15693_3Data iso_data = {0};
    Iso15693_3Error aerr = iso15693_3_poller_activate(iso, &iso_data);
    FURI_LOG_I(TAG, "activate ret=%d", aerr);
    if(aerr != Iso15693_3ErrorNone) {
        furi_string_set(app->error_message, "ISO activate failed");
        return false;
    }

    if(!iso15693_inventory_get_uid(iso, app->detected_uid)) {
        furi_string_set(app->error_message, "Inventory/UID a échoué");
        return false;
    }
    app->have_uid = true;
    if(out_iso) *out_iso = iso;
    return true;
}

// ==============================
// Écriture complète (UID + pwd + blocs) - worker
// ==============================
bool slix_writer_perform_write(SliWriterApp* app) {
    if(!app) return false;
    FURI_LOG_I(TAG, "=== WRITE START ===");

    Iso15693_3Poller* iso = NULL;
    if(!detect_activate_inventory(app, &iso)) return false;

    // 1) UID magic adressé à l'UID détecté
    if(!write_uid_magic(iso, app->nfc_data.uid, app->detected_uid)) {
        furi_string_set(app->error_message, "Échec écriture UID");
        return false;
    }
    furi_delay_ms(150);

    // 1b) Inventory de confirmation
    uint8_t confirm_uid[8] = {0};
    if(!iso15693_inventory_get_uid(iso, confirm_uid)) {
        furi_string_set(app->error_message, "Inventory après UID échoué");
        return false;
    }

    // 2) MDP Privacy adressé au NOUVEL UID
    if(!write_passwords_magic(iso, confirm_uid, app->nfc_data.password_privacy)) {
        furi_string_set(app->error_message, "Échec écriture mot de passe");
        return false;
    }
    furi_delay_ms(60);

    // 3) Blocs adressés au NOUVEL UID
    if(!write_blocks(iso, confirm_uid, app->nfc_data.data, app->nfc_data.block_count, app->nfc_data.block_size)) {
        furi_string_set(app->error_message, "Échec écriture blocs");
        return false;
    }

    FURI_LOG_I(TAG, "=== WRITE OK ===");
    return true;
}

// ==============================
// Mode test: envoyer uniquement INIT magic - worker
// ==============================
bool slix_writer_perform_test(SliWriterApp* app) {
    if(!app) return false;
    FURI_LOG_I(TAG, "=== TEST MAGIC INIT START ===");

    Iso15693_3Poller* iso = NULL;
    if(!detect_activate_inventory(app, &iso)) return false;

    if(!magic_send(iso, app->detected_uid, SLI_MAGIC_CMD_INIT, NULL, 0)) {
        furi_string_set(app->error_message, "INIT magic échoué");
        return false;
    }
    FURI_LOG_I(TAG, "=== TEST MAGIC INIT OK ===");
    return true;
}

// ==============================
// Worker thread (tout le NFC ici) — nfc_alloc puis nfc_start
// ==============================
int32_t slix_writer_work_thread(void* context) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return 0;

    FURI_LOG_I(TAG, "Worker start");

    // === NFC alloc ===
    if(!app->nfc) {
        FURI_LOG_I(TAG, "nfc_alloc (worker)...");
        app->nfc = nfc_alloc();
        if(!app->nfc) {
            furi_string_set(app->error_message, "nfc_alloc failed");
            view_dispatcher_send_custom_event(app->view_dispatcher, SliWriterCustomEventWriteError);
            return 0;
        }
    }

    // Petite sieste avant nfc_start (évite parfois un interlock sur UI)
    furi_delay_ms(20);

    // === NFC start === (si ça bloque, on le verra au log Discord)
    FURI_LOG_I(TAG, "nfc_start (worker)...");
    nfc_start(app->nfc, nfc_event_cb, app);
    app->nfc_started = true;
    FURI_LOG_I(TAG, "nfc_start (worker) returned");

    // === Poller ISO15693 ===
    if(app->poller) { nfc_poller_free(app->poller); app->poller = NULL; }
    FURI_LOG_I(TAG, "nfc_poller_alloc ISO15693 (worker)...");
    app->poller = nfc_poller_alloc(app->nfc, NfcProtocolIso15693_3);
    if(!app->poller) {
        furi_string_set(app->error_message, "poller alloc failed");
        goto WORKER_FAIL;
    }
    FURI_LOG_I(TAG, "nfc_poller_alloc OK");

    // === Mode test vs mode écriture ===
    if(app->test_mode) {
        if(slix_writer_perform_test(app)) {
            FURI_LOG_I(TAG, "Test success");
            goto WORKER_OK;
        } else {
            FURI_LOG_E(TAG, "Test error: %s", furi_string_get_cstr(app->error_message));
            goto WORKER_FAIL;
        }
    } else {
        if(!sli_writer_parse_nfc_file(app, furi_string_get_cstr(app->file_path))) {
            FURI_LOG_E(TAG, "Parse failed");
            furi_string_set(app->error_message, "Erreur lecture fichier");
            goto WORKER_FAIL;
        }
        if(slix_writer_perform_write(app)) {
            FURI_LOG_I(TAG, "Write success");
            goto WORKER_OK;
        } else {
            FURI_LOG_E(TAG, "Write error: %s", furi_string_get_cstr(app->error_message));
            goto WORKER_FAIL;
        }
    }

WORKER_OK:
    if(app->poller) { nfc_poller_free(app->poller); app->poller = NULL; }
    if(app->nfc_started) { FURI_LOG_I(TAG, "nfc_stop (worker)"); nfc_stop(app->nfc); app->nfc_started = false; }
    if(app->nfc) { FURI_LOG_I(TAG, "nfc_free (worker)"); nfc_free(app->nfc); app->nfc = NULL; }
    view_dispatcher_send_custom_event(app->view_dispatcher, SliWriterCustomEventWriteSuccess);
    return 0;

WORKER_FAIL:
    if(app->poller) { nfc_poller_free(app->poller); app->poller = NULL; }
    if(app->nfc_started) { FURI_LOG_I(TAG, "nfc_stop (worker)"); nfc_stop(app->nfc); app->nfc_started = false; }
    if(app->nfc) { FURI_LOG_I(TAG, "nfc_free (worker)"); nfc_free(app->nfc); app->nfc = NULL; }
    view_dispatcher_send_custom_event(app->view_dispatcher, SliWriterCustomEventWriteError);
    return 0;
}

// ==============================
// Scenes
// ==============================
void sli_writer_scene_start_on_enter(void* context) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return;

    FURI_LOG_I(TAG, "Scene Start enter");

    submenu_reset(app->submenu);
    submenu_add_item(app->submenu, "Write NFC File",   SliWriterSubmenuIndexWrite,     sli_writer_submenu_callback, app);
    submenu_add_item(app->submenu, "Test Magic INIT",  SliWriterSubmenuIndexTestMagic, sli_writer_submenu_callback, app);
    submenu_add_item(app->submenu, "About",            SliWriterSubmenuIndexAbout,     sli_writer_submenu_callback, app);
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewSubmenu);
}

bool sli_writer_scene_start_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return false;
    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SliWriterSubmenuIndexWrite) {
            FURI_LOG_I(TAG, "Menu -> FileSelect");
            app->test_mode = false;
            scene_manager_next_scene(app->scene_manager, SliWriterSceneFileSelect);
            return true;
        }
        if(event.event == SliWriterSubmenuIndexTestMagic) {
            FURI_LOG_I(TAG, "Menu -> TestMagic");
            app->test_mode = true;
            scene_manager_next_scene(app->scene_manager, SliWriterSceneWrite);
            return true;
        }
        if(event.event == SliWriterSubmenuIndexAbout) {
            FURI_LOG_I(TAG, "Menu -> About");
            dialog_ex_reset(app->dialog_ex);
            dialog_ex_set_header(app->dialog_ex, "SLI Writer", 64, 0, AlignCenter, AlignTop);
            dialog_ex_set_text(app->dialog_ex, "Write SLIX .nfc files\nto SLI Magic cards\n\nv2.7 (single-thread NFC)\n⚠️ Use with caution!", 64, 32, AlignCenter, AlignCenter);
            dialog_ex_set_left_button_text(app->dialog_ex, "Back");
            view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
            return true;
        }
    }
    return false;
}

void sli_writer_scene_start_on_exit(void* context) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return;
    FURI_LOG_I(TAG, "Scene Start exit");
    submenu_reset(app->submenu);
}

void sli_writer_scene_file_select_on_enter(void* context) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return;

    FURI_LOG_I(TAG, "Scene FileSelect enter");

    // Ouvrir directement sur /ext/nfc
    if(!app->file_path) app->file_path = furi_string_alloc();
    furi_string_set(app->file_path, "/ext/nfc");

    DialogsFileBrowserOptions opt;
    dialog_file_browser_set_basic_options(&opt, SLI_WRITER_FILE_EXTENSION, NULL);

    if(dialog_file_browser_show(app->dialogs, app->file_path, app->file_path, &opt)) {
        FURI_LOG_I(TAG, "File chosen: %s", furi_string_get_cstr(app->file_path));
        scene_manager_next_scene(app->scene_manager, SliWriterSceneWrite);
    } else {
        FURI_LOG_I(TAG, "File selection canceled");
        scene_manager_previous_scene(app->scene_manager);
    }
}

bool sli_writer_scene_file_select_on_event(void* context, SceneManagerEvent event) {
    UNUSED(context); UNUSED(event); return false;
}

void sli_writer_scene_file_select_on_exit(void* context) {
    FURI_LOG_I(TAG, "Scene FileSelect exit");
    UNUSED(context);
}

// --------- WRITE SCENE ---------
void sli_writer_scene_write_on_enter(void* context) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return;

    FURI_LOG_I(TAG, "Scene Write enter -> loading (worker runs NFC)");
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewLoading);

    if(app->worker_thread) {
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }
    app->worker_thread = furi_thread_alloc_ex("sli_writer_worker", 8192, slix_writer_work_thread, app);
    if(!app->worker_thread) {
        furi_string_set(app->error_message, "Thread alloc failed");
        scene_manager_next_scene(app->scene_manager, SliWriterSceneError);
        return;
    }
    furi_thread_start(app->worker_thread);
}

bool sli_writer_scene_write_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return false;

    if(event.type == SceneManagerEventTypeCustom) {
        if(event.event == SliWriterCustomEventWriteSuccess) {
            FURI_LOG_I(TAG, "Scene Write -> Success");
            scene_manager_next_scene(app->scene_manager, SliWriterSceneSuccess);
            return true;
        }
        if(event.event == SliWriterCustomEventParseError || event.event == SliWriterCustomEventWriteError) {
            FURI_LOG_I(TAG, "Scene Write -> Error");
            scene_manager_next_scene(app->scene_manager, SliWriterSceneError);
            return true;
        }
    }
    return false;
}

void sli_writer_scene_write_on_exit(void* context) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return;
    FURI_LOG_I(TAG, "Scene Write exit (join worker)");

    if(app->worker_thread) {
        furi_thread_join(app->worker_thread);
        furi_thread_free(app->worker_thread);
        app->worker_thread = NULL;
    }
    // Le worker a déjà nettoyé NFC/poller.
}

void sli_writer_scene_success_on_enter(void* context) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return;
    FURI_LOG_I(TAG, "Scene Success enter");
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Success!", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog_ex, "Card written successfully\n(or test OK)", 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "Back");
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
}

bool sli_writer_scene_success_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return false;
    if(event.type == SceneManagerEventTypeBack ||
       (event.type == SceneManagerEventTypeCustom && event.event == DialogExResultLeft)) {
        FURI_LOG_I(TAG, "Scene Success -> Start");
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SliWriterSceneStart);
        return true;
    }
    return false;
}

void sli_writer_scene_success_on_exit(void* context) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return;
    FURI_LOG_I(TAG, "Scene Success exit");
    dialog_ex_reset(app->dialog_ex);
}

void sli_writer_scene_error_on_enter(void* context) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return;
    FURI_LOG_I(TAG, "Scene Error enter: %s", furi_string_get_cstr(app->error_message));
    dialog_ex_reset(app->dialog_ex);
    dialog_ex_set_header(app->dialog_ex, "Error", 64, 0, AlignCenter, AlignTop);
    dialog_ex_set_text(app->dialog_ex, furi_string_get_cstr(app->error_message), 64, 32, AlignCenter, AlignCenter);
    dialog_ex_set_left_button_text(app->dialog_ex, "Back");
    view_dispatcher_switch_to_view(app->view_dispatcher, SliWriterViewDialogEx);
}

bool sli_writer_scene_error_on_event(void* context, SceneManagerEvent event) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return false;
    if(event.type == SceneManagerEventTypeBack ||
       (event.type == SceneManagerEventTypeCustom && event.event == DialogExResultLeft)) {
        FURI_LOG_I(TAG, "Scene Error -> Start");
        scene_manager_search_and_switch_to_previous_scene(app->scene_manager, SliWriterSceneStart);
        return true;
    }
    return false;
}

void sli_writer_scene_error_on_exit(void* context) {
    SliWriterApp* app = (SliWriterApp*)context;
    if(!app) return;
    FURI_LOG_I(TAG, "Scene Error exit");
    dialog_ex_reset(app->dialog_ex);
}

// ==============================
// Callbacks
// ==============================
void sli_writer_submenu_callback(void* context, uint32_t index) {
    SliWriterApp* app = (SliWriterApp*)context; if(!app) return;
    FURI_LOG_I(TAG, "Submenu index=%lu", index);
    view_dispatcher_send_custom_event(app->view_dispatcher, index);
}

void sli_writer_dialog_ex_callback(DialogExResult result, void* context) {
    SliWriterApp* app = (SliWriterApp*)context; if(!app) return;
    FURI_LOG_I(TAG, "Dialog result=%d", result);
    view_dispatcher_send_custom_event(app->view_dispatcher, result);
}

bool sli_writer_back_event_callback(void* context) {
    SliWriterApp* app = (SliWriterApp*)context; if(!app) return false;
    FURI_LOG_I(TAG, "Back event");
    return scene_manager_handle_back_event(app->scene_manager);
}

bool sli_writer_custom_event_callback(void* context, uint32_t event) {
    SliWriterApp* app = (SliWriterApp*)context; if(!app) return false;
    FURI_LOG_I(TAG, "Custom event=%lu", event);
    return scene_manager_handle_custom_event(app->scene_manager, event);
}

// ==============================
// Scene handlers arrays & lifecycle
// ==============================
static void (*const s_on_enter[])(void*) = {
    sli_writer_scene_start_on_enter,
    sli_writer_scene_file_select_on_enter,
    sli_writer_scene_write_on_enter,
    sli_writer_scene_success_on_enter,
    sli_writer_scene_error_on_enter,
};

static bool (*const s_on_event[])(void*, SceneManagerEvent) = {
    sli_writer_scene_start_on_event,
    sli_writer_scene_file_select_on_event,
    sli_writer_scene_write_on_event,
    sli_writer_scene_success_on_event,
    sli_writer_scene_error_on_event,
};

static void (*const s_on_exit[])(void*) = {
    sli_writer_scene_start_on_exit,
    sli_writer_scene_file_select_on_exit,
    sli_writer_scene_write_on_exit,
    sli_writer_scene_success_on_exit,
    sli_writer_scene_error_on_exit,
};

SliWriterApp* sli_writer_app_alloc(void) {
    FURI_LOG_I(TAG, "App alloc");
    SliWriterApp* app = malloc(sizeof(SliWriterApp));
    if(!app) return NULL;
    memset(app, 0, sizeof(*app));

    app->gui = furi_record_open(RECORD_GUI);
    app->view_dispatcher = view_dispatcher_alloc();

    static const SceneManagerHandlers handlers = {
        .on_enter_handlers = s_on_enter,
        .on_event_handlers = s_on_event,
        .on_exit_handlers  = s_on_exit,
        .scene_num = SliWriterSceneNum,
    };
    app->scene_manager = scene_manager_alloc(&handlers, app);

    app->submenu = submenu_alloc();
    app->dialog_ex = dialog_ex_alloc();
    app->loading = loading_alloc();

    app->storage = furi_record_open(RECORD_STORAGE);
    app->dialogs = furi_record_open(RECORD_DIALOGS);

    app->file_path = furi_string_alloc();
    app->error_message = furi_string_alloc();

    view_dispatcher_set_event_callback_context(app->view_dispatcher, app);
    view_dispatcher_set_custom_event_callback(app->view_dispatcher, sli_writer_custom_event_callback);
    view_dispatcher_set_navigation_event_callback(app->view_dispatcher, sli_writer_back_event_callback);

    view_dispatcher_add_view(app->view_dispatcher, SliWriterViewSubmenu, submenu_get_view(app->submenu));
    view_dispatcher_add_view(app->view_dispatcher, SliWriterViewDialogEx, dialog_ex_get_view(app->dialog_ex));
    view_dispatcher_add_view(app->view_dispatcher, SliWriterViewLoading, loading_get_view(app->loading));

    dialog_ex_set_context(app->dialog_ex, app);
    dialog_ex_set_result_callback(app->dialog_ex, sli_writer_dialog_ex_callback);

    view_dispatcher_attach_to_gui(app->view_dispatcher, app->gui, ViewDispatcherTypeFullscreen);

    // NFC : alloué/démarré UNIQUEMENT dans le worker
    app->nfc = NULL;
    app->nfc_started = false;

    scene_manager_next_scene(app->scene_manager, SliWriterSceneStart);
    return app;
}

void sli_writer_app_free(SliWriterApp* app) {
    FURI_LOG_I(TAG, "App free");
    if(!app) return;

    if(app->worker_thread) { furi_thread_join(app->worker_thread); furi_thread_free(app->worker_thread); app->worker_thread = NULL; }

    // Sécurité : si le worker a été interrompu, on nettoie proprement
    if(app->poller) { nfc_poller_free(app->poller); app->poller = NULL; }
    if(app->nfc_started && app->nfc) { nfc_stop(app->nfc); app->nfc_started = false; }
    if(app->nfc) { nfc_free(app->nfc); app->nfc = NULL; }

    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewSubmenu);
    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewDialogEx);
    view_dispatcher_remove_view(app->view_dispatcher, SliWriterViewLoading);

    if(app->submenu) submenu_free(app->submenu);
    if(app->dialog_ex) dialog_ex_free(app->dialog_ex);
    if(app->loading) loading_free(app->loading);

    if(app->view_dispatcher) view_dispatcher_free(app->view_dispatcher);
    if(app->scene_manager) scene_manager_free(app->scene_manager);

    if(app->storage) furi_record_close(RECORD_STORAGE);
    if(app->dialogs) furi_record_close(RECORD_DIALOGS);
    if(app->gui)     furi_record_close(RECORD_GUI);

    if(app->file_path)     furi_string_free(app->file_path);
    if(app->error_message) furi_string_free(app->error_message);

    free(app);
}

// ==============================
// Entry point
// ==============================
int32_t sli_writer_app(void* p) {
    UNUSED(p);
    FURI_LOG_I(TAG, "Entry");
    SliWriterApp* app = sli_writer_app_alloc();
    if(!app) return -1;
    view_dispatcher_run(app->view_dispatcher);
    sli_writer_app_free(app);
    FURI_LOG_I(TAG, "Exit");
    return 0;
}
