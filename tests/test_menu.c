#include "taiko_menu.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    TaikoMenu menu;
    taiko_menu_init(&menu);
    assert(!menu.open);
    assert(taiko_menu_backward_ms(&menu) == 5000);
    assert(taiko_menu_forward_ms(&menu) == 5000);
    assert(menu.don_volume == 70 && menu.ka_volume == 70);
    assert(taiko_menu_playback_tenths(&menu) == 10);
    assert(taiko_menu_scroll_tenths(&menu) == 10);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);

    taiko_menu_open(&menu);
    assert(menu.selected == TaikoMenuReturn);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuRestart);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuNewSong);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuDifficulty);
    menu.courses = (1U << TjaCourseEasy) | (1U << TjaCourseHard) | (1U << TjaCourseOni);
    taiko_menu_adjust(&menu, false);
    assert(menu.course == TjaCourseHard);
    taiko_menu_adjust(&menu, false);
    assert(menu.course == TjaCourseEasy);
    taiko_menu_adjust(&menu, false);
    assert(menu.course == TjaCourseOni);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuPlaybackSpeed);
    for(int i = 0; i < 5; i++)
        taiko_menu_adjust(&menu, false);
    for(uint8_t speed = 5; speed <= 20; speed++) {
        assert(taiko_menu_playback_tenths(&menu) == speed);
        assert(taiko_menu_scroll_tenths(&menu) == 10);
        taiko_menu_adjust(&menu, true);
    }
    assert(taiko_menu_playback_tenths(&menu) == 5);
    taiko_menu_adjust(&menu, false);
    assert(taiko_menu_playback_tenths(&menu) == 20);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuScrollSpeed);
    for(int i = 0; i < 5; i++)
        taiko_menu_adjust(&menu, false);
    assert(taiko_menu_scroll_tenths(&menu) == 5);
    const uint8_t scroll_expected[] = {5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                                       17, 18, 19, 20, 21, 22, 23, 24, 25, 30, 40};
    for(size_t i = 0; i < sizeof(scroll_expected); i++) {
        assert(taiko_menu_scroll_tenths(&menu) == scroll_expected[i]);
        assert(taiko_menu_playback_tenths(&menu) == 20);
        taiko_menu_adjust(&menu, true);
    }
    assert(taiko_menu_scroll_tenths(&menu) == 5);
    taiko_menu_adjust(&menu, false);
    assert(taiko_menu_scroll_tenths(&menu) == 40);
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
    assert(menu.selected == TaikoMenuDonVolume);
    taiko_menu_adjust(&menu, false);
    assert(menu.don_volume == 60 && menu.ka_volume == 70);
    for(int i = 0; i < 20; i++)
        taiko_menu_adjust(&menu, false);
    assert(menu.don_volume == 0);
    for(int i = 0; i < 20; i++)
        taiko_menu_adjust(&menu, true);
    assert(menu.don_volume == 100);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuKaVolume);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNone);
    assert(menu.open && menu.ka_volume == 80 && menu.don_volume == 100);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuExit);
    taiko_menu_adjust(&menu, true);
    assert(taiko_menu_forward_ms(&menu) == 1000);
    assert(menu.don_volume == 100 && menu.ka_volume == 80);
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
    taiko_menu_move(&menu, true);
    taiko_menu_move(&menu, true);
    assert(menu.selected == TaikoMenuNewSong);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionNewSong);
    assert(!menu.open);
    assert(taiko_menu_backward_ms(&menu) == 10000);
    assert(taiko_menu_forward_ms(&menu) == 1000);
    assert(menu.don_volume == 100 && menu.ka_volume == 80);
    taiko_menu_open(&menu);
    taiko_menu_move(&menu, false);
    assert(menu.selected == TaikoMenuExit);
    assert(taiko_menu_activate(&menu) == TaikoMenuActionExit);
    assert(!menu.open);
    taiko_menu_init(&menu);
    assert(taiko_menu_backward_ms(&menu) == 5000);
    assert(taiko_menu_forward_ms(&menu) == 5000);
    assert(menu.don_volume == 70 && menu.ka_volume == 70);
    assert(taiko_menu_playback_tenths(&menu) == 10 && taiko_menu_scroll_tenths(&menu) == 10);
    puts(
        "Menu actions, difficulty, independent speeds, intervals, volume limits and defaults tests passed");
    return 0;
}
