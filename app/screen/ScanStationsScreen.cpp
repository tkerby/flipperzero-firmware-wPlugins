#ifndef _SCAN_STATIONS_SCREEN_CLASS_
#define _SCAN_STATIONS_SCREEN_CLASS_

#include "lib/hardware/subghz/data/SubGhzReceivedDataStub.cpp"
#include "lib/ui/view/ColumnOrientedListUiView.cpp"

#include "PagerOptionsScreen.cpp"
#include "PagerActionsScreen.cpp"

#include "lib/hardware/subghz/SubGhzModule.cpp"

#include "app/AppNotifications.cpp"
#include "app/pager/PagerReceiver.cpp"

#define DEBUG true

static int8_t stationScreenColumnOffsets[]{
    3, // hex
    49, // station
    72, // pager
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
    ColumnOrientedListUiView* menuView;
    PagerReceiver* pagerReceiver;
    SubGhzModule* subghz;

public:
    ScanStationsScreen() {
        pagerReceiver = new PagerReceiver();

        menuView = new ColumnOrientedListUiView(
            stationScreenColumnOffsets,
            sizeof(stationScreenColumnOffsets),
            HANDLER_3ARG(&ScanStationsScreen::getElementColumnName)
        );
        menuView->SetOnDestroyHandler(HANDLER(&ScanStationsScreen::destroy));
        menuView->SetOnReturnToViewHandler([this]() { this->menuView->Refresh(); });

        menuView->SetColumnFonts(stationScreenColumnFonts);
        menuView->SetColumnAlignments(stationScreenColumnAlignments);

        menuView->SetLeftButton("Conf", HANDLER_1ARG(&ScanStationsScreen::showConfig));

        subghz = new SubGhzModule();
        subghz->SetReceiveHandler(HANDLER_1ARG(&ScanStationsScreen::receive));
        subghz->ReceiveAsync();

        if(subghz->IsExternal()) {
            menuView->SetNoElementCaption("Receiving via EXT...");
        } else {
            menuView->SetNoElementCaption("Receiving...");
        }

#if DEBUG
        receive(new SubGhzReceivedDataStub("Princeton", 0xCBC082));
        receive(new SubGhzReceivedDataStub("SMC5326", 0x71a4d0));
        receive(new SubGhzReceivedDataStub("Princeton", 0x1914c0));

        receive(new SubGhzReceivedDataStub("Princeton", 0x4b00C2));
        receive(new SubGhzReceivedDataStub("Princeton", 0x200194));

        // turn off all
        receive(new SubGhzReceivedDataStub("Princeton", 0x1F3E7F));

        for(int i = 0; i < 99; i++) {
            receive(new SubGhzReceivedDataStub("Princeton", 0xABC082));
        }
#endif
    }

    UiView* GetView() {
        return menuView;
    }

private:
    void receive(SubGhzReceivedData* data) {
        ReceivedPagerData* pagerData = pagerReceiver->Receive(data);

        if(pagerData != NULL) {
            if(pagerData->IsNew()) {
                Notification::Play(&NOTIFICATION_PAGER_RECEIVE);

                if(pagerData->GetIndex() == 0) { // add buttons after capturing the first transmission
                    menuView->SetCenterButton("Actions", HANDLER_1ARG(&ScanStationsScreen::showActions));
                    menuView->SetRightButton("Edit", HANDLER_1ARG(&ScanStationsScreen::editTransmission));
                }

                menuView->AddElement();
            }

            menuView->Refresh();

            delete pagerData;
        }
        delete data;
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
                str->format("**");
            }
            break;

        default:
            break;
        }
    }

    void showConfig(uint32_t index) {
        UNUSED(index);
        //     PagerOptionsScreen* screen = new PagerOptionsScreen(pagerReceiver, index);
        //     UiManager::GetInstance()->PushView(screen->GetView());
    }

    void editTransmission(uint32_t index) {
        PagerOptionsScreen* screen = new PagerOptionsScreen(pagerReceiver, index);
        UiManager::GetInstance()->PushView(screen->GetView());
    }

    void showActions(uint32_t index) {
        PagerDataStored* pagerData = pagerReceiver->GetPagerData(index);
        PagerDecoder* decoder = pagerReceiver->decoders[pagerData->decoder];
        PagerProtocol* protocol = pagerReceiver->protocols[pagerData->protocol];

        PagerActionsScreen* screen = new PagerActionsScreen(pagerData, decoder, protocol, subghz);
        UiManager::GetInstance()->PushView(screen->GetView());
        // PagerOptionsScreen* screen = new PagerOptionsScreen(pagerReceiver, index);
        // UiManager::GetInstance()->PushView(screen->GetView());
    }

    void destroy() {
        delete subghz;
        delete pagerReceiver;
        delete this;
    }
};

#endif //_SCAN_STATIONS_SCREEN_CLASS_
