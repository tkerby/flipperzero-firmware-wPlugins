#include "taiko_settings.h"

#include <assert.h>
#include <stdio.h>

int main(void) {
    TaikoSettings defaults, settings, captured;
    taiko_settings_defaults(&defaults);
    assert(defaults.volume == 70 && defaults.don_volume == 70 && defaults.ka_volume == 70);
    assert(defaults.backward_ms == 5000 && defaults.forward_ms == 5000);
    assert(defaults.playback_tenths == 10 && defaults.scroll_tenths == 10);
    assert(taiko_settings_validate(&defaults));

    settings = defaults;
    settings.volume = 40;
    settings.don_volume = 0;
    settings.ka_volume = 100;
    settings.backward_ms = 1000;
    settings.forward_ms = 10000;
    settings.playback_tenths = 7;
    settings.scroll_tenths = 30;
    settings.pins[TaikoGpioLeftKa] = TaikoGpioPinC0;
    assert(taiko_settings_validate(&settings));
    TaikoMenu menu;
    taiko_menu_init(&menu);
    menu.open = true;
    menu.selected = TaikoMenuRestoreDefaults;
    menu.course = TjaCourseHard;
    menu.courses = (1U << TjaCourseHard) | (1U << TjaCourseOni);
    menu.gpio.enabled = true;
    uint8_t volume;
    taiko_settings_apply(&settings, &menu, &volume);
    assert(volume == 40 && !menu.gpio.enabled);
    assert(menu.open && menu.selected == TaikoMenuRestoreDefaults);
    assert(
        menu.course == TjaCourseHard &&
        menu.courses == ((1U << TjaCourseHard) | (1U << TjaCourseOni)));
    assert(taiko_menu_backward_ms(&menu) == 1000 && taiko_menu_forward_ms(&menu) == 10000);
    assert(taiko_menu_playback_tenths(&menu) == 7 && taiko_menu_scroll_tenths(&menu) == 30);
    taiko_settings_capture(&captured, &menu, volume);
    assert(taiko_settings_equal(&settings, &captured));
    menu.gpio.enabled = true;
    menu.course = TjaCourseEasy;
    menu.selected = TaikoMenuNewSong;
    taiko_settings_capture(&captured, &menu, volume);
    assert(taiko_settings_equal(&settings, &captured)); /* Transient state is excluded. */

    taiko_settings_apply(&defaults, &menu, &volume);
    taiko_settings_capture(&captured, &menu, volume);
    assert(taiko_settings_equal(&defaults, &captured));
    assert(menu.open && menu.course == TjaCourseEasy && menu.selected == TaikoMenuNewSong);
    assert(!menu.gpio.enabled);

    /* Bad values default independently, leaving valid custom settings intact. */
    settings = defaults;
    settings.volume = UINT32_MAX;
    settings.don_volume = 75;
    settings.ka_volume = 30;
    settings.playback_tenths = 21;
    settings.scroll_tenths = 26;
    settings.backward_ms = 4000;
    settings.forward_ms = 1000;
    settings.pins[TaikoGpioLeftKa] = TaikoGpioPinB3;
    assert(!taiko_settings_validate(&settings));
    assert(settings.volume == 70 && settings.don_volume == 70 && settings.ka_volume == 30);
    assert(settings.playback_tenths == 10 && settings.scroll_tenths == 10);
    assert(settings.backward_ms == 5000 && settings.forward_ms == 1000);
    for(unsigned i = 0; i < TaikoGpioOutputCount; i++)
        assert(settings.pins[i] == defaults.pins[i]);
    settings.pins[TaikoGpioRightKa] = TaikoGpioPinCount;
    assert(!taiko_settings_validate(&settings));
    assert(settings.pins[TaikoGpioRightKa] == TaikoGpioPinB3);

    /* Every selectable speed survives a save/load through actual values. */
    taiko_menu_init(&menu);
    taiko_menu_open(&menu);
    const TaikoMenuItem speeds[] = {TaikoMenuPlaybackSpeed, TaikoMenuScrollSpeed};
    for(unsigned i = 0; i < 2; i++) {
        menu.selected = speeds[i];
        for(unsigned step = 0; step < 24; step++) {
            taiko_menu_adjust(&menu, true);
            taiko_settings_capture(&settings, &menu, 70);
            assert(taiko_settings_validate(&settings));
            TaikoMenu loaded;
            taiko_menu_init(&loaded);
            taiko_settings_apply(&settings, &loaded, &volume);
            taiko_settings_capture(&captured, &loaded, volume);
            assert(taiko_settings_equal(&settings, &captured));
        }
    }

    uint32_t number = 123;
    assert(taiko_settings_parse_uint(" 4294967295 \r", &number) && number == UINT32_MAX);
    assert(taiko_settings_parse_uint("0", &number) && number == 0);
    const char* invalid[] = {"", " ", "-1", "+1", "70oops", "70 80", "1.0", "4294967296"};
    for(unsigned i = 0; i < sizeof(invalid) / sizeof(invalid[0]); i++) {
        number = 123;
        assert(!taiko_settings_parse_uint(invalid[i], &number) && number == 123);
    }
    for(unsigned i = 0; i < TaikoGpioPinCount; i++) {
        TaikoGpioPin pin;
        assert(taiko_settings_parse_pin(taiko_gpio_pin_name(i), &pin) && pin == (TaikoGpioPin)i);
    }
    TaikoGpioPin pin = TaikoGpioPinA7;
    assert(!taiko_settings_parse_pin("5V", &pin) && pin == TaikoGpioPinA7);
    puts(
        "Settings round-trip, defaults, validation, GPIO-off policy and stable values tests passed");
    return 0;
}
