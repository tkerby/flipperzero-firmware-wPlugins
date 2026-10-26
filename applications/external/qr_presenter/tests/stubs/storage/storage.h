#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    unsigned unused;
} Storage;
typedef struct {
    int index;
    size_t position;
    size_t directory_index;
} File;
typedef struct {
    bool directory;
} FileInfo;
enum {
    FSAM_READ,
    FSAM_WRITE,
    FSOM_OPEN_EXISTING,
    FSOM_CREATE_ALWAYS
};
File* storage_file_alloc(Storage* storage);
void storage_file_free(File* file);
bool storage_file_open(File* file, const char* path, int access, int mode);
bool storage_file_close(File* file);
size_t storage_file_read(File* file, void* data, size_t length);
size_t storage_file_write(File* file, const void* data, size_t length);
uint64_t storage_file_size(File* file);
bool storage_file_sync(File* file);
bool storage_dir_open(File* file, const char* path);
bool storage_dir_read(File* file, FileInfo* info, char* name, size_t size);
bool storage_dir_close(File* file);
bool file_info_is_dir(const FileInfo* info);
bool storage_common_exists(Storage* storage, const char* path);
bool storage_simply_mkdir(Storage* storage, const char* path);
bool storage_simply_remove(Storage* storage, const char* path);
