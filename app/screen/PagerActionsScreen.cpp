#ifndef _PAGER_ACTIONS_SCREEN_CLASS_
#define _PAGER_ACTIONS_SCREEN_CLASS_

#include "lib/String.cpp"
#include "app/pager/PagerDataStored.cpp"
#include "app/pager/decoder/PagerDecoder.cpp"
#include "app/pager/protocol/PagerProtocol.cpp"
#include "lib/hardware/subghz/SubGhzModule.cpp"
#include "lib/ui/view/UiView.cpp"
#include "lib/ui/view/SubMenuUiView.cpp"
#include "app/screen/BatchTransmissionScreen.cpp"

#include "lib/ui/UiManager.cpp"

class PagerActionsScreen {
private:
    SubMenuUiView* submenu;
    PagerDataStored* pager;
    PagerDecoder* decoder;
    PagerProtocol* protocol;
    SubGhzModule* subghz;
    bool returnToReceiveAfterTransmission;

    String headerStr;
    String resendToAllStr;
    String resendToCurrentStr;
    String** actionsStrings;

public:
    PagerActionsScreen(
        PagerDataStored* pager,
        PagerDecoder* decoder,
        PagerProtocol* protocol,
        SubGhzModule* subghz,
        bool returnToReceiveAfterTransmission
    ) {
        this->pager = pager;
        this->decoder = decoder;
        this->protocol = protocol;
        this->subghz = subghz;
        this->returnToReceiveAfterTransmission = returnToReceiveAfterTransmission;

        PagerAction currentAction = decoder->GetAction(pager->data);
        uint8_t actionValue = decoder->GetActionValue(pager->data);
        uint16_t stationNum = decoder->GetStation(pager->data);
        uint16_t pagerNum = decoder->GetPager(pager->data);

        submenu = new SubMenuUiView(headerStr.format("Station %d actions", stationNum));
        submenu->SetOnDestroyHandler(HANDLER(&PagerActionsScreen::destroy));

        submenu->AddItem(
            resendToAllStr.format("Resend %d (%s) to ALL", actionValue, PagerActions::GetDescription(currentAction)),
            HANDLER_1ARG(&PagerActionsScreen::resendToAll)
        );

        if(currentAction == UNKNOWN) {
            submenu->AddItem(
                resendToCurrentStr.format("Resend only to pager %d", pagerNum), HANDLER_1ARG(&PagerActionsScreen::resendToAll)
            );
        }

        actionsStrings = new String*[decoder->GetSupportedActions().size()];
        for(size_t i = 0; i < decoder->GetSupportedActions().size(); i++) {
            PagerAction action = decoder->GetSupportedActions()[i];
            if(PagerActions::IsPagerActionSpecial(action)) {
                actionsStrings[i] = new String("Trigger action %s", PagerActions::GetDescription(action));
            } else {
                actionsStrings[i] = new String("%s only pager %d", PagerActions::GetDescription(action), pagerNum);
            }

            submenu->AddItem(actionsStrings[i]->cstr(), HANDLER_1ARG(&PagerActionsScreen::resendToAll));
        }

        subghz->SetTransmitCompleteHandler(HANDLER(&PagerActionsScreen::txComplete));
    }

private:
    void resendToAll(uint32_t) {
        SubGhzPayload* payload = protocol->CreatePayload(pager->data, pager->te, 10);
        subghz->Transmit(payload);
        delete payload;
    }

    void txComplete() {
        if(returnToReceiveAfterTransmission) {
            subghz->ReceiveAsync();
        }
    }

    void destroy() {
        for(size_t i = 0; i < decoder->GetSupportedActions().size(); i++) {
            delete actionsStrings[i];
        }
        delete[] actionsStrings;
        delete this;
    }

public:
    UiView* GetView() {
        return submenu;
    }
};

#endif //_PAGER_ACTIONS_SCREEN_CLASS_
