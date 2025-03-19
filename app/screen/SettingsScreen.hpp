#pragma once

#include "app/AppConfig.hpp"
#include "app/pager/PagerReceiver.hpp"
#include "lib/String.hpp"
#include "lib/hardware/subghz/SubGhzModule.hpp"
#include "lib/ui/view/VariableItemListUiView.hpp"

class SettingsScreen {
private:
    AppConfig* config;
    SubGhzModule* subghz;
    PagerReceiver* receiver;
    SubGhzSettings subghzSettings;
    VariableItemListUiView* varItemList;

    UiVariableItem* frequencyItem;
    UiVariableItem* maxPagerItem;
    UiVariableItem* signalRepeatItem;
    UiVariableItem* ignoreSavedItem;
    UiVariableItem* autosaveFoundItem;
    UiVariableItem* debugModeItem;

    String frequencyStr;
    String maxPagerStr;
    String signalRepeatStr;

public:
    SettingsScreen(AppConfig* config, PagerReceiver* receiver, SubGhzModule* subghz) {
        this->config = config;
        this->receiver = receiver;
        this->subghz = subghz;

        varItemList = new VariableItemListUiView();
        varItemList->SetOnDestroyHandler(HANDLER(&SettingsScreen::destroy));

        varItemList->AddItem(
            frequencyItem = new UiVariableItem(
                "Frequency",
                subghzSettings.GetFrequencyIndex(config->Frequency),
                subghzSettings.GetFrequencyCount(),
                [this](uint8_t val) {
                    uint32_t freq = this->config->Frequency = this->subghzSettings.GetFrequency(val);
                    this->subghz->SetFrequency(this->config->Frequency);
                    return frequencyStr.format("%lu.%02lu", freq / 1000000, (freq % 1000000) / 10000);
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
                "Saved stations",
                config->SavedStrategy,
                SavedStationStrategyValuesCount,
                [this](uint8_t val) {
                    this->config->SavedStrategy = static_cast<enum SavedStationStrategy>(val);
                    return savedStationsStrategy(this->config->SavedStrategy);
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

    const char* savedStationsStrategy(SavedStationStrategy value) {
        switch(value) {
        case IGNORE:
            return "Ignore";

        case SHOW_NAME:
            return "Show name";

        case HIDE:
            return "Hide";

        default:
            return NULL;
        }
    }

    void destroy() {
        config->Save();
        receiver->SetUserCategory(config->CurrentUserCategory);
        receiver->ReloadKnownStations();

        delete frequencyItem;
        delete maxPagerItem;
        delete signalRepeatItem;
        delete ignoreSavedItem;
        delete autosaveFoundItem;
        delete debugModeItem;

        delete this;
    }
};
