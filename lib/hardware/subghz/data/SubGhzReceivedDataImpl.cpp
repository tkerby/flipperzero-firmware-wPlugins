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
        if(decoder == NULL) {
            return "";
        }
        return decoder->protocol->name;
    }

    uint32_t GetHash() {
        if(decoder == NULL) {
            return 0x0;
        }
        FuriString* hashString = furi_string_alloc();
        decoder->protocol->decoder->get_string(decoder, hashString);
        unsigned int hash;
        sscanf(furi_string_get_cstr(hashString), "%x", &hash);
        furi_string_free(hashString);
        return hash;
        // return decoder->protocol->decoder->get_hash_data_long(decoder);
    }
};

#endif //_SUBGHZ_RECEIVED_DATA_IMPL_CLASS_
