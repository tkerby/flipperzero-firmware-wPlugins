#include "storage_glue.h"

static bool sd_blob_read(void* ctx, uint32_t offset, void* buf, uint32_t len) {
    PkbSdBlob* blob = ctx;
    if(!storage_file_seek(blob->file, offset, true)) return false;
    return storage_file_read(blob->file, buf, len) == len;
}

bool pkb_sd_blob_open(PkbSdBlob* blob, Storage* storage, const char* path, PkbBlobReader* reader) {
    blob->file = storage_file_alloc(storage);
    if(!storage_file_open(blob->file, path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        storage_file_free(blob->file);
        blob->file = NULL;
        return false;
    }
    reader->read = sd_blob_read;
    reader->ctx = blob;
    return true;
}

void pkb_sd_blob_close(PkbSdBlob* blob) {
    if(!blob->file) return;
    storage_file_close(blob->file);
    storage_file_free(blob->file);
    blob->file = NULL;
}
