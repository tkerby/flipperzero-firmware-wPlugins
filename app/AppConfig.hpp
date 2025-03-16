#pragma once

#include <cstdint>

#include "app/AppFileSystem.hpp"
#include "lib/file/FileManager.hpp"
#include "lib/hardware/subghz/SubGhzModule.hpp"

#define KEY_CONFIG_FREQUENCY    "Frequency"
#define KEY_CONFIG_MAX_PAGERS   "MaxPagerForBatchOrDetection"
#define KEY_CONFIG_REPEATS      "SignalRepeats"
#define KEY_CONFIG_DEBUG        "Debug"
#define KEY_CONFIG_IGNORE_SAVED "IgnoreMessagesFromSavedStations"
#define KEY_CONFIG_AUTOSAVE     "Autosave"

class AppConfig {
public:
    uint32_t Frequency = 433920000;
    uint32_t MaxPagerForBatchOrDetection = 30;
    uint32_t SignalRepeats = 10;
    bool Debug = false;
    bool IgnoreMessagesFromSavedStations = false;
    bool AutosaveFoundSignals = true;

private:
    void readFromFile(FlipperFile* file) {
        file->ReadUInt32(KEY_CONFIG_FREQUENCY, &Frequency);
        file->ReadUInt32(KEY_CONFIG_MAX_PAGERS, &MaxPagerForBatchOrDetection);
        file->ReadUInt32(KEY_CONFIG_REPEATS, &SignalRepeats);
        file->ReadBool(KEY_CONFIG_IGNORE_SAVED, &IgnoreMessagesFromSavedStations);
        file->ReadBool(KEY_CONFIG_DEBUG, &Debug);
        file->ReadBool(KEY_CONFIG_AUTOSAVE, &AutosaveFoundSignals);
    }

    void writeToFile(FlipperFile* file) {
        file->WriteUInt32(KEY_CONFIG_FREQUENCY, Frequency);
        file->WriteUInt32(KEY_CONFIG_MAX_PAGERS, MaxPagerForBatchOrDetection);
        file->WriteUInt32(KEY_CONFIG_REPEATS, SignalRepeats);
        file->WriteBool(KEY_CONFIG_IGNORE_SAVED, IgnoreMessagesFromSavedStations);
        file->WriteBool(KEY_CONFIG_DEBUG, Debug);
        file->WriteBool(KEY_CONFIG_AUTOSAVE, AutosaveFoundSignals);
    }

public:
    void Load() {
        FlipperFile* configFile = FileManager().OpenRead(CONFIG_FILE_PATH);
        if(configFile != NULL) {
            readFromFile(configFile);
            delete configFile;
        }
    }

    void Save() {
        FlipperFile* configFile = FileManager().OpenWrite(CONFIG_FILE_PATH);
        if(configFile != NULL) {
            writeToFile(configFile);
            delete configFile;
        }
    }
};
