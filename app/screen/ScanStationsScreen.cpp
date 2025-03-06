#ifndef _SCAN_STATIONS_SCREEN_CLASS_
#define _SCAN_STATIONS_SCREEN_CLASS_

#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/SubMenuUiView.cpp"

#include "lib/hardware/subghz/SubGhzModule.cpp"

#include "app/AppNotifications.cpp"
#include "app/pager/PagerReceiver.cpp"

class ScanStationsScreen {
private:
    SubMenuUiView* menuView;
    SubGhzModule* subghz;
    PagerReceiver* pagerReceiver;

public:
    ScanStationsScreen() {
        pagerReceiver = new PagerReceiver();

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
        PagerData* pagerData = pagerReceiver->Receive(data);

        if(pagerData == NULL) {
            return;
        }

        if(pagerData->IsNew()) {
            Notification::Play(&NOTIFICATION_SUBGHZ_RECEIVE);

            menuView->SetHeader(NULL);
            menuView->AddItem(pagerData->GetItemName(), HANDLER_1ARG(&ScanStationsScreen::doNothing));
        } else {
            menuView->SetItemLabel(pagerData->GetIndex(), pagerData->GetItemName());
        }
    }

    void doNothing(uint32_t) {
    }

    void destroy() {
        delete subghz;
        delete pagerReceiver;
    }
};

#endif //_SCAN_STATIONS_SCREEN_CLASS_
