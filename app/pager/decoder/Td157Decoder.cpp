#ifndef _TD157_DECODER_CLASS_
#define _TD157_DECODER_CLASS_

#include "PagerDecoder.cpp"

#define TD157_ACTION_RING         0b0010
#define TD157_ACTION_TURN_OFF_ALL 0b1111

class Td157Decoder : public PagerDecoder {
private:
    const uint32_t stationMask = 0b111111111100000000000000; // leading 10 bits (of 24) are station
    const uint32_t pagerMask = 0b11111111110000; // next 10 bits are pager
    const uint32_t actionMask = 0b1111; // and the last 4 bits is action
    const uint8_t stationOffset = 14;
    const uint8_t pagerOffset = 4;

public:
    const char* GetFullName() {
        return "Retekess TD157";
    }

    const char* GetShortName() {
        return "TD157";
    }

    uint16_t GetStation(uint32_t data) {
        uint32_t station = (data & stationMask) >> stationOffset;
        return (uint16_t)station;
    }

    uint16_t GetPager(uint32_t data) {
        uint32_t pager = (data & pagerMask) >> pagerOffset;
        return (uint16_t)pager;
    }

    uint32_t SetPager(uint32_t data, uint16_t pagerNum) {
        uint32_t pagerClearedData = data & ~pagerMask;
        return pagerClearedData | (pagerNum << pagerOffset);
    }

    PagerAction GetAction(uint32_t data) {
        uint8_t action = data & actionMask;
        switch(action) {
        case TD157_ACTION_RING:
            return RING;
        case TD157_ACTION_TURN_OFF_ALL:
            return TURN_OFF_ALL;
        default:
            return UNKNOWN;
        }
    }

    uint32_t SetAction(uint32_t data, PagerAction action) {
        switch(action) {
        case RING:
            return (data & ~actionMask) | TD157_ACTION_RING;
        case TURN_OFF_ALL:
            return data | TD157_ACTION_TURN_OFF_ALL;
        default:
            return data;
        }
    }
};

#endif //_TD157_DECODER_CLASS_
