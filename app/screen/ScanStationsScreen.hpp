#pragma once

#include "SettingsScreen.hpp"
#include "lib/hardware/subghz/data/SubGhzReceivedDataStub.hpp"
#include "lib/ui/view/ColumnOrientedListUiView.hpp"

#include "EditPagerScreen.hpp"
#include "PagerActionsScreen.hpp"

#include "lib/hardware/subghz/SubGhzModule.hpp"

#include "app/AppConfig.hpp"
#include "app/AppNotifications.hpp"
#include "app/pager/PagerReceiver.hpp"

static int8_t stationScreenColumnOffsets[]{
    3, // station name (if known)
    3, // hex
    49, // station
    72, // pager
    94, // action
    128 - 8 // repeats / edit flag
};
static Font stationScreenColumnFonts[]{
    FontSecondary, // station name (if known)
    FontBatteryPercent, // hex
    FontSecondary, // station
    FontSecondary, // pager
    FontBatteryPercent, // action
    FontBatteryPercent, // repeats
};

static Align stationScreenColumnAlignments[]{
    AlignLeft, // station name (if known)
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

        menuView = new ColumnOrientedListUiView(
            stationScreenColumnOffsets,
            sizeof(stationScreenColumnOffsets),
            HANDLER_3ARG(&ScanStationsScreen::getElementColumnName)
        );
        menuView->SetOnDestroyHandler(HANDLER(&ScanStationsScreen::destroy));
        menuView->SetOnReturnToViewHandler([this]() { this->menuView->Refresh(); });
        menuView->SetGoBackHandler(HANDLER(&ScanStationsScreen::goBack));

        menuView->SetColumnFonts(stationScreenColumnFonts);
        menuView->SetColumnAlignments(stationScreenColumnAlignments);

        menuView->SetLeftButton("Conf", HANDLER_1ARG(&ScanStationsScreen::showConfig));

        subghz = new SubGhzModule(config->Frequency);
        subghz->SetReceiveHandler(HANDLER_1ARG(&ScanStationsScreen::receive));
        subghz->SetReceiveAfterTransmission(true);
        subghz->ReceiveAsync();

        pagerReceiver = new PagerReceiver(config, subghz->GetSettings());

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

            receive(new SubGhzReceivedDataStub("Princeton", 0xCBC022)); // tokyo ramen another pager
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
                    menuView->SetRightButton("Edit", HANDLER_1ARG(&ScanStationsScreen::editPagerMessage));
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
        StoredPagerData* pagerData = pagerReceiver->PagerGetter(index)();
        PagerDecoder* decoder = pagerReceiver->decoders[pagerData->decoder];
        String* name = pagerReceiver->GetName(pagerData);

        switch(column) {
        case 0: // station name
            if(name != NULL) {
                str->format("%s", name->cstr());
            }
            break;

        case 1: // hex
            if(name == NULL) {
                str->format("%06X", pagerData->data);
            }
            break;

        case 2: // station
            if(name == NULL) {
                str->format("%d", decoder->GetStation(pagerData->data));
            }
            break;

        case 3: // pager
            str->format("%d", decoder->GetPager(pagerData->data));
            break;

        case 4: // action
        {
            PagerAction action = decoder->GetAction(pagerData->data);
            if(action == UNKNOWN) {
                str->format("%d", decoder->GetActionValue(pagerData->data));
            } else {
                str->format("%.4s", PagerActions::GetDescription(action));
            }
        }; break;

        case 5: // repeats or edit flag
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

    void showConfig(uint32_t) {
        SettingsScreen* screen = new SettingsScreen(config, pagerReceiver, subghz);
        UiManager::GetInstance()->PushView(screen->GetView());
    }

    void editPagerMessage(uint32_t index) {
        EditPagerScreen* screen = new EditPagerScreen(config, subghz, pagerReceiver, pagerReceiver->PagerGetter(index), NULL);
        UiManager::GetInstance()->PushView(screen->GetView());
    }

    void showActions(uint32_t index) {
        PagerDataGetter getPager = pagerReceiver->PagerGetter(index);
        StoredPagerData* pagerData = getPager();
        PagerDecoder* decoder = pagerReceiver->decoders[pagerData->decoder];
        PagerProtocol* protocol = pagerReceiver->protocols[pagerData->protocol];

        PagerActionsScreen* screen = new PagerActionsScreen(config, getPager, decoder, protocol, subghz);
        UiManager::GetInstance()->PushView(screen->GetView());
    }

    bool goBack() {
        if(menuView->GetElementsCount() > 0) {
            DialogUiView* confirmGoBack = new DialogUiView("Really stop scan?", "You may loose captured signals");
            confirmGoBack->AddLeftButton("No");
            confirmGoBack->AddRightButton("Yes");
            confirmGoBack->SetResultHandler(HANDLER_1ARG(&ScanStationsScreen::goBackConfirmationHandler));
            UiManager::GetInstance()->PushView(confirmGoBack);
            return false;
        }
        return true;
    }

    void goBackConfirmationHandler(DialogExResult dialogResult) {
        if(dialogResult == DialogExResultRight) {
            UiManager::GetInstance()->PopView(false);
        }
    }

    void destroy() {
        delete subghz;
        delete pagerReceiver;
        delete this;
    }
};
