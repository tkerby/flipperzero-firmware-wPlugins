#pragma once

#include <cstdint>
#include <functional>

using namespace std;

struct PagerDataStored {
    // first 4-bytes
    uint32_t data    : 25;
    uint8_t repeats  : 7;

    // second 4-byte
    int16_t te       : 16;
    int8_t decoder   : 8;
    uint8_t protocol : 7;
    bool edited      : 1 = false;
};

// PagerDataStored is short-living because it's stored as vector of stack allocated objects in PagerReceiver class.
// If vector size changes, it reallocates all the objects on the new addresses in memory. We could store pointers in vector instead of stack objects,
// but it would take more memory which is not acceptable (sizeof(PagerDataStored) + sizeof(PagerDataStored*)) vs sizeof(PagerDataStored).
// That's why we pass the getter instead of object itself, to make sure we always have the right pointer to the PagerDataStored structure.
typedef function<PagerDataStored*()> PagerDataGetter;
