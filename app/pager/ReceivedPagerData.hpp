#pragma once

#include <cstdint>
#include "PagerDataStored.hpp"

class ReceivedPagerData {
private:
    PagerDataGetter getStoredData;
    uint32_t index;
    bool isNew;

public:
    ReceivedPagerData(PagerDataGetter storedDataGetter, uint32_t index, bool isNew) {
        this->getStoredData = storedDataGetter;
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
        return getStoredData();
    }
};
