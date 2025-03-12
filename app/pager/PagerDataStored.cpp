#ifndef _PAGER_DATA_STORED_CLASS_
#define _PAGER_DATA_STORED_CLASS_

#include <cstdint>

struct PagerDataStored {
    // first 4-bytes
    uint32_t data    : 25;
    uint8_t repeats  : 7;

    // second 4-byte
    int16_t te       : 16;
    int8_t decoder   : 8;
    uint8_t protocol : 7;
    bool edited      : 1 = false;
};

#endif //_PAGER_DATA_STORED_CLASS_
