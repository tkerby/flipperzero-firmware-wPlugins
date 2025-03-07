#ifndef _PAGER_DATA_CLASS_
#define _PAGER_DATA_CLASS_

#include "core/string.h"
#include <cstdint>
#include "PagerDataStored.cpp"
#include "protocol/PagerProtocol.cpp"

#include "decoder/Td157Decoder.cpp"
#include "decoder/Td165Decoder.cpp"
#include "decoder/Td174Decoder.cpp"

static Td174Decoder pagerDecoder;

class PagerData {
private:
    uint32_t index;
    PagerDataStored storedData;
    PagerProtocol* protocol;

public:
    PagerData(PagerProtocol* protocol, PagerDataStored storedData) : PagerData(protocol, storedData, UINT32_MAX) {
    }

    PagerData(PagerProtocol* protocol, PagerDataStored storedData, uint32_t index) {
        this->protocol = protocol;
        this->storedData = storedData;
        this->index = index;
    }

    bool IsNew() {
        return index == UINT32_MAX;
    }

    uint32_t GetIndex() {
        return index;
    }

    const char* GetItemName() {
        return furi_string_get_cstr(furi_string_alloc_printf(
            "x%d %s%06X S:%d P:%d",
            storedData.repeats,
            protocol->GetShortName(),
            (unsigned int)storedData.data,
            pagerDecoder.GetStation(storedData.data),
            pagerDecoder.GetPager(storedData.data)));
    }
};

#endif //_PAGER_DATA_CLASS_
