// Planetary hours: whose hour it is now, and until when.
//
// From sunrise the hours belong to Saturn, Jupiter, Mars, the Sun, Venus,
// Mercury and the Moon in rotation (the Chaldean order). The day's first hour
// names the day. Day and night hours are unequal: each is a twelfth of the
// daylight or of the dark. Agrippa, Three Books of Occult Philosophy II.34.
//
// Pure C, no Flipper dependencies, so it can be checked on a host.
#pragma once

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    PlanetSaturn,
    PlanetJupiter,
    PlanetMars,
    PlanetSun,
    PlanetVenus,
    PlanetMercury,
    PlanetMoon,
    PlanetCount,
} Planet;

typedef struct {
    float latitude; // degrees, north positive
    float longitude; // degrees, east positive
    int16_t utc_offset; // standard time, minutes east of UTC
    bool us_dst; // apply the United States daylight saving rule
} HoursPlace;

typedef struct {
    int16_t year;
    int8_t month; // 1..12
    int8_t day; // 1..31
    int16_t minute; // minutes since local midnight, 0..1439
} HoursTime;

typedef struct {
    Planet planet;
    Planet day_ruler;
    bool is_day;
    uint8_t index; // 1..12 within the day or the night
    int16_t start; // minutes since the planetary day's local midnight; may exceed 1439 at night
    int16_t end;
    int16_t sunrise; // of the planetary day
    int16_t sunset;
    int16_t next_sunrise; // minutes past 1440
    int8_t weekday; // 0 = Sunday, of the planetary day
} PlanetaryHour;

extern const char* const planet_names[PlanetCount];

// Sunrise and sunset in local minutes for the civil date. Returns false in
// polar day or night, where there are no planetary hours to speak of.
bool hours_sun(
    const HoursPlace* place,
    int16_t year,
    int8_t month,
    int8_t day,
    int16_t* sunrise,
    int16_t* sunset);

// The planetary hour at a local time. False when the sun does not rise or set.
bool hours_now(const HoursPlace* place, const HoursTime* when, PlanetaryHour* out);

// The planet of the n-th hour (0..23 from sunrise) of a planetary day with the given ruler.
Planet hours_planet_of(Planet day_ruler, uint8_t n);

// 0 = Sunday
int8_t hours_weekday(int16_t year, int8_t month, int8_t day);
