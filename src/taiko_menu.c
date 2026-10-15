#include "taiko_menu.h"

static const uint32_t intervals[] = {1000, 3000, 5000, 10000};
#define INTERVAL_COUNT (sizeof(intervals) / sizeof(intervals[0]))
static const uint8_t playback_speeds[] =
    {5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20};
#define PLAYBACK_SPEED_COUNT (sizeof(playback_speeds) / sizeof(playback_speeds[0]))
static const uint8_t scroll_speeds[] = {5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                        17, 18, 19, 20, 21, 22, 23, 24, 25, 30, 40};
#define SCROLL_SPEED_COUNT (sizeof(scroll_speeds) / sizeof(scroll_speeds[0]))

void taiko_menu_init(TaikoMenu* menu) {
    *menu = (TaikoMenu){
        .backward_index = 2,
        .forward_index = 2,
        .don_volume = 70,
        .ka_volume = 70,
        .courses = 1U << TjaCourseOni,
        .course = TjaCourseOni,
        .playback_index = 5,
        .scroll_index = 5};
    menu->gpio = taiko_gpio_default_config();
}

void taiko_menu_open(TaikoMenu* menu) {
    menu->selected = TaikoMenuReturn;
    menu->gpio_settings = false;
    menu->open = true;
}

void taiko_menu_move(TaikoMenu* menu, bool down) {
    if(!menu->open) return;
    if(menu->gpio_settings) {
        menu->gpio_selected = (menu->gpio_selected + (down ? 1 : TaikoGpioMenuItemCount - 1)) %
                              TaikoGpioMenuItemCount;
        return;
    }
    menu->selected = (menu->selected + (down ? 1 : TaikoMenuItemCount - 1)) % TaikoMenuItemCount;
}

void taiko_menu_adjust(TaikoMenu* menu, bool forward) {
    if(!menu->open) return;
    if(menu->gpio_settings) {
        if(menu->gpio_selected != TaikoGpioMenuReturn)
            taiko_gpio_config_adjust(&menu->gpio, menu->gpio_selected - 1, forward);
        return;
    }
    if(menu->selected == TaikoMenuGpioOutput) {
        menu->gpio.enabled = !menu->gpio.enabled;
        return;
    }
    if(menu->selected == TaikoMenuDifficulty) {
        for(int i = 1; i <= TjaCourseCount; i++) {
            TjaCourse next = (menu->course + (forward ? i : TjaCourseCount - i)) % TjaCourseCount;
            if(menu->courses & (1U << next)) {
                menu->course = next;
                break;
            }
        }
        return;
    }
    if(menu->selected == TaikoMenuPlaybackSpeed || menu->selected == TaikoMenuScrollSpeed) {
        uint8_t* index = menu->selected == TaikoMenuPlaybackSpeed ? &menu->playback_index :
                                                                    &menu->scroll_index;
        size_t count = menu->selected == TaikoMenuPlaybackSpeed ? PLAYBACK_SPEED_COUNT :
                                                                  SCROLL_SPEED_COUNT;
        *index = (*index + (forward ? 1 : count - 1)) % count;
        return;
    }
    if(menu->selected == TaikoMenuDonVolume || menu->selected == TaikoMenuKaVolume) {
        uint8_t* volume = menu->selected == TaikoMenuDonVolume ? &menu->don_volume :
                                                                 &menu->ka_volume;
        if(forward && *volume < 100) *volume += 10;
        if(!forward && *volume > 0) *volume -= 10;
        return;
    }
    uint8_t* index;
    if(menu->selected == TaikoMenuBackward)
        index = &menu->backward_index;
    else if(menu->selected == TaikoMenuForward)
        index = &menu->forward_index;
    else
        return;
    *index = (*index + (forward ? 1 : INTERVAL_COUNT - 1)) % INTERVAL_COUNT;
}

TaikoMenuAction taiko_menu_activate(TaikoMenu* menu) {
    if(!menu->open) return TaikoMenuActionNone;
    if(menu->gpio_settings) {
        if(menu->gpio_selected == TaikoGpioMenuReturn) return taiko_menu_back(menu);
        taiko_menu_adjust(menu, true);
        return TaikoMenuActionNone;
    }
    if(menu->selected == TaikoMenuGpioSetting) {
        menu->gpio_settings = true;
        menu->gpio_selected = TaikoGpioMenuReturn;
        return TaikoMenuActionNone;
    }
    TaikoMenuAction action;
    switch(menu->selected) {
    case TaikoMenuReturn:
        action = TaikoMenuActionReturn;
        break;
    case TaikoMenuRestart:
        action = TaikoMenuActionRestart;
        break;
    case TaikoMenuNewSong:
        action = TaikoMenuActionNewSong;
        break;
    case TaikoMenuExit:
        action = TaikoMenuActionExit;
        break;
    case TaikoMenuRestoreDefaults:
        return TaikoMenuActionRestoreDefaults;
    default:
        taiko_menu_adjust(menu, true);
        return TaikoMenuActionNone;
    }
    menu->open = false;
    return action;
}

TaikoMenuAction taiko_menu_back(TaikoMenu* menu) {
    if(!menu->open) return TaikoMenuActionNone;
    if(menu->gpio_settings) {
        menu->gpio_settings = false;
        menu->selected = TaikoMenuGpioSetting;
        return TaikoMenuActionNone;
    }
    menu->open = false;
    return TaikoMenuActionReturn;
}

uint32_t taiko_menu_backward_ms(const TaikoMenu* menu) {
    return intervals[menu->backward_index];
}

uint32_t taiko_menu_forward_ms(const TaikoMenu* menu) {
    return intervals[menu->forward_index];
}

uint8_t taiko_menu_playback_tenths(const TaikoMenu* menu) {
    return playback_speeds[menu->playback_index];
}

uint8_t taiko_menu_scroll_tenths(const TaikoMenu* menu) {
    return scroll_speeds[menu->scroll_index];
}

bool taiko_menu_set_value(TaikoMenu* menu, TaikoMenuItem item, uint32_t value) {
    if(item == TaikoMenuBackward || item == TaikoMenuForward) {
        for(size_t i = 0; i < INTERVAL_COUNT; i++) {
            if(intervals[i] != value) continue;
            if(item == TaikoMenuBackward)
                menu->backward_index = i;
            else
                menu->forward_index = i;
            return true;
        }
    } else if(item == TaikoMenuPlaybackSpeed || item == TaikoMenuScrollSpeed) {
        const uint8_t* values = item == TaikoMenuPlaybackSpeed ? playback_speeds : scroll_speeds;
        size_t count = item == TaikoMenuPlaybackSpeed ? PLAYBACK_SPEED_COUNT : SCROLL_SPEED_COUNT;
        for(size_t i = 0; i < count; i++) {
            if(values[i] != value) continue;
            if(item == TaikoMenuPlaybackSpeed)
                menu->playback_index = i;
            else
                menu->scroll_index = i;
            return true;
        }
    }
    return false;
}
