#ifndef _PAGER_OPTIONS_SCREEN_CLASS_
#define _PAGER_OPTIONS_SCREEN_CLASS_

#include "app/pager/PagerReceiver.cpp"
#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/VariableItemListUiView.cpp"

#include "lib/ui/UiManager.cpp"

class PagerOptionsScreen {
private:
    uint32_t pagerIndex;
    PagerReceiver* receiver;
    VariableItemListUiView* varItemList;

    UiVariableItem* encodingItem;
    UiVariableItem* stationItem;

public:
    PagerOptionsScreen(PagerReceiver* receiver, uint32_t pagerIndex) {
        this->pagerIndex = pagerIndex;
        this->receiver = receiver;

        varItemList = new VariableItemListUiView();

        varItemList->AddItem(
            encodingItem = new UiVariableItem(
                "Encoding",
                receiver->GetPagerData(pagerIndex).decoder,
                receiver->decoders.size(),
                HANDLER_1ARG(&PagerOptionsScreen::encodingValueChanged)));

        varItemList->AddItem(
            encodingItem = new UiVariableItem(
                "Station",
                receiver->GetPagerData(pagerIndex).decoder,
                receiver->decoders.size(),
                HANDLER_1ARG(&PagerOptionsScreen::stationValueChanged)));

        // varItemList->AddItem(UiVariableItem *item, function<void (uint32_t)> changeHandler)
        // menuView->AddItem("Scan for station signals", HANDLER_1ARG(&MainMenuScreen::scanStationsMenuPressed));
        // menuView->AddItem("Saved staions database", HANDLER_1ARG(&MainMenuScreen::stationDatabasePressed));
        // menuView->AddItem("About / Manual", HANDLER_1ARG(&MainMenuScreen::aboutPressed));
    }

    UiView* GetView() {
        return varItemList;
    }

private:
    const char* encodingValueChanged(uint32_t index) {
        UNUSED(index);
        return receiver->decoders[index]->GetShortName();
        // return decoders[index].GetShortName();
    }

    const char* stationValueChanged(uint32_t) {
        //return receiver->GetPagerData(pagerIndex).decoder
    }
};

#endif //_PAGER_OPTIONS_SCREEN_CLASS_
