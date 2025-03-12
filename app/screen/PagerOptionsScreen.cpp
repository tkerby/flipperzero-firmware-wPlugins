#ifndef _PAGER_OPTIONS_SCREEN_CLASS_
#define _PAGER_OPTIONS_SCREEN_CLASS_

#include "lib/String.cpp"
#include "app/pager/PagerReceiver.cpp"
#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/VariableItemListUiView.cpp"

#define TE_DIV 10

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
    UiVariableItem* teItem = NULL;
    UiVariableItem* repeatsItem = NULL;

    String stationStr;
    String pagerStr;
    String actionStr;
    String hexStr;
    String repeatsStr;
    String teStr;

public:
    PagerOptionsScreen(PagerReceiver* receiver, uint32_t pagerIndex) {
        this->pager = receiver->GetPagerData(pagerIndex);
        this->receiver = receiver;

        PagerDecoder* decoder = receiver->decoders[pager->decoder];
        PagerProtocol* protocol = receiver->protocols[pager->protocol];

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
        updatePagerIsEditable();

        varItemList->AddItem(
            actionItem = new UiVariableItem(
                "Action",
                decoder->GetActionValue(pager->data),
                decoder->GetActionsCount(),
                HANDLER_1ARG(&PagerOptionsScreen::actionValueChanged)
            )
        );

        varItemList->AddItem(hexItem = new UiVariableItem("HEX value", HANDLER_1ARG(&PagerOptionsScreen::hexValueChanged)));
        varItemList->AddItem(protocolItem = new UiVariableItem("Protocol", protocol->GetDisplayName()));
        varItemList->AddItem(
            teItem = new UiVariableItem(
                "TE", pager->te / TE_DIV, protocol->GetMaxTE() / TE_DIV, HANDLER_1ARG(&PagerOptionsScreen::teValueChanged)
            )
        );
        varItemList->AddItem(
            repeatsItem = new UiVariableItem(
                "Signal Repeats", repeatsStr.format(pager->repeats == MAX_REPEATS ? "%d+" : "%d", pager->repeats)
            )
        );
    }

private:
    void updatePagerIsEditable() {
        int pagerNum = receiver->decoders[pager->decoder]->GetPager(pager->data);
        if(pagerNum < UINT8_MAX) {
            pagerItem->SetSelectedItem(pagerNum, UINT8_MAX);
        } else {
            pagerItem->SetSelectedItem(0, 1);
        }
    }

    const char* encodingValueChanged(uint8_t index) {
        PagerDecoder* decoder = receiver->decoders[index];
        pager->decoder = index;

        if(stationItem != NULL) {
            stationItem->Refresh();
        }
        if(pagerItem != NULL) {
            updatePagerIsEditable();
            pagerItem->Refresh();
        }
        if(actionItem != NULL) {
            actionItem->SetSelectedItem(decoder->GetActionValue(pager->data), decoder->GetActionsCount());
        }
        return receiver->decoders[pager->decoder]->GetShortName();
    }

    const char* stationValueChanged(uint8_t) {
        return stationStr.fromInt(receiver->decoders[pager->decoder]->GetStation(pager->data));
    }

    const char* pagerValueChanged(uint8_t newPager) {
        PagerDecoder* decoder = receiver->decoders[pager->decoder];
        if(pagerItem->Editable() && newPager != decoder->GetPager(pager->data)) {
            pager->data = decoder->SetPager(pager->data, newPager);
            pager->edited = true;

            if(hexItem != NULL) {
                hexItem->Refresh();
            }
        }
        return pagerStr.fromInt(decoder->GetPager(pager->data));
    }

    const char* actionValueChanged(uint8_t value) {
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

    const char* hexValueChanged(uint8_t) {
        return hexStr.format("%06X", (unsigned int)pager->data);
    }

    const char* teValueChanged(uint8_t newTeIndex) {
        if(newTeIndex != pager->te / TE_DIV) {
            int teDiff = pager->te % TE_DIV;
            int newTe = newTeIndex * TE_DIV + teDiff;

            pager->te = newTe;
            pager->edited = true;
        }

        return teStr.format("%d", pager->te);
    }

    void destroy() {
        delete encodingItem;
        delete stationItem;
        delete pagerItem;
        delete actionItem;
        delete hexItem;
        delete teItem;
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
