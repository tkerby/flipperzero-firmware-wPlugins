#include <furi.h>
#include <storage/storage.h>
#include "session.h"

#define MAX_NAME_LENGTH 256

void EnsureLoadedSession(const PSESSION session) {
    furi_check(session);
    Storage* storage = furi_record_open(RECORD_STORAGE);
    furi_check(storage);
    File* file = storage_file_alloc(storage);
    storage_dir_open(file, APP_DATA_PATH("sessions"));
    FileInfo info;
    char name[MAX_NAME_LENGTH + 1];
    FURI_LOG_I("FileTest", "Marker");

    while(storage_dir_read(file, &info, name, MAX_NAME_LENGTH)) {
        name[MAX_NAME_LENGTH] = 0;
        FURI_LOG_I("FileTest", "File: %s", name);
    }

    storage_dir_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
}
