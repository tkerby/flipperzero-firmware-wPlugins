#ifndef _SCAN_STATIONS_SCREEN_CLASS_
#define _SCAN_STATIONS_SCREEN_CLASS_

#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/SubMenuUiView.cpp"

#include "lib/hardware/subghz/SubGhzModule.cpp"

#include "app/AppNotifications.cpp"

class ScanStationsScreen {
private:
    SubMenuUiView* menuView;
    SubGhzModule* subghz;

public:
    ScanStationsScreen() {
        menuView = new SubMenuUiView("Scanning for signals...");
        menuView->SetOnDestroyHandler(HANDLER(&ScanStationsScreen::destroy));

        subghz = new SubGhzModule();
        subghz->SetReceiveHandler(HANDLER_1ARG(&ScanStationsScreen::receive));
        subghz->ReceiveAsync();
    }

    UiView* GetView() {
        return menuView;
    }

private:
    void receive(SubGhzReceivedData data) {
        menuView->SetHeader(NULL);

        Notification::Play(&NOTIFICATION_SUBGHZ_RECEIVE);

        FuriString* itemName = furi_string_alloc_printf("%s %X", data.GetProtocolName(), (unsigned int)data.GetKey());
        menuView->AddItem(furi_string_get_cstr(itemName), HANDLER_1ARG(&ScanStationsScreen::doNothing));
        furi_string_free(itemName);
    }

    void doNothing(uint32_t) {
    }

    void destroy() {
        delete subghz;
    }
};

#endif //_SCAN_STATIONS_SCREEN_CLASS_
