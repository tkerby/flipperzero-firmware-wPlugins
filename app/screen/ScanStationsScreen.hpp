#ifndef _SCAN_STATIONS_SCREEN_CLASS_
#define _SCAN_STATIONS_SCREEN_CLASS_

#include "lib/hardware/subghz/data/SubGhzReceivedDataStub.hpp"
#include "lib/ui/view/ColumnOrientedListUiView.hpp"

#include "EditPagerScreen.hpp"
#include "PagerActionsScreen.hpp"

#include "lib/hardware/subghz/SubGhzModule.hpp"

#include "app/AppConfig.hpp"
#include "app/AppNotifications.hpp"
#include "app/pager/PagerReceiver.hpp"

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
    AppConfig* config;
    ColumnOrientedListUiView* menuView;
    PagerReceiver* pagerReceiver;
    SubGhzModule* subghz;

public:
    ScanStationsScreen(AppConfig* config) {
        this->config = config;
        pagerReceiver = new PagerReceiver(config);

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

        subghz = new SubGhzModule(config->Frequency);
        subghz->SetReceiveHandler(HANDLER_1ARG(&ScanStationsScreen::receive));
        subghz->SetReceiveAfterTransmission(true);
        subghz->ReceiveAsync();

        if(subghz->IsExternal()) {
            menuView->SetNoElementCaption("Receiving via EXT...");
        } else {
            menuView->SetNoElementCaption("Receiving...");
        }

        if(config->Debug) {
            receive(new SubGhzReceivedDataStub("Princeton", 0x030012)); // Europolis, pasta & pizza
            receive(new SubGhzReceivedDataStub("Princeton", 0xCBC012)); // Europolis, tokyo ramen
            receive(new SubGhzReceivedDataStub("Princeton", 0xA00012)); // Europolis, Istanbul
            receive(new SubGhzReceivedDataStub("Princeton", 0x134012)); // Metropolis, Vai me

            receive(new SubGhzReceivedDataStub("Princeton", 0x71A420)); // batoni?
            receive(new SubGhzReceivedDataStub("SMC5326", 0x200084)); // koreana
            receive(new SubGhzReceivedDataStub("Princeton", 0xBC022)); // koreana
        }
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

            if(menuView->IsOnTop()) {
                menuView->Refresh();
            }

            delete pagerData;
        }
        delete data;
    }

    void getElementColumnName(int index, int column, String* str) {
        PagerDataStored* pagerData = pagerReceiver->PagerGetter(index)();
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
        //     EditPagerScreen* screen = new EditPagerScreen(pagerReceiver, index);
        //     UiManager::GetInstance()->PushView(screen->GetView());
    }

    void editTransmission(uint32_t index) {
        EditPagerScreen* screen = new EditPagerScreen(config, subghz, pagerReceiver, pagerReceiver->PagerGetter(index));
        UiManager::GetInstance()->PushView(screen->GetView());
    }

    void showActions(uint32_t index) {
        PagerDataGetter getPager = pagerReceiver->PagerGetter(index);
        PagerDataStored* pagerData = getPager();
        PagerDecoder* decoder = pagerReceiver->decoders[pagerData->decoder];
        PagerProtocol* protocol = pagerReceiver->protocols[pagerData->protocol];

        PagerActionsScreen* screen = new PagerActionsScreen(config, getPager, decoder, protocol, subghz);
        UiManager::GetInstance()->PushView(screen->GetView());
    }

    void destroy() {
        delete subghz;
        delete pagerReceiver;
        delete this;
    }
};

#endif //_SCAN_STATIONS_SCREEN_CLASS_
