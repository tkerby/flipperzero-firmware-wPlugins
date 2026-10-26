#include "key_saver.h"
#include <flipper_format/flipper_format.h>
#include <lib/nfc/nfc_device.h>
#include <lib/nfc/protocols/iso14443_3a/iso14443_3a.h>
#include <stdio.h>
#include <string.h>

const char* key_category_to_string(KeyCategory cat) {
    switch(cat) {
    case KeyCategoryLFRFID:
        return "125 kHz RFID";
    case KeyCategoryNFC:
        return "13.56 MHz NFC";
    case KeyCategoryIButton:
        return "1-Wire iButton";
    default:
        return "Unknown";
    }
}

const char* key_category_to_asset_dir(KeyCategory cat) {
    switch(cat) {
    case KeyCategoryLFRFID:
        return "/ext/lfrfid";
    case KeyCategoryNFC:
        return "/ext/nfc";
    case KeyCategoryIButton:
        return "/ext/ibutton";
    default:
        return "/ext";
    }
}

const char* key_category_to_extension(KeyCategory cat) {
    switch(cat) {
    case KeyCategoryLFRFID:
        return ".rfid";
    case KeyCategoryNFC:
        return ".nfc";
    case KeyCategoryIButton:
        return ".ibtn";
    default:
        return ".txt";
    }
}

bool key_saver_save_to_native_asset(DetectedKey* key, char* out_path, size_t out_path_size) {
    if(!key || !key->valid) return false;

    Storage* storage = furi_record_open(RECORD_STORAGE);
    if(!storage) return false;

    const char* asset_dir = key_category_to_asset_dir(key->category);
    const char* ext = key_category_to_extension(key->category);

    // Ensure the native asset directory exists
    storage_simply_mkdir(storage, asset_dir);

    // Pick appropriate base name
    const char* base_name = "Key";
    if(key->category == KeyCategoryNFC) {
        base_name = "Card";
    } else if(key->category == KeyCategoryIButton) {
        base_name = (strstr(key->protocol_name, "Cyfral") != NULL)  ? "Cyfral" :
                    (strstr(key->protocol_name, "Metakom") != NULL) ? "Metakom" :
                                                                      "Dallas";
    } else if(key->category == KeyCategoryLFRFID) {
        base_name = (strstr(key->protocol_name, "HID") != NULL)    ? "HID" :
                    (strstr(key->protocol_name, "Indala") != NULL) ? "Indala" :
                                                                     "EM4100";
    }

    // Generate unique non-colliding file name in the native directory
    FuriString* next_filename = furi_string_alloc();
    storage_get_next_filename(storage, asset_dir, base_name, ext, next_filename, 32);

    char full_path[128];
    snprintf(
        full_path,
        sizeof(full_path),
        "%s/%s%s",
        asset_dir,
        furi_string_get_cstr(next_filename),
        ext);
    furi_string_free(next_filename);

    bool success = false;

    if(key->category == KeyCategoryNFC) {
        Iso14443_3aData nfc_data;
        memset(&nfc_data, 0, sizeof(Iso14443_3aData));
        size_t uid_len = key->data_len;
        if(uid_len > ISO14443_3A_MAX_UID_SIZE) uid_len = ISO14443_3A_MAX_UID_SIZE;
        memcpy(nfc_data.uid, key->data, uid_len);
        nfc_data.uid_len = (uint8_t)uid_len;

        // Default ATQA/SAK if empty (e.g. Mifare Classic standard 00 04 / 08)
        if(key->atqa_raw[0] == 0 && key->atqa_raw[1] == 0) {
            nfc_data.atqa[0] = 0x04;
            nfc_data.atqa[1] = 0x00;
        } else {
            nfc_data.atqa[0] = key->atqa_raw[0];
            nfc_data.atqa[1] = key->atqa_raw[1];
        }
        if(key->sak_raw == 0) {
            nfc_data.sak = 0x08;
        } else {
            nfc_data.sak = key->sak_raw;
        }

        NfcDevice* nfc_dev = nfc_device_alloc();
        if(nfc_dev) {
            nfc_device_set_data(nfc_dev, NfcProtocolIso14443_3a, &nfc_data);
            success = nfc_device_save(nfc_dev, full_path);
            nfc_device_free(nfc_dev);
        }
    } else {
        FlipperFormat* file = flipper_format_file_alloc(storage);
        do {
            if(!flipper_format_file_open_always(file, full_path)) break;

            if(key->category == KeyCategoryLFRFID) {
                // Native Flipper RFID format
                if(!flipper_format_write_header_cstr(file, "Flipper RFID key", 1)) break;
                if(!flipper_format_write_string_cstr(file, "Key type", key->protocol_name)) break;
                if(!flipper_format_write_hex(file, "Data", key->data, (uint16_t)key->data_len))
                    break;
                success = true;
            } else if(key->category == KeyCategoryIButton) {
                // Native Flipper iButton format
                if(!flipper_format_write_header_cstr(file, "Flipper iButton key", 1)) break;
                if(!flipper_format_write_string_cstr(file, "Key type", key->protocol_name)) break;
                if(!flipper_format_write_hex(file, "Data", key->data, (uint16_t)key->data_len))
                    break;
                success = true;
            }
        } while(0);

        flipper_format_file_close(file);
        flipper_format_free(file);
    }

    furi_record_close(RECORD_STORAGE);

    if(success) {
        strncpy(key->saved_path, full_path, sizeof(key->saved_path) - 1);
        if(out_path && out_path_size > 0) {
            strncpy(out_path, full_path, out_path_size - 1);
            out_path[out_path_size - 1] = '\0';
        }
    }

    return success;
}

const char* scan_mode_to_string(ScanMode mode) {
    switch(mode) {
    case ScanModeAuto:
        return "AUTO 3-in-1";
    case ScanModeRFIDOnly:
        return "RFID 125kHz";
    case ScanModeNFCOnly:
        return "NFC 13.56MHz";
    case ScanModeIButtonOnly:
        return "iButton 1-Wire";
    default:
        return "Unknown";
    }
}
