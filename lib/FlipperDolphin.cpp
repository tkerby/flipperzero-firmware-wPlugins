#ifndef _FLIPPER_DOLPHIN_CLASS_
#define _FLIPPER_DOLPHIN_CLASS_

#include <dolphin/dolphin.h>

class FlipperDolphin {
private:
public:
    static void Deed(DolphinDeed deed) {
        dolphin_deed(deed);
    }
};

#endif //_FLIPPER_DOLPHIN_CLASS_
