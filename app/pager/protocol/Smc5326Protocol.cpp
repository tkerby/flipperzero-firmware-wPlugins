#ifndef _SMC5326_PROTCOL_CLASS_
#define _SMC5326_PROTCOL_CLASS_

#include "PagerProtocol.cpp"

class Smc5326Protocol : public PagerProtocol {
    const char* GetSystemName() {
        return "SMC5326";
    }

    const char* GetDisplayName() {
        return GetSystemName();
    }

    const char* GetShortName() {
        return "s";
    }

    int GetFallbackTE() {
        return 326;
    }

    int GetMaxTE() {
        return 900;
    }

    SubGhzPayload* CreatePayload(uint64_t data, uint32_t te, uint32_t repeats) {
        SubGhzPayload* payload = new SubGhzPayload(GetSystemName());
        payload->SetBits(25);
        payload->SetKey(data);
        payload->SetTE(te);
        payload->SetRepeat(repeats);
        return payload;
    }
};

#endif //_SMC5326_PROTCOL_CLASS_
