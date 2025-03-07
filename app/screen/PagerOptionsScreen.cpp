#ifndef _PAGER_OPTIONS_SCREEN_CLASS_
#define _PAGER_OPTIONS_SCREEN_CLASS_

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
    }

    UiView* GetView() {
        return varItemList;
    }

private:
    const char* encodingValueChanged(uint32_t index) {
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

    const char* stationValueChanged(uint32_t) {
        int value = receiver->decoders[pager->decoder]->GetStation(pager->data);
        FuriString* str = furi_string_alloc_printf("%d", value);
        return furi_string_get_cstr(str);
    }

    const char* pagerValueChanged(uint32_t) {
        int value = receiver->decoders[pager->decoder]->GetPager(pager->data);
        FuriString* str = furi_string_alloc_printf("%d", value);
        return furi_string_get_cstr(str);
    }

    const char* actionValueChanged(uint32_t) {
        int value = receiver->decoders[pager->decoder]->GetActionValue(pager->data);
        FuriString* str = furi_string_alloc_printf("%d", value);
        return furi_string_get_cstr(str);
    }
};

#endif //_PAGER_OPTIONS_SCREEN_CLASS_
