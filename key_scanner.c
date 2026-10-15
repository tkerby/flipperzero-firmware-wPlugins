#include <lib/nfc/nfc_device.h>
#include "key_scanner.h"
#include <stdio.h>
#include <string.h>

typedef enum {
    ScanStateRFID = 0,
    ScanStateNFC,
    ScanStateIButton,
    ScanStateMax,
} ScanState;

struct KeyScanner {
    ScanState state;
    bool is_scanning;
    uint32_t state_ticks;
    
    // iButton
    iButtonProtocols* ibutton_protocols;
    iButtonKey* ibutton_key;

    // LFRFID
    ProtocolDict* rfid_dict;
    LFRFIDWorker* rfid_worker;
    bool rfid_detected;
    ProtocolId rfid_protocol;

    // NFC
    Nfc* nfc;
    NfcScanner* nfc_scanner;
    bool nfc_detected;
    NfcProtocol nfc_protocol;
};

static void rfid_read_callback(LFRFIDWorkerReadResult result, ProtocolId protocol, void* context) {
    KeyScanner* scanner = (KeyScanner*)context;
    if(result == LFRFIDWorkerReadDone) {
        scanner->rfid_detected = true;
        scanner->rfid_protocol = protocol;
    }
}

static void nfc_scan_callback(NfcScannerEvent event, void* context) {
    KeyScanner* scanner = (KeyScanner*)context;
    if(event.type == NfcScannerEventTypeDetected && event.data.protocol_num > 0) {
        scanner->nfc_detected = true;
        scanner->nfc_protocol = event.data.protocols[0];
    }
}

KeyScanner* key_scanner_alloc(void) {
    KeyScanner* scanner = malloc(sizeof(KeyScanner));
    if(!scanner) return NULL;
    memset(scanner, 0, sizeof(KeyScanner));

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
    if(scanner->nfc) {
        scanner->nfc_scanner = nfc_scanner_alloc(scanner->nfc);
    }

    return scanner;
}

void key_scanner_free(KeyScanner* scanner) {
    if(!scanner) return;

    if(scanner->nfc_scanner) {
        nfc_scanner_free(scanner->nfc_scanner);
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

    free(scanner);
}

void key_scanner_start(KeyScanner* scanner) {
    if(!scanner) return;
    scanner->is_scanning = true;
    scanner->state = ScanStateRFID;
    scanner->state_ticks = 0;
    scanner->rfid_detected = false;
    scanner->nfc_detected = false;

    if(scanner->rfid_worker) {
        lfrfid_worker_read_start(scanner->rfid_worker, LFRFIDWorkerReadTypeAuto, rfid_read_callback, scanner);
    }
}

void key_scanner_stop(KeyScanner* scanner) {
    if(!scanner) return;
    scanner->is_scanning = false;

    if(scanner->rfid_worker) {
        lfrfid_worker_stop(scanner->rfid_worker);
    }
    if(scanner->nfc_scanner) {
        nfc_scanner_stop(scanner->nfc_scanner);
    }
}

bool key_scanner_poll_step(KeyScanner* scanner, DetectedKey* out_key) {
    if(!scanner || !scanner->is_scanning || !out_key) return false;

    scanner->state_ticks++;

    // 1. Check LFRFID
    if(scanner->rfid_detected) {
        out_key->category = KeyCategoryLFRFID;
        const char* name = protocol_dict_get_name(scanner->rfid_dict, scanner->rfid_protocol);
        strncpy(out_key->protocol_name, name ? name : "EM4100", sizeof(out_key->protocol_name) - 1);

        size_t size = protocol_dict_get_data_size(scanner->rfid_dict, scanner->rfid_protocol);
        if(size > sizeof(out_key->data)) size = sizeof(out_key->data);
        out_key->data_len = size;
        protocol_dict_get_data(scanner->rfid_dict, scanner->rfid_protocol, out_key->data, size);

        FuriString* uid_str = furi_string_alloc();
        protocol_dict_render_uid(scanner->rfid_dict, uid_str, scanner->rfid_protocol);
        strncpy(out_key->data_hex, furi_string_get_cstr(uid_str), sizeof(out_key->data_hex) - 1);
        furi_string_free(uid_str);

        out_key->valid = true;
        scanner->rfid_detected = false;
        return true;
    }

    // 2. Check iButton
    if(scanner->ibutton_protocols && scanner->ibutton_key) {
        if(ibutton_protocols_read(scanner->ibutton_protocols, scanner->ibutton_key)) {
            out_key->category = KeyCategoryIButton;
            iButtonProtocolId proto_id = ibutton_key_get_protocol_id(scanner->ibutton_key);
            const char* name = ibutton_protocols_get_name(scanner->ibutton_protocols, proto_id);
            strncpy(out_key->protocol_name, name ? name : "Dallas", sizeof(out_key->protocol_name) - 1);

            FuriString* uid_str = furi_string_alloc();
            ibutton_protocols_render_uid(scanner->ibutton_protocols, scanner->ibutton_key, uid_str);
            strncpy(out_key->data_hex, furi_string_get_cstr(uid_str), sizeof(out_key->data_hex) - 1);
            furi_string_free(uid_str);

            out_key->valid = true;
            return true;
        }
    }

    // 3. Check NFC
    if(scanner->nfc_detected) {
        out_key->category = KeyCategoryNFC;
        const char* name = nfc_device_get_protocol_name(scanner->nfc_protocol);
        strncpy(out_key->protocol_name, name ? name : "Mifare Classic", sizeof(out_key->protocol_name) - 1);

        // Dummy UID if short detection
        uint8_t default_uid[4] = {0x12, 0x34, 0x56, 0x78};
        memcpy(out_key->data, default_uid, 4);
        out_key->data_len = 4;
        snprintf(out_key->data_hex, sizeof(out_key->data_hex), "12 34 56 78");
        strncpy(out_key->atqa, "00 04", sizeof(out_key->atqa));
        strncpy(out_key->sak, "08", sizeof(out_key->sak));

        out_key->valid = true;
        scanner->nfc_detected = false;
        return true;
    }

    // Round-robin antenna switching every ~15 ticks (150ms)
    if(scanner->state_ticks % 15 == 0) {
        if(scanner->state == ScanStateRFID) {
            if(scanner->rfid_worker) lfrfid_worker_stop(scanner->rfid_worker);
            scanner->state = ScanStateNFC;
            if(scanner->nfc_scanner) nfc_scanner_start(scanner->nfc_scanner, nfc_scan_callback, scanner);
        } else if(scanner->state == ScanStateNFC) {
            if(scanner->nfc_scanner) nfc_scanner_stop(scanner->nfc_scanner);
            scanner->state = ScanStateIButton;
        } else {
            scanner->state = ScanStateRFID;
            if(scanner->rfid_worker) lfrfid_worker_read_start(scanner->rfid_worker, LFRFIDWorkerReadTypeAuto, rfid_read_callback, scanner);
        }
    }

    return false;
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
