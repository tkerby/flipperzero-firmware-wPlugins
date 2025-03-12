#ifndef _PRINCETON_PROTCOL_CLASS_
#define _PRINCETON_PROTCOL_CLASS_

#include <stream/stream.h>

#include "PagerProtocol.cpp"

class PrincetonProtocol : public PagerProtocol {
public:
    const char* GetSystemName() {
        return "Princeton";
    }

    const char* GetDisplayName() {
        return GetSystemName();
    }

    const char* GetShortName() {
        return "p";
    }

    int GetFallbackTE() {
        return 212;
    }

    int GetMaxTE() {
        return 1200;
    }

    SubGhzPayload* CreatePayload(uint64_t data, uint32_t te, uint32_t repeats) {
        SubGhzPayload* payload = new SubGhzPayload(GetSystemName());
        payload->SetBits(24);
        payload->SetKey(data);
        payload->SetTE(te);
        payload->SetRepeat(repeats);
        return payload;
    }
};

#endif //_PRINCETON_PROTCOL_CLASS_
