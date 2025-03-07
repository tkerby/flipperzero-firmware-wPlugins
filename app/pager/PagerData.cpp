#ifndef _PAGER_DATA_CLASS_
#define _PAGER_DATA_CLASS_

#include "core/string.h"
#include <cstdint>
#include "PagerDataStored.cpp"
#include "protocol/PagerProtocol.cpp"
#include "decoder/PagerDecoder.cpp"

class PagerData {
private:
    uint32_t index;
    PagerDataStored* storedData;
    PagerProtocol* protocol;
    PagerDecoder* decoder;

public:
    PagerData(PagerDataStored* storedData, PagerProtocol* protocol, PagerDecoder* decoder) :
        PagerData(storedData, protocol, decoder, UINT32_MAX) {
    }

    PagerData(PagerDataStored* storedData, PagerProtocol* protocol, PagerDecoder* decoder, uint32_t index) {
        this->storedData = storedData;
        this->protocol = protocol;
        this->decoder = decoder;
        this->index = index;
    }

    bool IsNew() {
        return index == UINT32_MAX;
    }

    uint32_t GetIndex() {
        return index;
    }

    const char* GetItemName() {
        PagerAction action = decoder->GetAction(storedData->data);
        return furi_string_get_cstr(furi_string_alloc_printf(
            "x%d %s%06X %d/%d %s:%d",
            storedData->repeats,
            protocol->GetShortName(),
            (unsigned int)storedData->data,
            decoder->GetStation(storedData->data),
            decoder->GetPager(storedData->data),
            action == UNKNOWN ? "A" : PagerActions::GetDescription(action),
            decoder->GetActionValue(storedData->data)));
    }
};

#endif //_PAGER_DATA_CLASS_
