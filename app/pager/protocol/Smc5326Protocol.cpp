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
};

#endif //_SMC5326_PROTCOL_CLASS_
