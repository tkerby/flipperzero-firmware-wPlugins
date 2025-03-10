#ifndef _PAGER_OPTIONS_SCREEN_CLASS_
#define _PAGER_OPTIONS_SCREEN_CLASS_

#include "lib/String.cpp"
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

    UiVariableItem* protocolItem = NULL;
    UiVariableItem* repeatsItem = NULL;

    String stationStr;
    String pagerStr;
    String actionStr;
    String hexStr;
    String repeatsStr;

public:
    PagerOptionsScreen(PagerReceiver* receiver, uint32_t pagerIndex) {
        this->pager = receiver->GetPagerData(pagerIndex);
        this->receiver = receiver;

        PagerDecoder* decoder = receiver->decoders[pager->decoder];

        varItemList = new VariableItemListUiView();
        varItemList->SetOnDestroyHandler(HANDLER(&PagerOptionsScreen::destroy));

        varItemList->AddItem(
            encodingItem = new UiVariableItem(
                "Encoding",
                receiver->GetPagerData(pagerIndex)->decoder,
                receiver->decoders.size(),
                HANDLER_1ARG(&PagerOptionsScreen::encodingValueChanged)
            )
        );

        varItemList->AddItem(stationItem = new UiVariableItem("Station", HANDLER_1ARG(&PagerOptionsScreen::stationValueChanged)));
        varItemList->AddItem(pagerItem = new UiVariableItem("Pager", HANDLER_1ARG(&PagerOptionsScreen::pagerValueChanged)));
        varItemList->AddItem(
            actionItem = new UiVariableItem(
                "Action",
                decoder->GetActionValue(pager->data),
                decoder->GetActionsCount(),
                HANDLER_1ARG(&PagerOptionsScreen::actionValueChanged)
            )
        );

        varItemList->AddItem(hexItem = new UiVariableItem("HEX value", HANDLER_1ARG(&PagerOptionsScreen::hexValueChanged)));

        varItemList->AddItem(
            protocolItem = new UiVariableItem("Protocol", receiver->protocols[pager->protocol]->GetDisplayName())
        );
        varItemList->AddItem(
            repeatsItem = new UiVariableItem(
                "Signal Repeats", repeatsStr.format(pager->repeats == MAX_REPEATS ? "%d+" : "%d", pager->repeats)
            )
        );
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
        return stationStr.fromInt(receiver->decoders[pager->decoder]->GetStation(pager->data));
    }

    const char* pagerValueChanged(int8_t) {
        return pagerStr.fromInt(receiver->decoders[pager->decoder]->GetPager(pager->data));
    }

    const char* actionValueChanged(int8_t value) {
        PagerDecoder* decoder = receiver->decoders[pager->decoder];
        if(decoder->GetActionValue(pager->data) != value) {
            pager->data = decoder->SetActionValue(pager->data, value);
            pager->edited = true;
        }

        if(hexItem != NULL) {
            hexItem->Refresh();
        }

        uint8_t actionValue = decoder->GetActionValue(pager->data);
        PagerAction action = decoder->GetAction(pager->data);
        const char* actionDesc = PagerActions::GetDescription(action);

        return actionStr.format("%d (%s)", actionValue, actionDesc);
    }

    const char* hexValueChanged(int8_t) {
        return hexStr.format("%06X", (unsigned int)pager->data);
    }

    void destroy() {
        delete encodingItem;
        delete stationItem;
        delete pagerItem;
        delete actionItem;
        delete hexItem;
        delete protocolItem;
        delete repeatsItem;
        delete this;
    }

public:
    UiView* GetView() {
        return varItemList;
    }
};

#endif //_PAGER_OPTIONS_SCREEN_CLASS_
