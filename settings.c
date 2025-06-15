#include "cuberzero.h"
#include <storage/storage.h>

static const char* const SettingsFile = APP_DATA_PATH("cuberzero_settings.cbzs");

static void loadDefaultSettings() {
}

void SettingsLoad() {
	Storage* storage = furi_record_open(RECORD_STORAGE);

	if(!storage) {
		loadDefaultSettings();
		return;
	}

	File* file = storage_file_alloc(storage);

	if(!file) {
		loadDefaultSettings();
		goto closeStorage;
	}

	if(!storage_file_open(file, SettingsFile, FSAM_READ, FSOM_OPEN_EXISTING)) {
		loadDefaultSettings();
		goto freeFile;
	}

	for(uint64_t i = 0; i < storage_file_size(file); i++) {
		FURI_LOG_I(CUBERZERO_TAG, "Counter");
	}
freeFile:
	storage_file_close(file);
	storage_file_free(file);
closeStorage:
	furi_record_close(RECORD_STORAGE);
}

void SettingsSave() {
}
