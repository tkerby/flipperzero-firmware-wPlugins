#include "key_scanner.h"
#include <lib/nfc/nfc_poller.h>
#include <furi_hal_rfid.h>
#include <lib/nfc/nfc_device.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <stdio.h>
#include <string.h>

struct KeyScanner {
    FuriThread* thread;
    FuriMutex* mutex;
    bool is_scanning;
    bool is_thread_running;
    bool key_detected;
    DetectedKey detected_key;

    // iButton
    iButtonProtocols* ibutton_protocols;
    iButtonKey* ibutton_key;

    // LFRFID
    ProtocolDict* rfid_dict;
    LFRFIDWorker* rfid_worker;
    bool rfid_raw_detected;
    ProtocolId rfid_protocol;

    // NFC
    Nfc* nfc;
};

static void rfid_read_callback(LFRFIDWorkerReadResult result, ProtocolId protocol, void* context) {
    KeyScanner* scanner = (KeyScanner*)context;
    if(result == LFRFIDWorkerReadDone) {
        scanner->rfid_raw_detected = true;
        scanner->rfid_protocol = protocol;
    }
}

static int32_t key_scanner_worker(void* context) {
    KeyScanner* scanner = (KeyScanner*)context;

    while(scanner->is_thread_running) {
        if(!scanner->is_scanning) {
            furi_delay_ms(50);
            continue;
        }

        // ==========================================
        // PHASE 1: 125 kHz LFRFID
        // ==========================================
        scanner->rfid_raw_detected = false;
        if(scanner->rfid_worker) {
            lfrfid_worker_read_start(scanner->rfid_worker, LFRFIDWorkerReadTypeAuto, rfid_read_callback, scanner);
            
            for(int i = 0; i < 5 && scanner->is_scanning && !scanner->rfid_raw_detected; i++) {
                furi_delay_ms(80);
            }

            lfrfid_worker_stop(scanner->rfid_worker);
            furi_delay_ms(120); // Guarantee worker mode loop exits to idle
            furi_hal_rfid_pins_reset(); // Free shared GPIOs
        }

        if(scanner->rfid_raw_detected) {
            furi_mutex_acquire(scanner->mutex, FuriWaitForever);
            memset(&scanner->detected_key, 0, sizeof(DetectedKey));
            scanner->detected_key.category = KeyCategoryLFRFID;
            scanner->detected_key.valid = true;

            const char* name = protocol_dict_get_name(scanner->rfid_dict, scanner->rfid_protocol);
            strncpy(scanner->detected_key.protocol_name, name ? name : "EM4100", sizeof(scanner->detected_key.protocol_name) - 1);

            size_t size = protocol_dict_get_data_size(scanner->rfid_dict, scanner->rfid_protocol);
            if(size > sizeof(scanner->detected_key.data)) size = sizeof(scanner->detected_key.data);
            scanner->detected_key.data_len = size;
            protocol_dict_get_data(scanner->rfid_dict, scanner->rfid_protocol, scanner->detected_key.data, size);

            FuriString* uid_str = furi_string_alloc();
            protocol_dict_render_uid(scanner->rfid_dict, uid_str, scanner->rfid_protocol);
            strncpy(scanner->detected_key.data_hex, furi_string_get_cstr(uid_str), sizeof(scanner->detected_key.data_hex) - 1);
            furi_string_free(uid_str);

            scanner->key_detected = true;
            scanner->is_scanning = false;
            furi_mutex_release(scanner->mutex);
            continue;
        }

        if(!scanner->is_scanning) continue;

        // ==========================================
        // PHASE 2: 13.56 MHz NFC
        // ==========================================
        if(scanner->nfc) {
            for(int i = 0; i < 4 && scanner->is_scanning; i++) {
                NfcPoller* poller = nfc_poller_alloc(scanner->nfc, NfcProtocolIso14443_3a);
                if(poller) {
                    if(nfc_poller_detect(poller)) {
                        const Iso14443_3aData* nfc_data = (const Iso14443_3aData*)nfc_poller_get_data(poller);
                        if(nfc_data && nfc_data->uid_len > 0) {
                            furi_mutex_acquire(scanner->mutex, FuriWaitForever);
                            memset(&scanner->detected_key, 0, sizeof(DetectedKey));
                            scanner->detected_key.category = KeyCategoryNFC;
                            scanner->detected_key.valid = true;

                            // Determine type (Mifare Classic, Ultralight, NTAG)
                            const char* card_name = "Mifare Classic";
                            if(nfc_data->sak == 0x00) {
                                card_name = "Mifare Ultralight";
                            } else if(nfc_data->sak == 0x20) {
                                card_name = "Mifare Plus";
                            }
                            strncpy(scanner->detected_key.protocol_name, card_name, sizeof(scanner->detected_key.protocol_name) - 1);

                            size_t uid_len = nfc_data->uid_len;
                            if(uid_len > sizeof(scanner->detected_key.data)) uid_len = sizeof(scanner->detected_key.data);
                            scanner->detected_key.data_len = uid_len;
                            memcpy(scanner->detected_key.data, nfc_data->uid, uid_len);

                            // Format HEX UID
                            char hex_buf[64] = "";
                            int offset = 0;
                            for(size_t u = 0; u < uid_len && offset < (int)sizeof(hex_buf) - 4; u++) {
                                offset += snprintf(hex_buf + offset, sizeof(hex_buf) - offset, "%s%02X", (u > 0 ? " " : ""), nfc_data->uid[u]);
                            }
                            strncpy(scanner->detected_key.data_hex, hex_buf, sizeof(scanner->detected_key.data_hex) - 1);

                            snprintf(scanner->detected_key.atqa, sizeof(scanner->detected_key.atqa), "%02X %02X", nfc_data->atqa[0], nfc_data->atqa[1]);
                            snprintf(scanner->detected_key.sak, sizeof(scanner->detected_key.sak), "%02X", nfc_data->sak);

                            scanner->key_detected = true;
                            scanner->is_scanning = false;
                            furi_mutex_release(scanner->mutex);
                            nfc_poller_free(poller);
                            break;
                        }
                    }
                    nfc_poller_free(poller);
                }
                furi_delay_ms(80);
            }
        }

        if(!scanner->is_scanning) continue;

        // ==========================================
        // PHASE 3: 1-Wire iButton
        // ==========================================
        if(scanner->ibutton_protocols && scanner->ibutton_key) {
            for(int i = 0; i < 3 && scanner->is_scanning; i++) {
                if(ibutton_protocols_read(scanner->ibutton_protocols, scanner->ibutton_key)) {
                    furi_mutex_acquire(scanner->mutex, FuriWaitForever);
                    memset(&scanner->detected_key, 0, sizeof(DetectedKey));
                    scanner->detected_key.category = KeyCategoryIButton;
                    scanner->detected_key.valid = true;

                    iButtonProtocolId proto_id = ibutton_key_get_protocol_id(scanner->ibutton_key);
                    const char* name = ibutton_protocols_get_name(scanner->ibutton_protocols, proto_id);
                    strncpy(scanner->detected_key.protocol_name, name ? name : "Dallas", sizeof(scanner->detected_key.protocol_name) - 1);

                    FuriString* uid_str = furi_string_alloc();
                    ibutton_protocols_render_uid(scanner->ibutton_protocols, scanner->ibutton_key, uid_str);
                    strncpy(scanner->detected_key.data_hex, furi_string_get_cstr(uid_str), sizeof(scanner->detected_key.data_hex) - 1);
                    furi_string_free(uid_str);

                    scanner->key_detected = true;
                    scanner->is_scanning = false;
                    furi_mutex_release(scanner->mutex);
                    break;
                }
                furi_delay_ms(80);
            }
        }
    }

    return 0;
}

