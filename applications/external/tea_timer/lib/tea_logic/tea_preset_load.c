#include "tea_preset_load.h"

#include <furi.h>
#include <stdio.h>
#include <string.h>

#include "tea_preset.h"

/** Скопировать basename пути в поле ошибки. */
static void tea_preset_error_set_file_name(PresetError* err, const char* path) {
    if(err == NULL || path == NULL) {
        return;
    }
    const char* basename = path;
    for(const char* p = path; *p != '\0'; p++) {
        if(*p == '/') {
            basename = p + 1;
        }
    }
    snprintf(err->file_name, sizeof(err->file_name), "%s", basename);
}

TeaPresetResult tea_preset_load(
    Storage* storage,
    const char* path,
    char* buf,
    size_t buf_size,
    TeaPreset* out,
    PresetError* err) {
    furi_check(storage != NULL);
    furi_check(path != NULL);
    furi_check(buf != NULL);
    furi_check(out != NULL);

    if(err != NULL) {
        memset(err, 0, sizeof(PresetError));
        err->result = TeaPresetResultErrNotFound;
    }
    tea_preset_error_set_file_name(err, path);

    TeaPresetResult result = TeaPresetResultErrNotFound;
    char error_field[24];
    error_field[0] = '\0';

    File* file = storage_file_alloc(storage);
    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            result = TeaPresetResultErrNotFound;
            break;
        }

        uint64_t size = storage_file_size(file);
        if(size > TEA_FILE_MAX_SIZE || size + 1 > buf_size) {
            result = TeaPresetResultErrTooLarge;
            break;
        }

        size_t read = storage_file_read(file, buf, (size_t)size);
        if(read != (size_t)size) {
            result = TeaPresetResultErrNotFound;
            break;
        }
        buf[read] = '\0';

        result = tea_preset_parse_buf(buf, read, out, error_field, sizeof(error_field));
        if(result == TeaPresetResultOK) {
            snprintf(out->path, sizeof(out->path), "%s", path);
        }
    } while(false);

    // Файл закрывается всегда, даже при ошибке открытия.
    storage_file_close(file);
    storage_file_free(file);

    if(err != NULL) {
        err->result = result;
        snprintf(err->error_field, sizeof(err->error_field), "%s", error_field);
    }

    return result;
}
