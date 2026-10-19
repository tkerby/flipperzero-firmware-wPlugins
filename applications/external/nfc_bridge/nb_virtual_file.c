#include "nb_virtual_file.h"
#include <furi.h>
#include <storage/storage.h>

#define NBV_FILE_MAX (64 * 1024) /* a 4K Classic .nfc is ~20 KB; anything bigger is not a card */

bool nbv_save_path(NbVirtual* v, const char* path) {
    if(!v || !path) return false;
    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    do {
        if(!storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
            break;
        }
        size_t size = (v->kind == KindMfc) ? (v->units * 16) : (v->units * 4);
        if(storage_file_write(file, v->data, size) != size) {
            break;
        }
        ok = true;
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}

bool nbv_load_path(NbVirtual* v, const char* path) {
    // ... (rest of the file)

    Storage* storage = furi_record_open(RECORD_STORAGE);
    File* file = storage_file_alloc(storage);
    bool ok = false;

    nbv_load_begin(v);
    do {
        if(!storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
            nbv_fail(v, "Cannot open file");
            break;
        }
        if(storage_file_size(file) > NBV_FILE_MAX) {
            nbv_fail(v, "File too big for a card dump");
            break;
        }
        uint8_t chunk[256]; /* the loader streams: the whole file is never held in RAM */
        bool good = true;
        while(good && !storage_file_eof(file)) {
            uint16_t n = storage_file_read(file, chunk, sizeof(chunk));
            if(n == 0) break;
            good = nbv_load_feed(v, chunk, n);
        }
        if(!good) break;
        if(!storage_file_eof(file)) {
            nbv_fail(v, "Read error");
            break;
        }
        ok = nbv_load_end(v);
    } while(false);

    storage_file_close(file);
    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
