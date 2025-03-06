#ifndef _PAGER_DECODER_CLASS_
#define _PAGER_DECODER_CLASS_

#include <cstdint>

class PagerDecoder {
public:
    uint8_t id;
    virtual const char* GetFullName() = 0;
    virtual const char* GetShortName() = 0;
    virtual uint16_t GetStation(uint32_t data);
    virtual uint16_t GetPager(uint32_t data);
    virtual uint32_t SetPager(uint32_t data, uint16_t pagerNum);
};

#endif //_PAGER_DECODER_CLASS_
