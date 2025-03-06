#ifndef _PAGER_DATA_STORED_CLASS_
#define _PAGER_DATA_STORED_CLASS_

#include <cstdint>

struct PagerDataStored {
    uint32_t data;
    uint8_t protocol;
    uint8_t repeats;
};

#endif //_PAGER_DATA_STORED_CLASS_
