#include "taiko_menu.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    TaikoMenu menu;
    taiko_menu_init(&menu);
    assert(!menu.open);
    assert(taiko_menu_backward_ms(&menu) == 5000);
    assert(taiko_menu_forward_ms(&menu) == 5000);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);

    taiko_menu_open(&menu);
    assert(menu.selected == TaikoMenuReturn);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuRestart);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuBackward);
    taiko_menu_adjust(&menu, false);
    assert(taiko_menu_backward_ms(&menu) == 3000);
    taiko_menu_adjust(&menu, false);
    assert(taiko_menu_backward_ms(&menu) == 1000);
    taiko_menu_adjust(&menu, false);
    assert(taiko_menu_backward_ms(&menu) == 10000);
    assert(taiko_menu_forward_ms(&menu) == 5000);

    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuForward);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);
    assert(menu.open);
    assert(taiko_menu_forward_ms(&menu) == 10000);
    taiko_menu_adjust(&menu, true);
    assert(taiko_menu_forward_ms(&menu) == 1000);
    assert(taiko_menu_backward_ms(&menu) == 10000);

    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuExit);
    taiko_menu_adjust(&menu, true);
    assert(taiko_menu_forward_ms(&menu) == 1000);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuReturn);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionReturn);
    assert(!menu.open);

    taiko_menu_open(&menu);
    assert(menu.selected == TaikoMenuReturn);
    assert(taiko_menu_backward_ms(&menu) == 10000);
    assert(taiko_menu_forward_ms(&menu) == 1000);
    taiko_menu_move(&menu, true);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionRestart);
    assert(!menu.open);
    taiko_menu_open(&menu);
    taiko_menu_move(&menu, false);
    assert(menu.selected == TaikoMenuExit);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionExit);
    assert(!menu.open);
    taiko_menu_init(&menu);
    assert(taiko_menu_backward_ms(&menu) == 5000);
    assert(taiko_menu_forward_ms(&menu) == 5000);
    puts("Menu order, actions, independent intervals and defaults tests passed");
    return 0;
}
