#include "hours.h"
#include <math.h>

const char* const planet_names[PlanetCount] =
    {"SATURN", "JUPITER", "MARS", "SUN", "VENUS", "MERCURY", "MOON"};

// Sunday..Saturday -> the planet that rules the day
static const Planet day_rulers[7] =
    {PlanetSun, PlanetMoon, PlanetMars, PlanetMercury, PlanetJupiter, PlanetVenus, PlanetSaturn};

static bool leap(int16_t y) {
    return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
}

static int8_t days_in_month(int16_t y, int8_t m) {
    static const int8_t d[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && leap(y)) ? 29 : d[m - 1];
}

static int16_t day_of_year(int16_t y, int8_t m, int8_t d) {
    int16_t n = d;
    for(int8_t i = 1; i < m; i++)
        n += days_in_month(y, i);
    return n;
}

int8_t hours_weekday(int16_t y, int8_t m, int8_t d) {
    // Zeller, shifted so that 0 is Sunday
    if(m < 3) {
        m += 12;
        y--;
    }
    int k = y % 100, j = y / 100;
    int h = (d + 13 * (m + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7; // 0 = Saturday
    return (int8_t)((h + 6) % 7);
}

static void step_day(int16_t* y, int8_t* m, int8_t* d, int dir) {
    if(dir > 0) {
        if(++*d > days_in_month(*y, *m)) {
            *d = 1;
            if(++*m > 12) {
                *m = 1;
                (*y)++;
            }
        }
    } else {
        if(--*d < 1) {
            if(--*m < 1) {
                *m = 12;
                (*y)--;
            }
            *d = days_in_month(*y, *m);
        }
    }
}

// United States rule: second Sunday in March to first Sunday in November.
// The switch happens at 02:00 local, which we ignore: the day is in or out.
static bool us_dst_active(int16_t y, int8_t m, int8_t d) {
    if(m < 3 || m > 11) return false;
    if(m > 3 && m < 11) return true;
    int8_t first_sunday = (int8_t)((7 - hours_weekday(y, m, 1)) % 7 + 1);
    if(m == 3) return d >= first_sunday + 7;
    return d < first_sunday;
}

bool hours_sun(
    const HoursPlace* place,
    int16_t y,
    int8_t m,
    int8_t d,
    int16_t* sunrise,
    int16_t* sunset) {
    // NOAA's solar calculator, at local noon
    float days = leap(y) ? 366.0f : 365.0f;
    float g = 2.0f * (float)M_PI / days * (day_of_year(y, m, d) - 1 + 0.5f);
    float eqtime = 229.18f * (0.000075f + 0.001868f * cosf(g) - 0.032077f * sinf(g) -
                              0.014615f * cosf(2 * g) - 0.040849f * sinf(2 * g));
    float decl = 0.006918f - 0.399912f * cosf(g) + 0.070257f * sinf(g) - 0.006758f * cosf(2 * g) +
                 0.000907f * sinf(2 * g) - 0.002697f * cosf(3 * g) + 0.00148f * sinf(3 * g);
    float lat = place->latitude * (float)M_PI / 180.0f;
    float zenith = 90.833f * (float)M_PI / 180.0f; // the upper limb, with refraction
    float cos_ha = cosf(zenith) / (cosf(lat) * cosf(decl)) - tanf(lat) * tanf(decl);
    if(cos_ha > 1.0f || cos_ha < -1.0f) return false;
    float ha = acosf(cos_ha) * 180.0f / (float)M_PI;
    int16_t offset = place->utc_offset + ((place->us_dst && us_dst_active(y, m, d)) ? 60 : 0);
    *sunrise = (int16_t)lroundf(720.0f - 4.0f * (place->longitude + ha) - eqtime) + offset;
    *sunset = (int16_t)lroundf(720.0f - 4.0f * (place->longitude - ha) - eqtime) + offset;
    return true;
}

Planet hours_planet_of(Planet day_ruler, uint8_t n) {
    return (Planet)((day_ruler + n) % PlanetCount);
}

bool hours_now(const HoursPlace* place, const HoursTime* when, PlanetaryHour* out) {
    int16_t y = when->year;
    int8_t m = when->month, d = when->day;
    int16_t t = when->minute;
    int16_t rise, set;
    if(!hours_sun(place, y, m, d, &rise, &set)) return false;
    if(t < rise) {
        // still the night of the previous planetary day
        step_day(&y, &m, &d, -1);
        int16_t r0, s0;
        if(!hours_sun(place, y, m, d, &r0, &s0)) return false;
        t += 1440;
        out->next_sunrise = rise + 1440;
        rise = r0;
        set = s0;
    } else {
        int16_t y1 = y;
        int8_t m1 = m, d1 = d;
        step_day(&y1, &m1, &d1, 1);
        int16_t r1, s1;
        if(!hours_sun(place, y1, m1, d1, &r1, &s1)) return false;
        out->next_sunrise = r1 + 1440;
    }
    out->weekday = hours_weekday(y, m, d);
    out->day_ruler = day_rulers[out->weekday];
    out->sunrise = rise;
    out->sunset = set;
    uint8_t n;
    if(t < set) {
        float len = (set - rise) / 12.0f;
        n = (uint8_t)((t - rise) / len);
        if(n > 11) n = 11;
        out->is_day = true;
        out->index = n + 1;
        out->start = rise + (int16_t)(n * len);
        out->end = rise + (int16_t)((n + 1) * len);
    } else {
        float len = (out->next_sunrise - set) / 12.0f;
        n = (uint8_t)((t - set) / len);
        if(n > 11) n = 11;
        out->is_day = false;
        out->index = n + 1;
        out->start = set + (int16_t)(n * len);
        out->end = set + (int16_t)((n + 1) * len);
        n += 12;
    }
    out->planet = hours_planet_of(out->day_ruler, n);
    return true;
}
