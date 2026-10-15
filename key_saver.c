#include "key_saver.h"
#include <flipper_format/flipper_format.h>
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
        base_name = (strstr(key->protocol_name, "Cyfral") != NULL) ? "Cyfral" :
                    (strstr(key->protocol_name, "Metakom") != NULL) ? "Metakom" : "Dallas";
    } else if(key->category == KeyCategoryLFRFID) {
        base_name = (strstr(key->protocol_name, "HID") != NULL) ? "HID" :
                    (strstr(key->protocol_name, "Indala") != NULL) ? "Indala" : "EM4100";
    }

    // Generate unique non-colliding file name in the native directory
    FuriString* next_filename = furi_string_alloc();
    storage_get_next_filename(storage, asset_dir, base_name, ext, next_filename, 32);

    char full_path[128];
    snprintf(full_path, sizeof(full_path), "%s/%s%s", asset_dir, furi_string_get_cstr(next_filename), ext);
    furi_string_free(next_filename);

    FlipperFormat* file = flipper_format_file_alloc(storage);
    bool success = false;

    do {
        if(!flipper_format_file_open_always(file, full_path)) break;

        if(key->category == KeyCategoryLFRFID) {
            // Native Flipper RFID format
            if(!flipper_format_write_header_cstr(file, "Flipper RFID key", 1)) break;
            if(!flipper_format_write_string_cstr(file, "Key type", key->protocol_name)) break;
            if(!flipper_format_write_hex(file, "Data", key->data, (uint16_t)key->data_len)) break;
            success = true;
        } else if(key->category == KeyCategoryIButton) {
            // Native Flipper iButton format
            if(!flipper_format_write_header_cstr(file, "Flipper iButton key", 1)) break;
            if(!flipper_format_write_string_cstr(file, "Key type", key->protocol_name)) break;
            if(!flipper_format_write_hex(file, "Data", key->data, (uint16_t)key->data_len)) break;
            success = true;
        } else if(key->category == KeyCategoryNFC) {
            // Native Flipper NFC format
            if(!flipper_format_write_header_cstr(file, "Flipper NFC device", 3)) break;
            if(!flipper_format_write_string_cstr(file, "Device type", key->protocol_name)) break;
            if(!flipper_format_write_hex(file, "UID", key->data, (uint16_t)key->data_len)) break;
            if(strlen(key->atqa) > 0) {
                flipper_format_write_string_cstr(file, "ATQA", key->atqa);
            }
            if(strlen(key->sak) > 0) {
                flipper_format_write_string_cstr(file, "SAK", key->sak);
            }
            success = true;
        }
    } while(0);

    flipper_format_file_close(file);
    flipper_format_free(file);
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
