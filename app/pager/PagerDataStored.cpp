#ifndef _PAGER_DATA_STORED_CLASS_
#define _PAGER_DATA_STORED_CLASS_

#include <cstdint>

struct PagerDataStored {
    uint32_t data    : 25; // princeton is 24 bit, smc5326 is 25 bit (we take the largest)
    uint8_t protocol : 7;
};

#endif //_PAGER_DATA_STORED_CLASS_
