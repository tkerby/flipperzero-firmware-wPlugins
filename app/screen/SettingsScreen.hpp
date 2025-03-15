#pragma once

#include <vector>
#include "app/AppConfig.hpp"
#include "lib/String.hpp"
#include "lib/ui/view/VariableItemListUiView.hpp"

class SettingsScreen {
private:
    AppConfig* config;
    SubGhzModule* subghz;
    VariableItemListUiView* varItemList;

    UiVariableItem* frequencyItem;
    UiVariableItem* maxPagerItem;
    UiVariableItem* signalRepeatItem;
    UiVariableItem* ignoreSavedItem;
    UiVariableItem* autosaveFoundItem;
    UiVariableItem* debugModeItem;

    String maxPagerStr;
    String signalRepeatStr;

    //TODO: add only supported frequencies
    //TODO: change on-the-fly
    vector<uint32_t> frequencies{
        315000000,
        433920000,
        467750000,
    };

public:
    SettingsScreen(AppConfig* config, SubGhzModule* subghz) {
        this->config = config;
        this->subghz = subghz;

        varItemList = new VariableItemListUiView();
        varItemList->SetOnDestroyHandler(HANDLER(&SettingsScreen::destroy));

        varItemList->AddItem(
            frequencyItem = new UiVariableItem(
                "Frequency",
                indexOf(config->Frequency, frequencies, indexOf<uint32_t>(DEFAULT_FREQUENCY, frequencies, 0)),
                frequencies.size(),
                [this](uint8_t val) {
                    uint32_t freq = this->config->Frequency = frequencies[val];
                    //TODO: change on-the-fly HERE
                    return maxPagerStr.format("%lu.%02lu", freq / 1000000, (freq % 1000000) / 10000);
                }
            )
        );

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

    template <class T>
    uint8_t indexOf(T value, vector<T> values, uint8_t defaultIndex) {
        for(uint8_t i = 0; i < (uint8_t)values.size(); i++) {
            if(values[i] == value) {
                return i;
            }
        }
        return defaultIndex;
    }

    void destroy() {
        config->Save();

        delete frequencyItem;
        delete maxPagerItem;
        delete signalRepeatItem;
        delete ignoreSavedItem;
        delete autosaveFoundItem;
        delete debugModeItem;

        delete this;
    }
};
