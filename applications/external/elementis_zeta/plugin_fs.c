// write_file for the plugins (device only; contract 25.6). The writers of the card used to live in assets.c, i.e. resident, but nothing resident writes a file:
// the UI plugin writes settings.bin and save.bin, the PC plugin writes save.bin and the box files. Each of them carries this one function (one plugin
// is loaded at a time); the host harness has its own in tools/host/shim_assets.c, so this file is NOT in the host Makefiles.
#include "assets.h"
#include <furi.h>
#include <storage/storage.h>
#include <stdio.h>
#include <string.h>

bool write_file(const char* path, const void* data, uint32_t len) {
    char tmp[96], dir[96];
    if(path[0] != '/') return false;
    if(snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return false;
    memcpy(
        dir,
        path,
        strlen(path) +
            1); // fits: the ".tmp" name above is < 96 B, so the path is < 92 (a snprintf here trips -Wformat-truncation when the plugin is one translation unit)
    Storage* st = furi_record_open(RECORD_STORAGE);
    for(char* p = dir + 1; *p; p++) { // mkdir each parent folder in turn
        if(*p != '/') continue;
        *p = 0;
        storage_simply_mkdir(st, dir);
        *p = '/';
    }
    bool ok = false;
    File* f = storage_file_alloc(st);
    if(storage_file_open(f, tmp, FSAM_WRITE, FSOM_CREATE_ALWAYS)) {
        ok = storage_file_write(f, data, len) == len;
        ok = storage_file_sync(f) && ok;
        storage_file_close(f);
    }
    storage_file_free(f);
    if(ok) {
        storage_simply_remove(st, path); // rename does not overwrite
        ok = storage_common_rename(st, tmp, path) == FSE_OK;
    }
    if(!ok) storage_simply_remove(st, tmp);
    furi_record_close(RECORD_STORAGE);
    return ok;
}
