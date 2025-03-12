#ifndef _PRINCETON_PROTCOL_CLASS_
#define _PRINCETON_PROTCOL_CLASS_

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
};

#endif //_PRINCETON_PROTCOL_CLASS_
