#include "cuberzero.h"
#include <storage/storage.h>

static const char* const SettingsFile = APP_DATA_PATH("cuberzero_settings.bin");

void SettingsLoad() {
	Storage* storage = furi_record_open(RECORD_STORAGE);

	if(!storage) {
		goto loadDefaults;
	}

	if(!storage_file_exists(storage, SettingsFile)) {
		FURI_LOG_I(CUBERZERO_TAG, "File does not exist");
		furi_record_close(RECORD_STORAGE);
		goto loadDefaults;
	}

	FURI_LOG_I(CUBERZERO_TAG, "File does exist");
	File* file = storage_file_alloc(storage);

	if(!file) {
		furi_record_close(RECORD_STORAGE);
		goto loadDefaults;
	}

	//storage_file_open(file, SettingsFile, NULL, NULL);
	storage_file_free(file);
	return;
loadDefaults:
	return;
}

void SettingsSave() {
}
