#include "cuberzero.h"
#include <storage/storage.h>

#define DEFAULT_SETTINGS_CUBE WCA_3X3

static const char* const FileSettings = APP_DATA_PATH("cuberzero_settings.cbzs");

void CuberZeroSettingsLoad(const PCUBERZEROSETTINGS settings) {
    Storage* const storage = furi_record_open(RECORD_STORAGE);
    bool loaded = false;

    if(!storage) {
        goto loadDefault;
    }

    File* const file = storage_file_alloc(storage);

    if(!file) {
        goto closeStorage;
    }

    if(!storage_file_open(file, FileSettings, FSAM_READ, FSOM_OPEN_EXISTING) ||
       storage_file_size(file) < sizeof(CUBERZEROSETTINGS) ||
       storage_file_read(file, settings, sizeof(CUBERZEROSETTINGS)) < sizeof(CUBERZEROSETTINGS)) {
        goto freeFile;
    }

    if(settings->cube >= CUBERZERO_CUBE_COUNT) {
        settings->cube = DEFAULT_SETTINGS_CUBE;
    }

    loaded = true;
freeFile:
    storage_file_close(file);
    storage_file_free(file);
closeStorage:
    furi_record_close(RECORD_STORAGE);
loadDefault:
    if(!loaded) {
        settings->cube = DEFAULT_SETTINGS_CUBE;
    }
}

void CuberZeroSettingsSave(const PCUBERZEROSETTINGS settings) {
    Storage* const storage = furi_record_open(RECORD_STORAGE);

    if(!storage) {
        return;
    }

    File* const file = storage_file_alloc(storage);

    if(!file) {
        goto closeStorage;
    }

    if(!storage_file_open(file, FileSettings, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        goto freeFile;
    }

    storage_file_write(file, settings, sizeof(CUBERZEROSETTINGS));
freeFile:
    storage_file_close(file);
    storage_file_free(file);
closeStorage:
    furi_record_close(RECORD_STORAGE);
}
