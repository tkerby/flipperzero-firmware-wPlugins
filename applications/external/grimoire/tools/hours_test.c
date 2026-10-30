// Host check for hours.c against published Portland times (timeanddate, NOAA), tolerance 3 min.
#include "../hours.h"
#include <stdio.h>
#include <stdlib.h>

static const HoursPlace portland = {45.52f, -122.68f, -8 * 60, true};
static int fails = 0;

static void expect_sun(int y, int m, int d, int rise, int set) {
    int16_t r, s;
    hours_sun(&portland, y, m, d, &r, &s);
    int ok = abs(r - rise) <= 3 && abs(s - set) <= 3;
    printf(
        "%s %04d-%02d-%02d sunrise %02d:%02d (want %02d:%02d) sunset %02d:%02d (want %02d:%02d)\n",
        ok ? "ok  " : "FAIL",
        y,
        m,
        d,
        r / 60,
        r % 60,
        rise / 60,
        rise % 60,
        s / 60,
        s % 60,
        set / 60,
        set % 60);
    if(!ok) fails++;
}

static void expect_hour(int y, int m, int d, int minute, Planet planet, int is_day, int index) {
    HoursTime t = {y, m, d, minute};
    PlanetaryHour h;
    hours_now(&portland, &t, &h);
    int ok = h.planet == planet && h.is_day == is_day && h.index == index;
    printf(
        "%s %04d-%02d-%02d %02d:%02d -> %s %s %d (want %s %s %d) ends %02d:%02d\n",
        ok ? "ok  " : "FAIL",
        y,
        m,
        d,
        minute / 60,
        minute % 60,
        planet_names[h.planet],
        h.is_day ? "day" : "night",
        h.index,
        planet_names[planet],
        is_day ? "day" : "night",
        index,
        (h.end % 1440) / 60,
        h.end % 60);
    if(!ok) fails++;
}

int main(void) {
    expect_sun(2026, 10, 26, 7 * 60 + 40, 18 * 60 + 8); // PDT
    expect_sun(2026, 6, 21, 5 * 60 + 22, 21 * 60 + 3); // PDT
    expect_sun(2026, 12, 21, 7 * 60 + 48, 16 * 60 + 30); // PST
    expect_sun(2026, 11, 2, 6 * 60 + 51, 16 * 60 + 57); // PST, the day after the change
    // 2026-10-26 is a Monday: Moon rules; first hour from sunrise is the Moon's
    printf("weekday 2026-10-26 = %d (want 1)\n", hours_weekday(2026, 10, 26));
    expect_hour(2026, 10, 26, 7 * 60 + 50, PlanetMoon, 1, 1);
    expect_hour(2026, 10, 26, 8 * 60 + 40, PlanetSaturn, 1, 2);
    expect_hour(2026, 10, 26, 18 * 60 + 10, PlanetVenus, 0, 1); // 13th hour: Moon+12 = Venus
    // before sunrise on Tuesday the 27th it is still Monday's night: 06:30 falls in the 23rd hour, Moon+22 = Saturn
    expect_hour(2026, 10, 27, 6 * 60 + 30, PlanetSaturn, 0, 11);
    // a Sunday noon: Sun rules; 12:00 is the 5th hour: Sun, Venus, Mercury, Moon, Saturn
    printf("weekday 2026-10-25 = %d (want 0)\n", hours_weekday(2026, 10, 25));
    expect_hour(2026, 10, 25, 12 * 60, PlanetSaturn, 1, 5);
    printf("%s\n", fails ? "SOME CHECKS FAILED" : "all checks passed");
    return fails != 0;
}
