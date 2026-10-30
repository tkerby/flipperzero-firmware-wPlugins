#include "key_saver.h"
#include <lib/ibutton/ibutton_protocols.h>
#include <lib/lfrfid/protocols/lfrfid_protocols.h>
#include <lib/lfrfid/lfrfid_dict_file.h>
#include <toolbox/protocols/protocol_dict.h>
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
    } else if(key->category == KeyCategoryIButton) {
        iButtonProtocols* proto = ibutton_protocols_alloc();
        if(proto) {
            iButtonKey* key_obj = ibutton_key_alloc(ibutton_protocols_get_max_data_size(proto));
            if(key_obj) {
                iButtonProtocolId proto_id =
                    ibutton_protocols_get_id_by_name(proto, key->protocol_name);
                if(proto_id == iButtonProtocolIdInvalid) {
                    if(strstr(key->protocol_name, "Dallas") != NULL) {
                        proto_id = ibutton_protocols_get_id_by_name(proto, "DS1990");
                    } else if(strstr(key->protocol_name, "Cyfral") != NULL) {
                        proto_id = ibutton_protocols_get_id_by_name(proto, "Cyfral");
                    } else if(strstr(key->protocol_name, "Metakom") != NULL) {
                        proto_id = ibutton_protocols_get_id_by_name(proto, "Metakom");
                    }
                }
                if(proto_id == iButtonProtocolIdInvalid) {
                    proto_id = ibutton_protocols_get_id_by_name(proto, "DS1990");
                }
                ibutton_key_set_protocol_id(key_obj, proto_id);
                iButtonEditableData editable;
                ibutton_protocols_get_editable_data(proto, key_obj, &editable);
                if(editable.ptr && editable.size > 0) {
                    size_t sz = key->data_len;
                    if(sz > editable.size) sz = editable.size;
                    memcpy(editable.ptr, key->data, sz);
                    ibutton_protocols_apply_edits(proto, key_obj);
                }
                success = ibutton_protocols_save(proto, key_obj, full_path);
                ibutton_key_free(key_obj);
            }
            ibutton_protocols_free(proto);
        }
    } else if(key->category == KeyCategoryLFRFID) {
        ProtocolDict* dict = protocol_dict_alloc(lfrfid_protocols, LFRFIDProtocolMax);
        if(dict) {
            ProtocolId proto_id = protocol_dict_get_protocol_by_name(dict, key->protocol_name);
            if(proto_id == PROTOCOL_NO) {
                proto_id = LFRFIDProtocolEM4100;
            }
            protocol_dict_set_data(dict, proto_id, key->data, key->data_len);
            success = lfrfid_dict_file_save(dict, proto_id, full_path);
            protocol_dict_free(dict);
        }
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
