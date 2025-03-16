#pragma once

#include <cstdint>
#include <vector>
#include "../PagerAction.hpp"

using namespace std;

class PagerDecoder {
public:
    uint8_t id;
    virtual const char* GetFullName() = 0;
    virtual const char* GetShortName() = 0;

    virtual uint16_t GetStation(uint32_t data) = 0;

    virtual uint16_t GetPager(uint32_t data) = 0;
    virtual uint32_t SetPager(uint32_t data, uint16_t pagerNum) = 0;

    virtual uint8_t GetActionValue(uint32_t data) = 0;
    virtual PagerAction GetAction(uint32_t data) = 0;
    virtual uint32_t SetAction(uint32_t data, PagerAction action) = 0;
    virtual uint32_t SetActionValue(uint32_t data, uint8_t action) = 0;
    virtual vector<PagerAction> GetSupportedActions() = 0;
    virtual uint8_t GetActionsCount() = 0;

    virtual ~PagerDecoder() {
    }

protected:
    uint32_t reverseBits(uint32_t number, int count) {
        uint32_t rev = 0;

        while(count-- > 0) {
            rev <<= 1;
            if((number & 1) == 1) {
                rev ^= 1;
            }
            number >>= 1;
        }

        return rev;
    }
};
