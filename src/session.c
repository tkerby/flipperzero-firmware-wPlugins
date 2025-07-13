#include <furi.h>
#include <storage/storage.h>
#include "session.h"

void EnsureLoadedSession(const PSESSION session) {
	furi_check(session);
	Storage* storage = furi_record_open(RECORD_STORAGE);
	furi_check(storage);
	File* file = storage_file_alloc(storage);
	storage_dir_open(file, APP_DATA_PATH("sessions"));
	char name[2048];
	storage_dir_read(0, 0, name, sizeof(name) / sizeof(char));
	storage_dir_close(file);
	storage_file_free(file);
	furi_record_close(RECORD_STORAGE);
}
