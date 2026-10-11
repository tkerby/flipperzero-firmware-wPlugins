#pragma once
#include <furi.h>
typedef struct Storage Storage;
typedef struct File File;
typedef struct { uint64_t size; uint8_t flags; } FileInfo;
typedef enum { FSE_OK = 0, FSE_NOT_EXIST, FSE_INTERNAL, FSE_EXIST } FS_Error;
typedef enum { FSAM_READ = 1, FSAM_WRITE = 2 } FS_AccessMode;
typedef enum { FSOM_OPEN_EXISTING = 1, FSOM_CREATE_ALWAYS = 2, FSOM_CREATE_NEW = 3 } FS_OpenMode;
#define STORAGE_EXT_PATH_PREFIX "/ext"
static inline bool file_info_is_dir(const FileInfo* i) { return i->flags & 1; }
File* storage_file_alloc(Storage* s);
void storage_file_free(File* f);
bool storage_file_open(File* f, const char* path, FS_AccessMode a, FS_OpenMode m);
bool storage_file_close(File* f);
size_t storage_file_read(File* f, void* buf, size_t n);
size_t storage_file_write(File* f, const void* buf, size_t n);
uint64_t storage_file_size(File* f);
bool storage_dir_open(File* f, const char* path);
bool storage_dir_read(File* f, FileInfo* info, char* name, uint16_t cap);
bool storage_dir_close(File* f);
FS_Error storage_common_stat(Storage* s, const char* path, FileInfo* info);
FS_Error storage_common_remove(Storage* s, const char* path);
FS_Error storage_common_rename(Storage* s, const char* a, const char* b);
bool storage_simply_mkdir(Storage* s, const char* path);
bool storage_simply_remove_recursive(Storage* s, const char* path);
const char* storage_error_get_desc(FS_Error e);
FS_Error storage_common_fs_info(Storage* s, const char* p, uint64_t* total, uint64_t* free_);
