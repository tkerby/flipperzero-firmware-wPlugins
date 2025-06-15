#include "cuberzero.h"
#include <storage/storage.h>

#define SETTINGS_CUBE_DEFAULT WCA_3X3

static const char* const SettingsFile = APP_DATA_PATH("cuberzero_settings.cbzs");

void SettingsLoad(const PCUBERZEROSETTINGS settings) {
	Storage* const storage = furi_record_open(RECORD_STORAGE);
	bool loaded = false;

	if(!storage) {
		goto loadDefault;
	}

	File* const file = storage_file_alloc(storage);

	if(!file) {
		goto closeStorage;
	}

	if(!storage_file_open(file, SettingsFile, FSAM_READ, FSOM_OPEN_EXISTING) || storage_file_size(file) < sizeof(CUBERZEROSETTINGS) || storage_file_read(file, settings, sizeof(CUBERZEROSETTINGS)) < sizeof(CUBERZEROSETTINGS)) {
		goto freeFile;
	}

	if(settings->cube >= CUBERZERO_CUBE_COUNT) {
		settings->cube = SETTINGS_CUBE_DEFAULT;
	}

	loaded = true;
freeFile:
	storage_file_close(file);
	storage_file_free(file);
closeStorage:
	furi_record_close(RECORD_STORAGE);
loadDefault:
	if(loaded) {
		return;
	}

	settings->cube = SETTINGS_CUBE_DEFAULT;
}

void SettingsSave() {
}
