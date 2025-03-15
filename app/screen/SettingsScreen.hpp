#pragma once

#include "app/AppConfig.hpp"
#include "lib/String.hpp"
#include "lib/ui/view/VariableItemListUiView.hpp"

class SettingsScreen {
private:
    AppConfig* config;
    VariableItemListUiView* varItemList;

    UiVariableItem* maxPagerItem;
    UiVariableItem* signalRepeatItem;
    UiVariableItem* ignoreSavedItem;
    UiVariableItem* autosaveFoundItem;
    UiVariableItem* debugModeItem;

    String maxPagerStr;
    String signalRepeatStr;

public:
    SettingsScreen(AppConfig* config) {
        this->config = config;

        varItemList = new VariableItemListUiView();
        varItemList->SetOnDestroyHandler(HANDLER(&SettingsScreen::destroy));

        varItemList->AddItem(
            maxPagerItem = new UiVariableItem(
                "Max pager value",
                config->MaxPagerForBatchOrDetection - 1,
                UINT8_MAX,
                [this](uint8_t val) {
                    this->config->MaxPagerForBatchOrDetection = val + 1;
                    return maxPagerStr.fromInt(this->config->MaxPagerForBatchOrDetection);
                }
            )
        );

        varItemList->AddItem(
            signalRepeatItem = new UiVariableItem(
                "Times to repeat signal",
                config->SignalRepeats - 1,
                UINT8_MAX,
                [this](uint8_t val) {
                    this->config->SignalRepeats = val + 1;
                    return signalRepeatStr.fromInt(this->config->SignalRepeats);
                }
            )
        );

        varItemList->AddItem(
            ignoreSavedItem = new UiVariableItem(
                "Ignore saved stations",
                config->IgnoreMessagesFromSavedStations,
                2,
                [this](uint8_t val) {
                    this->config->IgnoreMessagesFromSavedStations = val;
                    return boolOption(val);
                }
            )
        );

        varItemList->AddItem(
            autosaveFoundItem = new UiVariableItem(
                "Autosave found signals",
                config->AutosaveFoundSignals,
                2,
                [this](uint8_t val) {
                    this->config->AutosaveFoundSignals = val;
                    return boolOption(val);
                }
            )
        );

        // clang-format off
        varItemList->AddItem(
            debugModeItem = new UiVariableItem(
                "Debug mode",
                config->Debug,
                2,
                [this](uint8_t val) {
                    this->config->Debug = val;
                    return boolOption(val);
                }
            )
        );
        // clang-format on
    }

    UiView* GetView() {
        return varItemList;
    }

private:
    const char* boolOption(uint8_t value) {
        return value ? "ON" : "OFF";
    }

    void destroy() {
        config->Save();

        delete maxPagerItem;
        delete signalRepeatItem;
        delete ignoreSavedItem;
        delete autosaveFoundItem;
        delete debugModeItem;

        delete this;
    }
};
