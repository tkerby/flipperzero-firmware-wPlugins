#ifndef _PAGER_OPTIONS_SCREEN_CLASS_
#define _PAGER_OPTIONS_SCREEN_CLASS_

#include "lib/Helpers.cpp"
#include "app/pager/PagerReceiver.cpp"
#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/VariableItemListUiView.cpp"

#include "lib/ui/UiManager.cpp"

class PagerOptionsScreen {
private:
    PagerDataStored* pager;
    PagerReceiver* receiver;
    VariableItemListUiView* varItemList;

    UiVariableItem* encodingItem = NULL;
    UiVariableItem* stationItem = NULL;
    UiVariableItem* pagerItem = NULL;
    UiVariableItem* actionItem = NULL;

public:
    PagerOptionsScreen(PagerReceiver* receiver, uint32_t pagerIndex) {
        this->pager = receiver->GetPagerData(pagerIndex);
        this->receiver = receiver;

        varItemList = new VariableItemListUiView();

        varItemList->AddItem(
            encodingItem = new UiVariableItem(
                "Encoding",
                receiver->GetPagerData(pagerIndex)->decoder,
                receiver->decoders.size(),
                HANDLER_1ARG(&PagerOptionsScreen::encodingValueChanged)));

        varItemList->AddItem(stationItem = new UiVariableItem("Station", HANDLER_1ARG(&PagerOptionsScreen::stationValueChanged)));
        varItemList->AddItem(pagerItem = new UiVariableItem("Pager", HANDLER_1ARG(&PagerOptionsScreen::pagerValueChanged)));
        varItemList->AddItem(actionItem = new UiVariableItem("Action", HANDLER_1ARG(&PagerOptionsScreen::actionValueChanged)));

        varItemList->AddItem(new UiVariableItem("Protocol", receiver->protocols[pager->protocol]->GetDisplayName()));
        varItemList->AddItem(new UiVariableItem(
            "Signal Repeats", Helpers::intToString(pager->repeats, pager->repeats == MAX_REPEATS ? "%d+" : "%d")));
    }

private:
    const char* encodingValueChanged(int8_t index) {
        pager->decoder = index;
        if(stationItem != NULL) {
            stationItem->Refresh();
        }
        if(pagerItem != NULL) {
            pagerItem->Refresh();
        }
        if(actionItem != NULL) {
            actionItem->Refresh();
        }
        return receiver->decoders[pager->decoder]->GetShortName();
    }

    const char* stationValueChanged(int8_t) {
        return Helpers::intToString(receiver->decoders[pager->decoder]->GetStation(pager->data));
    }

    const char* pagerValueChanged(int8_t) {
        return Helpers::intToString(receiver->decoders[pager->decoder]->GetPager(pager->data));
    }

    const char* actionValueChanged(int8_t) {
        return Helpers::intToString(receiver->decoders[pager->decoder]->GetActionValue(pager->data));
    }

public:
    UiView* GetView() {
        return varItemList;
    }
};

#endif //_PAGER_OPTIONS_SCREEN_CLASS_