KeyScanner* key_scanner_alloc(void) {
    KeyScanner* scanner = malloc(sizeof(KeyScanner));
    if(!scanner) return NULL;
    memset(scanner, 0, sizeof(KeyScanner));

    scanner->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    scanner->is_thread_running = true;

    // Allocate iButton
    scanner->ibutton_protocols = ibutton_protocols_alloc();
    if(scanner->ibutton_protocols) {
        scanner->ibutton_key = ibutton_key_alloc(ibutton_protocols_get_max_data_size(scanner->ibutton_protocols));
    }

    // Allocate LFRFID
    scanner->rfid_dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
    if(scanner->rfid_dict) {
        scanner->rfid_worker = lfrfid_worker_alloc(scanner->rfid_dict);
        if(scanner->rfid_worker) {
            lfrfid_worker_start_thread(scanner->rfid_worker);
        }
    }

    // Allocate NFC
    scanner->nfc = nfc_alloc();

    // Start scanner background worker thread
    scanner->thread = furi_thread_alloc();
    furi_thread_set_name(scanner->thread, "KeyWizWorker");
    furi_thread_set_context(scanner->thread, scanner);
    furi_thread_set_stack_size(scanner->thread, 2048);
    furi_thread_set_callback(scanner->thread, key_scanner_worker);
    furi_thread_start(scanner->thread);

    return scanner;
}

