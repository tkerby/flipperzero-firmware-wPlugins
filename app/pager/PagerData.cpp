#ifndef _PAGER_DATA_CLASS_
#define _PAGER_DATA_CLASS_

#include <cstdint>
#include "PagerDataStored.cpp"

class PagerData {
private:
    uint32_t index = UINT32_MAX;
    PagerDataStored storedData;

public:
    bool IsNew() {
        return index != UINT32_MAX;
    }

    uint32_t GetIndex() {
        return index;
    }

    const char* GetItemName() {
    }
};

#endif //_PAGER_DATA_CLASS_
