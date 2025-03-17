#pragma once

#include "lib/String.hpp"
#include "app/pager/PagerReceiver.hpp"
#include "lib/hardware/subghz/SubGhzModule.hpp"
#include "lib/ui/view/UiView.hpp"
#include "lib/ui/view/VariableItemListUiView.hpp"
#include "lib/ui/view/TextInputUiView.hpp"
#include "lib/ui/view/DialogUiView.hpp"
#include "lib/FlipperDolphin.hpp"
#include "lib/ui/UiManager.hpp"
#include "app/AppFileSystem.hpp"
#include "lib/file/FileManager.hpp"
#include "app/pager/PagerSerializer.hpp"

#define TE_DIV 10

class EditPagerScreen {
private:
    AppConfig* config;
    SubGhzModule* subghz;
    PagerReceiver* receiver;
    PagerDataGetter getPager;
    TextInputUiView* nameInputView;
    VariableItemListUiView* varItemList;

    UiVariableItem* encodingItem = NULL;
    UiVariableItem* stationItem = NULL;
    UiVariableItem* pagerItem = NULL;
    UiVariableItem* actionItem = NULL;
    UiVariableItem* hexItem = NULL;
    UiVariableItem* protocolItem = NULL;
    UiVariableItem* teItem = NULL;
    UiVariableItem* repeatsItem = NULL;

    UiVariableItem* saveAsItem = NULL;
    UiVariableItem* deleteItem = NULL;

    String stationStr;
    String pagerStr;
    String actionStr;
    String hexStr;
    String repeatsStr;
    String teStr;
    int32_t saveAsItemIndex = -1;
    int32_t deleteItemIndex = -1;

    String* savedAsName = NULL;

public:
    EditPagerScreen(
        AppConfig* config,
        SubGhzModule* subghz,
        PagerReceiver* receiver,
        PagerDataGetter pagerGetter,
        String* savedAsNameFromFile
    ) {
        this->config = config;
        this->subghz = subghz;
        this->receiver = receiver;
        this->getPager = pagerGetter;

        StoredPagerData* pager = getPager();
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

        if(savedAsNameFromFile != NULL) {
            this->savedAsName = new String("%s", savedAsNameFromFile);
        } else {
            FileManager* fileManager = new FileManager();
            this->savedAsName = PagerSerializer().LoadOnlyStationName(fileManager, SAVED_STATIONS_PATH, pager);
            delete fileManager;
        }

        if(canSaveOrDelete()) {
            const char* saveAsItemName = savedAsName == NULL ? "Save signal as..." : "Save / Rename";
            saveAsItemIndex = varItemList->AddItem(saveAsItem = new UiVariableItem(saveAsItemName, ""));
            if(savedAsName != NULL) {
                deleteItemIndex = varItemList->AddItem(deleteItem = new UiVariableItem("Delete station", ""));
            }
        }
    }

private:
    bool canSaveOrDelete() {
        bool notKnown = !receiver->IsKnown(getPager());
        bool savedAsNameNotNull = this->savedAsName != NULL;
        FURI_LOG_I(LOG_TAG, "NK: %d, SANNN: %d", notKnown, savedAsNameNotNull);
        return !receiver->IsKnown(getPager()) || this->savedAsName != NULL;
    }

    void updatePagerIsEditable() {
        StoredPagerData* pager = getPager();
        int pagerNum = receiver->decoders[pager->decoder]->GetPager(pager->data);
        if(pagerNum < UINT8_MAX) {
            pagerItem->SetSelectedItem(pagerNum, UINT8_MAX);
        } else {
            pagerItem->SetSelectedItem(0, 1);
        }
    }

