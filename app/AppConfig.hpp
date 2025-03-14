#pragma once

#include <cstdint>

class AppConfig {
public:
    uint32_t Frequency = 433920000;
    int MaxPagerForBatchOrDetection = 30;
    uint32_t SignalRepeats = 10;
    bool Debug = true;
    bool IgnoreMessagesFromSavedStations = false;
};
