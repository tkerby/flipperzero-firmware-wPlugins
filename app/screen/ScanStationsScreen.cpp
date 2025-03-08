#ifndef _SCAN_STATIONS_SCREEN_CLASS_
#define _SCAN_STATIONS_SCREEN_CLASS_

#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/SubMenuUiView.cpp"

#include "PagerOptionsScreen.cpp"

#include "lib/hardware/subghz/SubGhzModule.cpp"
#include "lib/hardware/subghz/data/SubGhzReceivedDataStub.cpp"

#include "app/AppNotifications.cpp"
#include "app/pager/PagerReceiver.cpp"

#define DEBUG true

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

#if DEBUG
        receive(new SubGhzReceivedDataStub("Princeton", 0xCBC082));
        receive(new SubGhzReceivedDataStub("SMC5326", 0x71a4d0));
        receive(new SubGhzReceivedDataStub("Princeton", 0x1914c0));
#endif
    }

    UiView* GetView() {
        return menuView;
    }

private:
    void receive(SubGhzReceivedData* data) {
        PagerData* pagerData = pagerReceiver->Receive(data);

        if(pagerData == NULL) {
            return;
        }

        if(pagerData->IsNew()) {
            Notification::Play(&NOTIFICATION_PAGER_RECEIVE);

            menuView->SetHeader(NULL);
            menuView->AddItem(pagerData->GetItemName(), HANDLER_1ARG(&ScanStationsScreen::showOptions));
        } else {
            menuView->SetItemLabel(pagerData->GetIndex(), pagerData->GetItemName());
        }
    }

    void showOptions(uint32_t index) {
        PagerOptionsScreen* screen = new PagerOptionsScreen(pagerReceiver, index);
        UiManager::GetInstance()->PushView(screen->GetView());
    }

    void destroy() {
        delete subghz;
        delete pagerReceiver;
    }
};

#endif //_SCAN_STATIONS_SCREEN_CLASS_
