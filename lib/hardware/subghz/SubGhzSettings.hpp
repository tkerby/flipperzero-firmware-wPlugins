#pragma once

#include "lib/subghz/subghz_setting.h"

class SubGhzSettings {
private:
    SubGhzSetting* setting;

public:
    SubGhzSettings() {
        setting = subghz_setting_alloc();
        subghz_setting_load(setting, EXT_PATH("subghz/assets/setting_user"));
    }

    uint32_t GetFrequency(size_t index) {
        return subghz_setting_get_frequency(setting, index);
    }

    uint32_t GetDefaultFrequency() {
        return subghz_setting_get_default_frequency(setting);
    }

    size_t GetDefaultFrequencyIndex() {
        return subghz_setting_get_frequency_default_index(setting);
    }

    size_t GetFrequencyIndex(uint32_t freq) {
        for(size_t i = 0; i < GetFrequencyCount(); i++) {
            if(GetFrequency(i) == freq) {
                return i;
            }
        }
        return GetDefaultFrequencyIndex();
    }

    size_t GetFrequencyCount() {
        return subghz_setting_get_frequency_count(setting);
    }

    ~SubGhzSettings() {
        subghz_setting_free(setting);
    }
};
