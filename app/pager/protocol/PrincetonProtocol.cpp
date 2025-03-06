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
};

#endif //_PRINCETON_PROTCOL_CLASS_
