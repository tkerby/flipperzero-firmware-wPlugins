#pragma once

#include <toolbox/path.h>
#include <storage/storage.h>

class Directory {
private:
    bool isOpened;
    Storage* storage;
    File* dir;

public:
    Directory(Storage* storage, const char* dirPath) {
        this->storage = storage;
        dir = storage_file_alloc(storage);
        isOpened = storage_dir_open(dir, dirPath);
    }

    bool IsOpened() {
        return isOpened;
    }

    void Rewind() {
        if(isOpened) {
            storage_dir_rewind(dir);
        }
    }

    bool GetNextFile(char* name, uint16_t nameLength) {
        return isOpened && storage_dir_read(dir, NULL, name, nameLength);
    }

    ~Directory() {
        storage_dir_close(dir);
        storage_file_free(dir);
    }
};
