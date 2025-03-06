#ifndef _PAGER_PROTCOL_CLASS_
#define _PAGER_PROTCOL_CLASS_

#include <cstdint>

class PagerProtocol {
public:
    uint8_t id;
    virtual const char* GetSystemName() = 0;
    virtual const char* GetDisplayName() = 0;
    virtual const char* GetShortName() = 0;
};

#endif //_PAGER_PROTCOL_CLASS_
