#ifndef _TD165_DECODER_CLASS_
#define _TD165_DECODER_CLASS_

#include "PagerDecoder.cpp"

class Td165Decoder : public PagerDecoder {
public:
    const char* GetFullName() {
        return "Retekess TD165";
    }

    const char* GetShortName() {
        return "TD165";
    }

    uint16_t GetStation(uint32_t data);
    uint16_t GetPager(uint32_t data);
    uint32_t SetPager(uint32_t data, uint16_t pagerNum);
};

#endif //_TD165_DECODER_CLASS_
