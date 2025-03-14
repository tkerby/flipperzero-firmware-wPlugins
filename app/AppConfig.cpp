#ifndef _APP_CONFIG_CLASS_
#define _APP_CONFIG_CLASS_

#include <cstdint>

class AppConfig {
public:
    uint32_t Frequency = 433920000;
    int MaxPagerForBatchOrDetection = 30;
    uint32_t SignalRepeats = 10;
    bool Debug = false;
};

#endif //_APP_CONFIG_CLASS_
