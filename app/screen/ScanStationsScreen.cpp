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
    vector<String*> elementNames;

public:
    ScanStationsScreen() {
        pagerReceiver = new PagerReceiver();

        menuView = new SubMenuUiView("Scanning for signals...");
        menuView->SetOnDestroyHandler(HANDLER(&ScanStationsScreen::destroy));
        menuView->SetOnReturnToViewHandler(HANDLER(&ScanStationsScreen::onReturn));

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
        ReceivedPagerData* pagerData = pagerReceiver->Receive(data);

        if(pagerData == NULL) {
            return;
        }

        if(pagerData->IsNew()) {
            Notification::Play(&NOTIFICATION_PAGER_RECEIVE);

            menuView->SetHeader(NULL);

            String* elementName = new String();
            elementNames.push_back(elementName);
            menuView->AddItem(getItemName(pagerData->GetData(), elementName), HANDLER_1ARG(&ScanStationsScreen::showOptions));
        } else {
            refreshName(pagerData->GetIndex());
        }

        delete pagerData;
    }

    void refreshName(uint32_t index) {
        menuView->SetItemLabel(index, getItemName(pagerReceiver->GetPagerData(index), elementNames[index]));
    }

    const char* getItemName(PagerDataStored* pagerData, String* string) {
        PagerDecoder* decoder = pagerReceiver->decoders[pagerData->decoder];
        PagerProtocol* protocol = pagerReceiver->protocols[pagerData->protocol];
        PagerAction action = decoder->GetAction(pagerData->data);
        return string->format(
            "%sx%d %s%06X %d/%d %s:%d",
            pagerData->edited ? "*" : "",
            pagerData->repeats,
            protocol->GetShortName(),
            (unsigned int)pagerData->data,
            decoder->GetStation(pagerData->data),
            decoder->GetPager(pagerData->data),
            action == UNKNOWN ? "A" : PagerActions::GetDescription(action),
            decoder->GetActionValue(pagerData->data));
    }

    void showOptions(uint32_t index) {
        PagerOptionsScreen* screen = new PagerOptionsScreen(pagerReceiver, index);
        UiManager::GetInstance()->PushView(screen->GetView());
    }

    void onReturn() {
        refreshName(menuView->GetCurrentIndex());
    }

    void destroy() {
        delete subghz;
        delete pagerReceiver;

        for(size_t i = 0; i < elementNames.size(); i++) {
            delete elementNames[i];
        }
    }
};

#endif //_SCAN_STATIONS_SCREEN_CLASS_
