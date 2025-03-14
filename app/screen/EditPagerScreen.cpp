#ifndef _EDIT_PAGER_SCREEN_CLASS_
#define _EDIT_PAGER_SCREEN_CLASS_

#include "lib/String.cpp"
#include "app/pager/PagerReceiver.cpp"
#include "lib/hardware/subghz/SubGhzModule.cpp"
#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/VariableItemListUiView.cpp"

#define TE_DIV 10

class EditPagerScreen {
private:
    AppConfig* config;
    SubGhzModule* subghz;
    PagerReceiver* receiver;
    PagerDataGetter getPager;
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
    EditPagerScreen(AppConfig* config, SubGhzModule* subghz, PagerReceiver* receiver, PagerDataGetter pagerGetter) {
        this->config = config;
        this->subghz = subghz;
        this->receiver = receiver;
        this->getPager = pagerGetter;

        PagerDataStored* pager = getPager();
        PagerDecoder* decoder = receiver->decoders[pager->decoder];
        PagerProtocol* protocol = receiver->protocols[pager->protocol];

        varItemList = new VariableItemListUiView();
        varItemList->SetOnDestroyHandler(HANDLER(&EditPagerScreen::destroy));
        varItemList->SetEnterPressHandler(HANDLER_1ARG(&EditPagerScreen::enterPressed));

        varItemList->AddItem(
            encodingItem = new UiVariableItem(
                "Encoding", pager->decoder, receiver->decoders.size(), HANDLER_1ARG(&EditPagerScreen::encodingValueChanged)
            )
        );

        varItemList->AddItem(stationItem = new UiVariableItem("Station", HANDLER_1ARG(&EditPagerScreen::stationValueChanged)));
        varItemList->AddItem(pagerItem = new UiVariableItem("Pager", HANDLER_1ARG(&EditPagerScreen::pagerValueChanged)));
        updatePagerIsEditable();

        varItemList->AddItem(
            actionItem = new UiVariableItem(
                "Action",
                decoder->GetActionValue(pager->data),
                decoder->GetActionsCount(),
                HANDLER_1ARG(&EditPagerScreen::actionValueChanged)
            )
        );

        varItemList->AddItem(hexItem = new UiVariableItem("HEX value", HANDLER_1ARG(&EditPagerScreen::hexValueChanged)));
        varItemList->AddItem(protocolItem = new UiVariableItem("Protocol", protocol->GetDisplayName()));
        varItemList->AddItem(
            teItem = new UiVariableItem(
                "TE", pager->te / TE_DIV, protocol->GetMaxTE() / TE_DIV, HANDLER_1ARG(&EditPagerScreen::teValueChanged)
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
        PagerDataStored* pager = getPager();
        int pagerNum = receiver->decoders[pager->decoder]->GetPager(pager->data);
        if(pagerNum < UINT8_MAX) {
            pagerItem->SetSelectedItem(pagerNum, UINT8_MAX);
        } else {
            pagerItem->SetSelectedItem(0, 1);
        }
    }

    void enterPressed(uint32_t) {
        PagerDataStored* pager = getPager();
        PagerProtocol* protocol = receiver->protocols[pager->protocol];
        subghz->Transmit(protocol->CreatePayload(pager->data, pager->te, config->SignalRepeats));
    }

    const char* encodingValueChanged(uint8_t index) {
        PagerDataStored* pager = getPager();
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
        PagerDataStored* pager = getPager();
        return stationStr.fromInt(receiver->decoders[pager->decoder]->GetStation(pager->data));
    }

    const char* pagerValueChanged(uint8_t newPager) {
        PagerDataStored* pager = getPager();
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
        PagerDataStored* pager = getPager();
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
        PagerDataStored* pager = getPager();
        return hexStr.format("%06X", (unsigned int)pager->data);
    }

    const char* teValueChanged(uint8_t newTeIndex) {
        PagerDataStored* pager = getPager();
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

#endif //_EDIT_PAGER_SCREEN_CLASS_
