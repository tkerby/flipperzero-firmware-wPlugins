#ifndef _RECEIVED_PAGER_DATA_CLASS_
#define _RECEIVED_PAGER_DATA_CLASS_

#include <cstdint>
#include "PagerDataStored.cpp"

class ReceivedPagerData {
private:
    PagerDataStored* storedData;
    uint32_t index;
    bool isNew;

public:
    ReceivedPagerData(PagerDataStored* storedData, uint32_t index, bool isNew) {
        this->storedData = storedData;
        this->index = index;
        this->isNew = isNew;
    }

    bool IsNew() {
        return isNew;
    }

    uint32_t GetIndex() {
        return index;
    }

    PagerDataStored* GetData() {
        return storedData;
    }
};

#endif //_RECEIVED_PAGER_DATA_CLASS_
