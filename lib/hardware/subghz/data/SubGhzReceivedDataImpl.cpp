#ifndef _SUBGHZ_RECEIVED_DATA_IMPL_CLASS_
#define _SUBGHZ_RECEIVED_DATA_IMPL_CLASS_

#include <lib/subghz/protocols/base.h>

#include "SubGhzReceivedData.cpp"

class SubGhzReceivedDataImpl : public SubGhzReceivedData {
private:
    SubGhzProtocolDecoderBase* decoder;

public:
    SubGhzReceivedDataImpl(SubGhzProtocolDecoderBase* decoder) {
        this->decoder = decoder;
    }

    const char* GetProtocolName() {
        return decoder->protocol->name;
    }

    uint32_t GetHash() {
        return decoder->protocol->decoder->get_hash_data_long(decoder);
    }
};

#endif //_SUBGHZ_RECEIVED_DATA_IMPL_CLASS_
