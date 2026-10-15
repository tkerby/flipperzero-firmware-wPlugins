#pragma once

#include <stdbool.h>
#include <stdint.h>

typedef enum {
    TaikoMenuReturn,
    TaikoMenuRestart,
    TaikoMenuNewSong,
    TaikoMenuBackward,
    TaikoMenuForward,
    TaikoMenuDonVolume,
    TaikoMenuKaVolume,
    TaikoMenuExit,
    TaikoMenuItemCount,
} TaikoMenuItem;

typedef enum {
    TaikoMenuActionNone,
    TaikoMenuActionReturn,
    TaikoMenuActionRestart,
    TaikoMenuActionNewSong,
    TaikoMenuActionExit,
} TaikoMenuAction;

typedef struct {
    bool open;
    TaikoMenuItem selected;
    uint8_t backward_index;
    uint8_t forward_index;
    uint8_t don_volume;
    uint8_t ka_volume;
} TaikoMenu;

void taiko_menu_init(TaikoMenu* menu);
void taiko_menu_open(TaikoMenu* menu);
void taiko_menu_move(TaikoMenu* menu, bool down);
void taiko_menu_adjust(TaikoMenu* menu, bool forward);
TaikoMenuAction taiko_menu_activate(TaikoMenu* menu);
uint32_t taiko_menu_backward_ms(const TaikoMenu* menu);
uint32_t taiko_menu_forward_ms(const TaikoMenu* menu);
