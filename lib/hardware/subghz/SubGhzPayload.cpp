#ifndef _SUBGHZ_PAYLOAD_CLASS_
#define _SUBGHZ_PAYLOAD_CLASS_

#include <algorithm>
#include "flipper_format.h"

using namespace std;

class SubGhzPayload {
private:
    const char* protocol;
    FlipperFormat* flipperFormat;

public:
    SubGhzPayload(const char* protocol) {
        this->protocol = protocol;

        flipperFormat = flipper_format_string_alloc();
        flipper_format_write_string_cstr(flipperFormat, "Protocol", protocol);
    }

    void SetKey(uint64_t key) {
        char* dataBytes = (char*)&key;
        reverse(dataBytes, dataBytes + sizeof(key));
        flipper_format_write_hex(flipperFormat, "Key", (const uint8_t*)dataBytes, sizeof(key));
    }

    void SetBits(uint32_t bits) {
        flipper_format_write_uint32(flipperFormat, "Bit", &bits, 1);
    }

    void SetTE(uint32_t te) {
        flipper_format_write_uint32(flipperFormat, "TE", &te, 1);
    }

    void SetRepeat(uint32_t repeats) {
        flipper_format_write_uint32(flipperFormat, "Repeat", &repeats, 1);
    }

    FlipperFormat* GetFlipperFormat() {
        // flipper_format_rewind(flipperFormat);
        return flipperFormat;
    }

    const char* GetProtocol() {
        return protocol;
    }

    ~SubGhzPayload() {
        if(flipperFormat != NULL) {
            flipper_format_free(flipperFormat);
            flipperFormat = NULL;
        }
    }
};

#endif //_SUBGHZ_PAYLOAD_CLASS_
