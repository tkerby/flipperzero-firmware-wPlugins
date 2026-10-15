#include "taiko_menu.h"

static const uint32_t intervals[] = {1000, 3000, 5000, 10000};
#define INTERVAL_COUNT (sizeof(intervals) / sizeof(intervals[0]))

void taiko_menu_init(TaikoMenu* menu) {
    *menu =
        (TaikoMenu){.backward_index = 2, .forward_index = 2, .don_volume = 50, .ka_volume = 50};
}

void taiko_menu_open(TaikoMenu* menu) {
    menu->selected = TaikoMenuReturn;
    menu->open = true;
}

void taiko_menu_move(TaikoMenu* menu, bool down) {
    if(!menu->open) return;
    menu->selected = (menu->selected + (down ? 1 : TaikoMenuItemCount - 1)) % TaikoMenuItemCount;
}

void taiko_menu_adjust(TaikoMenu* menu, bool forward) {
    if(!menu->open) return;
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
    TaikoMenuAction action;
    switch(menu->selected) {
    case TaikoMenuReturn:
        action = TaikoMenuActionReturn;
        break;
    case TaikoMenuRestart:
        action = TaikoMenuActionRestart;
        break;
    case TaikoMenuExit:
        action = TaikoMenuActionExit;
        break;
    default:
        taiko_menu_adjust(menu, true);
        return TaikoMenuActionNone;
    }
    menu->open = false;
    return action;
}

uint32_t taiko_menu_backward_ms(const TaikoMenu* menu) {
    return intervals[menu->backward_index];
}

uint32_t taiko_menu_forward_ms(const TaikoMenu* menu) {
    return intervals[menu->forward_index];
}
