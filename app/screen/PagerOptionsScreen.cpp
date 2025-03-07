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
    UiVariableItem* hexItem = NULL;

public:
    PagerOptionsScreen(PagerReceiver* receiver, uint32_t pagerIndex) {
        this->pager = receiver->GetPagerData(pagerIndex);
        this->receiver = receiver;

        varItemList = new VariableItemListUiView();

        PagerDecoder* decoder = receiver->decoders[pager->decoder];

        varItemList->AddItem(
            encodingItem = new UiVariableItem(
                "Encoding",
                receiver->GetPagerData(pagerIndex)->decoder,
                receiver->decoders.size(),
                HANDLER_1ARG(&PagerOptionsScreen::encodingValueChanged)));

        varItemList->AddItem(stationItem = new UiVariableItem("Station", HANDLER_1ARG(&PagerOptionsScreen::stationValueChanged)));
        varItemList->AddItem(pagerItem = new UiVariableItem("Pager", HANDLER_1ARG(&PagerOptionsScreen::pagerValueChanged)));
        varItemList->AddItem(
            actionItem = new UiVariableItem(
                "Action",
                decoder->GetActionValue(pager->data),
                decoder->GetActionsCount(),
                HANDLER_1ARG(&PagerOptionsScreen::actionValueChanged)));

        varItemList->AddItem(hexItem = new UiVariableItem("HEX value", HANDLER_1ARG(&PagerOptionsScreen::hexValueChanged)));

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
            PagerDecoder* decoder = receiver->decoders[pager->decoder];
            actionItem->SetSelectedItem(decoder->GetActionValue(pager->data), decoder->GetActionsCount());
        }
        return receiver->decoders[pager->decoder]->GetShortName();
    }

    const char* stationValueChanged(int8_t) {
        return Helpers::intToString(receiver->decoders[pager->decoder]->GetStation(pager->data));
    }

    const char* pagerValueChanged(int8_t) {
        return Helpers::intToString(receiver->decoders[pager->decoder]->GetPager(pager->data));
    }

    const char* actionValueChanged(int8_t value) {
        PagerDecoder* decoder = receiver->decoders[pager->decoder];
        if(decoder->GetActionValue(pager->data) != value) {
            pager->data = decoder->SetActionValue(pager->data, value);
        }

        if(hexItem != NULL) {
            hexItem->Refresh();
        }

        uint8_t actionValue = decoder->GetActionValue(pager->data);
        PagerAction action = decoder->GetAction(pager->data);
        const char* actionDesc = PagerActions::GetDescription(action);
        return furi_string_get_cstr(furi_string_alloc_printf("%d (%s)", actionValue, actionDesc));
    }

    const char* hexValueChanged(int8_t) {
        return furi_string_get_cstr(furi_string_alloc_printf("%06X", (unsigned int)pager->data));
    }

public:
    UiView* GetView() {
        return varItemList;
    }
};

#endif //_PAGER_OPTIONS_SCREEN_CLASS_
