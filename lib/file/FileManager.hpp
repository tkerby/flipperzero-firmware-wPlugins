#pragma once

#include <toolbox/path.h>
#include <storage/storage.h>

#include "FlipperFile.hpp"
#include "Directory.hpp"

class FileManager {
private:
    Storage* storage;

public:
    FileManager() {
        storage = (Storage*)furi_record_open(RECORD_STORAGE);
    }

    Directory* OpenDirectory(const char* path) {
        Directory* dir = new Directory(storage, path);
        if(dir->IsOpened()) {
            return dir;
        }
        delete dir;
        return NULL;
    }

    FlipperFile* OpenRead(const char* path) {
        FlipperFile* file = new FlipperFile(storage, path, false);
        if(file->IsOpened()) {
            return file;
        }
        delete file;
        return NULL;
    }

    FlipperFile* OpenWrite(const char* path) {
        FlipperFile* file = new FlipperFile(storage, path, true);
        if(file->IsOpened()) {
            return file;
        }
        delete file;
        return NULL;
    }

    ~FileManager() {
        furi_record_close(RECORD_STORAGE);
    }
};
