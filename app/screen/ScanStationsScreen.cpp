#ifndef _SCAN_STATIONS_SCREEN_CLASS_
#define _SCAN_STATIONS_SCREEN_CLASS_

#include "lib/ui/view/AdvancedColumnListUiView.cpp"

#include "PagerOptionsScreen.cpp"

#include "lib/hardware/subghz/SubGhzModule.cpp"
#include "lib/hardware/subghz/data/SubGhzReceivedDataStub.cpp"

#include "app/AppNotifications.cpp"
#include "app/pager/PagerReceiver.cpp"

#define DEBUG true

static int8_t stationScreenColumnOffsets[]{
    3, // hex
    50, // station
    73, // pager
    94, // action
    128 - 8 // repeats / edit flag
};
static Font stationScreenColumnFonts[]{
    FontBatteryPercent, // hex
    FontSecondary, // station
    FontSecondary, // pager
    FontBatteryPercent, // action
    FontBatteryPercent, // repeats
};

static Align stationScreenColumnAlignments[]{
    AlignLeft, // hex
    AlignCenter, // station
    AlignCenter, // pager
    AlignCenter, // action
    AlignRight, // repeats
};

class ScanStationsScreen {
private:
    // SubMenuUiView* menuView;
    AdvancedColumnListUiView* menuView;
    PagerReceiver* pagerReceiver;
    SubGhzModule* subghz;

public:
    ScanStationsScreen() {
        pagerReceiver = new PagerReceiver();

        menuView = new AdvancedColumnListUiView(
            stationScreenColumnOffsets,
            sizeof(stationScreenColumnOffsets),
            HANDLER_3ARG(&ScanStationsScreen::getElementColumnName)
        );
        menuView->SetOnDestroyHandler(HANDLER(&ScanStationsScreen::destroy));
        menuView->SetOnReturnToViewHandler(HANDLER(&ScanStationsScreen::onReturn));

        menuView->SetNoElementCaption("Receiving...");
        menuView->SetColumnFonts(stationScreenColumnFonts);
        menuView->SetColumnAlignments(stationScreenColumnAlignments);

        menuView->SetLeftButton("Config");

        // menuView = new SubMenuUiView("Scanning for signals...");
        // menuView->SetOnDestroyHandler(HANDLER(&ScanStationsScreen::destroy));
        // menuView->SetOnReturnToViewHandler(HANDLER(&ScanStationsScreen::onReturn));

        subghz = new SubGhzModule();
        subghz->SetReceiveHandler(HANDLER_1ARG(&ScanStationsScreen::receive));
        subghz->ReceiveAsync();

#if DEBUG
        receive(new SubGhzReceivedDataStub("Princeton", 0xCBC082));
        receive(new SubGhzReceivedDataStub("SMC5326", 0x71a4d0));
        receive(new SubGhzReceivedDataStub("Princeton", 0x1914c0));

        receive(new SubGhzReceivedDataStub("Princeton", 0x4b00C2));
        receive(new SubGhzReceivedDataStub("Princeton", 0x200194));

        // turn off all
        receive(new SubGhzReceivedDataStub("Princeton", 0x1F3E7F));

        for(int i = 0; i < 99; i++) {
            receive(new SubGhzReceivedDataStub("Princeton", 0x20019F));
        }
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

            if(pagerData->GetIndex() == 0) {
                menuView->SetCenterButton("View");
                menuView->SetRightButton("Action");
            }

            menuView->AddElement();
            // menuView->AddItem(getItemName(pagerData->GetData(), elementName), HANDLER_1ARG(&ScanStationsScreen::showOptions));
        }

        menuView->Refresh();
        delete pagerData;
    }

    void getElementColumnName(int index, int column, String* str) {
        PagerDataStored* pagerData = pagerReceiver->GetPagerData(index);
        PagerDecoder* decoder = pagerReceiver->decoders[pagerData->decoder];

        switch(column) {
        case 0: // hex
            str->format("%06X", pagerData->data);
            break;

        case 1: // station
            str->format("%d", decoder->GetStation(pagerData->data));
            break;

        case 2: // pager
            str->format("%d", decoder->GetPager(pagerData->data));
            break;

        case 3: // action
        {
            PagerAction action = decoder->GetAction(pagerData->data);
            if(action == UNKNOWN) {
                str->format("%d", decoder->GetActionValue(pagerData->data));
            } else {
                str->format("%.4s", PagerActions::GetDescription(action));
            }
        }; break;

        case 4: // repeats or edit flag
            if(!pagerData->edited) {
                str->format("x%d", pagerData->repeats);
            } else {
                str->format("*");
            }
            break;

        default:
            break;
        }
    }

    void refreshName(uint32_t index) {
        UNUSED(index);
        // menuView->SetItemLabel(index, getItemName(pagerReceiver->GetPagerData(index), elementNames[index]));
    }

    const char* getItemName(PagerDataStored* pagerData, String* string) {
        PagerDecoder* decoder = pagerReceiver->decoders[pagerData->decoder];
        // PagerProtocol* protocol = pagerReceiver->protocols[pagerData->protocol];
        PagerAction action = decoder->GetAction(pagerData->data);

        return string->format(
            "%s%06X - %d/%d - %s:%d",
            pagerData->edited ? "*" : "",
            (unsigned int)pagerData->data,
            decoder->GetStation(pagerData->data),
            decoder->GetPager(pagerData->data),
            action == UNKNOWN ? "A" : PagerActions::GetDescription(action),
            decoder->GetActionValue(pagerData->data)
        );
    }

    void showOptions(uint32_t index) {
        PagerOptionsScreen* screen = new PagerOptionsScreen(pagerReceiver, index);
        UiManager::GetInstance()->PushView(screen->GetView());
    }

    void onReturn() {
        // refreshName(menuView->GetCurrentIndex());
        menuView->Refresh();
    }

    void destroy() {
        delete subghz;
        delete pagerReceiver;
    }
};

#endif //_SCAN_STATIONS_SCREEN_CLASS_
