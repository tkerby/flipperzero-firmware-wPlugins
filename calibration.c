#include "calibration.h"

// File format: "dry=XXXX\nwet=XXXX\n"

bool calibration_load(MoistureSensorApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool success = false;

    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    if(storage_file_open(file, CALIBRATION_FILE_PATH, FSAM_READ, FSOM_OPEN_EXISTING)) {
        char buffer[64];
        uint16_t bytes_read = storage_file_read(file, buffer, sizeof(buffer) - 1);
        if(bytes_read > 0) {
            buffer[bytes_read] = '\0';
            uint16_t dry_val, wet_val;
            if(sscanf(buffer, "dry=%hu\nwet=%hu", &dry_val, &wet_val) == 2) {
                app->cal_dry_value = dry_val;
                app->cal_wet_value = wet_val;
                success = true;
            }
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}

bool calibration_save(MoistureSensorApp* app) {
    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Ensure app data directory exists
    storage_common_mkdir(storage, APP_DATA_PATH(""));

    File* file = storage_file_alloc(storage);
    bool success = false;

    if(!file) {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    if(storage_file_open(file, CALIBRATION_FILE_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        char buffer[64];
        int len = snprintf(
            buffer, sizeof(buffer), "dry=%u\nwet=%u\n", app->cal_dry_value, app->cal_wet_value);
        if(len > 0 && (size_t)len < sizeof(buffer)) {
            success = (storage_file_write(file, buffer, len) == (uint16_t)len);
        }
    }

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return success;
}
