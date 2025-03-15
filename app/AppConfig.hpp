#pragma once

#include <cstdint>

#include "app/AppFileSystem.hpp"
#include "lib/file/FileManager.hpp"

#define KEY_FREQUENCY    "Frequency"
#define KEY_MAX_PAGERS   "MaxPagerForBatchOrDetection"
#define KEY_REPEATS      "SignalRepeats"
#define KEY_DEBUG        "Debug"
#define KEY_IGNORE_SAVED "IgnoreMessagesFromSavedStations"

class AppConfig {
public:
    uint32_t Frequency = 433920000;
    uint32_t MaxPagerForBatchOrDetection = 30;
    uint32_t SignalRepeats = 10;
    bool Debug = true;
    bool IgnoreMessagesFromSavedStations = false;

private:
    void readFromFile(FlipperFile* file) {
        file->ReadUInt32(KEY_FREQUENCY, &Frequency);
        file->ReadUInt32(KEY_MAX_PAGERS, &MaxPagerForBatchOrDetection);
        file->ReadUInt32(KEY_REPEATS, &SignalRepeats);
        file->ReadBool(KEY_IGNORE_SAVED, &IgnoreMessagesFromSavedStations);
        file->ReadBool(KEY_DEBUG, &Debug);
    }

    void writeToFile(FlipperFile* file) {
        file->WriteUInt32(KEY_FREQUENCY, &Frequency);
        file->WriteUInt32(KEY_MAX_PAGERS, &MaxPagerForBatchOrDetection);
        file->WriteUInt32(KEY_REPEATS, &SignalRepeats);
        file->WriteBool(KEY_IGNORE_SAVED, &IgnoreMessagesFromSavedStations);
        file->ReadBool(KEY_DEBUG, &Debug);
    }

public:
    void Load() {
        FileManager filemanager = FileManager();
        FlipperFile* configFile = filemanager.OpenRead(CONFIG_FILE_PATH);
        if(configFile != NULL) {
            readFromFile(configFile);
            delete configFile;
        }
    }

    void Save() {
        FileManager filemanager = FileManager();
        FlipperFile* configFile = filemanager.OpenWrite(CONFIG_FILE_PATH);
        if(configFile != NULL) {
            writeToFile(configFile);
            delete configFile;
        }
    }
};
