#ifndef _SUBGHZ_RECEIVED_DATA_CLASS_
#define _SUBGHZ_RECEIVED_DATA_CLASS_

#include <cstdint>

class SubGhzReceivedData {
public:
    virtual const char* GetProtocolName() = 0;
    virtual uint32_t GetHash() = 0;
    virtual ~SubGhzReceivedData() {};
    virtual int GetTE() = 0;
};

#endif //_SUBGHZ_RECEIVED_DATA_CLASS_
