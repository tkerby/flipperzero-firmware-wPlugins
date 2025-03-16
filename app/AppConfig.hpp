#pragma once

#include <cstdint>

#include "app/AppFileSystem.hpp"
#include "lib/file/FileManager.hpp"
#include "app/pager/SavedStationStrategy.hpp"

#define KEY_CONFIG_FREQUENCY      "Frequency"
#define KEY_CONFIG_MAX_PAGERS     "MaxPagerForBatchOrDetection"
#define KEY_CONFIG_REPEATS        "SignalRepeats"
#define KEY_CONFIG_DEBUG          "Debug"
#define KEY_CONFIG_SAVED_STRATEGY "SavedStationStrategy"
#define KEY_CONFIG_AUTOSAVE       "AutosaveFoundSignals"

class AppConfig {
public:
    uint32_t Frequency = 433920000;
    uint32_t MaxPagerForBatchOrDetection = 30;
    uint32_t SignalRepeats = 10;
    SavedStationStrategy SavedStrategy = SHOW_NAME;
    bool AutosaveFoundSignals = true;
    bool Debug = false;

private:
    void readFromFile(FlipperFile* file) {
        uint32_t savedStrategyValue = SavedStrategy;

        file->ReadUInt32(KEY_CONFIG_FREQUENCY, &Frequency);
        file->ReadUInt32(KEY_CONFIG_MAX_PAGERS, &MaxPagerForBatchOrDetection);
        file->ReadUInt32(KEY_CONFIG_REPEATS, &SignalRepeats);
        file->ReadUInt32(KEY_CONFIG_SAVED_STRATEGY, &savedStrategyValue);
        file->ReadBool(KEY_CONFIG_DEBUG, &Debug);
        file->ReadBool(KEY_CONFIG_AUTOSAVE, &AutosaveFoundSignals);

        SavedStrategy = static_cast<enum SavedStationStrategy>(savedStrategyValue);
    }

    void writeToFile(FlipperFile* file) {
        file->WriteUInt32(KEY_CONFIG_FREQUENCY, Frequency);
        file->WriteUInt32(KEY_CONFIG_MAX_PAGERS, MaxPagerForBatchOrDetection);
        file->WriteUInt32(KEY_CONFIG_REPEATS, SignalRepeats);
        file->WriteUInt32(KEY_CONFIG_SAVED_STRATEGY, SavedStrategy);
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