    void enterPressed(int32_t index) {
        if(index == saveAsItemIndex) {
            saveAs();
        } else if(index == deleteItemIndex) {
            DialogUiView* removeConfirmation = new DialogUiView("Really delete?", savedAsName->cstr());
            removeConfirmation->AddLeftButton("Nope");
            removeConfirmation->AddRightButton("Yup");
            removeConfirmation->SetResultHandler(HANDLER_1ARG(&EditPagerScreen::confirmDelete));

            UiManager::GetInstance()->PushView(removeConfirmation);
        } else {
            transmitMessage();
        }
    }

    void confirmDelete(DialogExResult result) {
        if(result == DialogExResultRight) {
            String* pagerFile = PagerSerializer().GetFilename(getPager());
            FileManager().DeleteFile(SAVED_STATIONS_PATH, pagerFile->cstr());
            delete pagerFile;

            receiver->ReloadKnownStations();
            UiManager::GetInstance()->PopView(false);
        }
    }

    void transmitMessage() {
        StoredPagerData* pager = getPager();
        PagerProtocol* protocol = receiver->protocols[pager->protocol];
        subghz->Transmit(protocol->CreatePayload(pager->data, pager->te, config->SignalRepeats));

        FlipperDolphin::Deed(DolphinDeedSubGhzSend);
    }

    void saveAs() {
        if(nameInputView == NULL) {
            nameInputView = new TextInputUiView("Enter station name", NAME_MIN_LENGTH, NAME_MAX_LENGTH);
            if(savedAsName != NULL) {
                nameInputView->SetDefaultText(savedAsName);
            }
            nameInputView->SetOnDestroyHandler([this]() { this->nameInputView = NULL; });
            nameInputView->SetResultHandler(HANDLER_1ARG(&EditPagerScreen::saveAsHandler));
        }
        UiManager::GetInstance()->PushView(nameInputView);
    }

    void saveAsHandler(const char* name) {
        FileManager fileManager = FileManager();
        fileManager.CreateDirIfNotExists(STATIONS_PATH);
        fileManager.CreateDirIfNotExists(SAVED_STATIONS_PATH);

        StoredPagerData* pager = getPager();
        PagerDecoder* decoder = receiver->decoders[pager->decoder];
        PagerProtocol* protocol = receiver->protocols[pager->protocol];
        uint32_t frequency = SubGhzSettings().GetFrequency(pager->frequency);

        PagerSerializer().SavePagerData(&fileManager, SAVED_STATIONS_PATH, name, pager, decoder, protocol, frequency);
        FlipperDolphin::Deed(DolphinDeedSubGhzSave);
        receiver->ReloadKnownStations();

        UiManager::GetInstance()->PopView(true);
    }

    const char* encodingValueChanged(uint8_t index) {
        StoredPagerData* pager = getPager();
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
        StoredPagerData* pager = getPager();
        return stationStr.fromInt(receiver->decoders[pager->decoder]->GetStation(pager->data));
    }

    const char* pagerValueChanged(uint8_t newPager) {
        StoredPagerData* pager = getPager();
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
        StoredPagerData* pager = getPager();
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
        StoredPagerData* pager = getPager();
        return hexStr.format("%06X", (unsigned int)pager->data);
    }

    const char* teValueChanged(uint8_t newTeIndex) {
        StoredPagerData* pager = getPager();
        if(newTeIndex != pager->te / TE_DIV) {
            int teDiff = pager->te % TE_DIV;
            int newTe = newTeIndex * TE_DIV + teDiff;

            pager->te = newTe;
            pager->edited = true;
        }

        return teStr.format("%d", pager->te);
    }

    void destroy() {
        if(nameInputView != NULL) {
            delete nameInputView;
        }

        delete encodingItem;
        delete stationItem;
        delete pagerItem;
        delete actionItem;
        delete hexItem;
        delete teItem;
        delete protocolItem;
        delete repeatsItem;

        if(saveAsItem != NULL) {
            delete saveAsItem;
        }

        if(savedAsName != NULL) {
            delete savedAsName;
        }

        if(deleteItem != NULL) {
            delete deleteItem;
        }

        delete this;
    }

public:
    UiView* GetView() {
        return varItemList;
    }
};