void key_scanner_free(KeyScanner* scanner) {
    if(!scanner) return;

    scanner->is_scanning = false;
    scanner->is_thread_running = false;
    if(scanner->thread) {
        // Stop worker thread
        furi_thread_join(scanner->thread);
        furi_thread_free(scanner->thread);
    }

    if(scanner->nfc) {
        nfc_free(scanner->nfc);
    }

    if(scanner->rfid_worker) {
        lfrfid_worker_stop(scanner->rfid_worker);
        lfrfid_worker_stop_thread(scanner->rfid_worker);
        lfrfid_worker_free(scanner->rfid_worker);
    }
    if(scanner->rfid_dict) {
        protocol_dict_free(scanner->rfid_dict);
    }

    if(scanner->ibutton_key) {
        ibutton_key_free(scanner->ibutton_key);
    }
    if(scanner->ibutton_protocols) {
        ibutton_protocols_free(scanner->ibutton_protocols);
    }

    if(scanner->mutex) {
        furi_mutex_free(scanner->mutex);
    }

    free(scanner);
}

void key_scanner_start(KeyScanner* scanner) {
    if(!scanner) return;
    furi_mutex_acquire(scanner->mutex, FuriWaitForever);
    scanner->key_detected = false;
    scanner->is_scanning = true;
    furi_mutex_release(scanner->mutex);
}

void key_scanner_stop(KeyScanner* scanner) {
    if(!scanner) return;
    furi_mutex_acquire(scanner->mutex, FuriWaitForever);
    scanner->is_scanning = false;
    furi_mutex_release(scanner->mutex);
}

bool key_scanner_poll_step(KeyScanner* scanner, DetectedKey* out_key) {
    if(!scanner || !out_key) return false;

    bool found = false;
    furi_mutex_acquire(scanner->mutex, FuriWaitForever);
    if(scanner->key_detected) {
        *out_key = scanner->detected_key;
        scanner->key_detected = false;
        found = true;
    }
    furi_mutex_release(scanner->mutex);

    return found;
}

void key_scanner_simulate_detection(KeyScanner* scanner, KeyCategory category, DetectedKey* out_key) {
    UNUSED(scanner);
    if(!out_key) return;

    memset(out_key, 0, sizeof(DetectedKey));
    out_key->category = category;
    out_key->valid = true;

    if(category == KeyCategoryLFRFID) {
        strncpy(out_key->protocol_name, "EM4100", sizeof(out_key->protocol_name) - 1);
        uint8_t uid[5] = {0x01, 0x23, 0x45, 0x67, 0x89};
        memcpy(out_key->data, uid, 5);
        out_key->data_len = 5;
        snprintf(out_key->data_hex, sizeof(out_key->data_hex), "01 23 45 67 89");
    } else if(category == KeyCategoryNFC) {
        strncpy(out_key->protocol_name, "Mifare Classic", sizeof(out_key->protocol_name) - 1);
        uint8_t uid[4] = {0xDE, 0xAD, 0xBE, 0xEF};
        memcpy(out_key->data, uid, 4);
        out_key->data_len = 4;
        snprintf(out_key->data_hex, sizeof(out_key->data_hex), "DE AD BE EF");
        strncpy(out_key->atqa, "00 04", sizeof(out_key->atqa));
        strncpy(out_key->sak, "08", sizeof(out_key->sak));
    } else if(category == KeyCategoryIButton) {
        strncpy(out_key->protocol_name, "Dallas", sizeof(out_key->protocol_name) - 1);
        uint8_t rom[8] = {0x01, 0x00, 0xBE, 0x11, 0xAA, 0x00, 0x00, 0xFB};
        memcpy(out_key->data, rom, 8);
        out_key->data_len = 8;
        snprintf(out_key->data_hex, sizeof(out_key->data_hex), "01 00 BE 11 AA 00 00 FB");
    }
}
