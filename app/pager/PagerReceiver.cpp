#ifndef _PAGER_RECEIVER_CLASS_
#define _PAGER_RECEIVER_CLASS_

#include <vector>

#include "lib/hardware/subghz/SubGhzReceivedData.cpp"
#include "PagerData.cpp"

using namespace std;

class PagerReceiver {
private:
    vector<PagerDataStored> pagers;

public:
    PagerData* Receive(SubGhzReceivedData data) {
        // FuriString* itemName = furi_string_alloc_printf("%s %X", data.GetProtocolName(), (unsigned int)data.GetHash());
        // menuView->AddItem(furi_string_get_cstr(itemName), HANDLER_1ARG(&ScanStationsScreen::doNothing));
        // furi_string_free(itemName);
    }
};

#endif //_PAGER_RECEIVER_CLASS_
