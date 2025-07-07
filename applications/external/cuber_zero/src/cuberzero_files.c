#include <storage/storage.h>

#define SESSIONS APP_DATA_PATH("sessions")

void filesInitialize() {
    FURI_LOG_I("Files", "---------- Begin ----------");
    Storage* storage = furi_record_open(RECORD_STORAGE);

    if(!storage) {
        goto stop;
    }

    storage_simply_mkdir(storage, SESSIONS);
    File* file = storage_file_alloc(storage);

    if(!file) {
        goto close;
    }

    if(!storage_dir_open(file, SESSIONS)) {
        goto closeDirectory;
    }

    char name[2048];

    while(storage_dir_read(file, 0, name, 2048)) {
        FURI_LOG_I("Files", "File: %s", name);
    }
closeDirectory:
    storage_dir_close(file);
    storage_file_free(file);
close:
    furi_record_close(RECORD_STORAGE);
stop:
    FURI_LOG_I("Files", "---------- End ----------");
}
