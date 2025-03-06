#ifndef _PAGER_DECODER_CLASS_
#define _PAGER_DECODER_CLASS_

#include <cstdint>

class PagerDecoder {
public:
    uint8_t id;
    virtual const char* GetSystemName() = 0;
    virtual const char* GetDisplayName() = 0;
    virtual const char* GetShortName() = 0;
};

#endif //_PAGER_DECODER_CLASS_
