# Persian Calendar for Flipper Zero

A Persian (Jalali / Solar Hijri) calendar app for the Flipper Zero. Shows today's date
in both the Persian and Gregorian calendars, converts dates between the two, and
displays the day of the week in Persian and English.

## Features

- **Today view** — opens showing today's date in the Persian calendar (big), the
  Gregorian calendar, and the weekday in Persian transliteration and English,
  e.g. `Seshanbeh (Tue)`. Rolls over automatically at midnight.
- **Date converter** — Gregorian → Persian and Persian → Gregorian, with the weekday
  of the converted date.
- **Set today's date** — writes the date to the Flipper's real-time clock
  (time of day is left untouched).
- **Shahanshahi (Imperial) calendar option** — in Settings, switch the Persian year
  display between Solar Hijri (e.g. 1405) and Shahanshahi (e.g. 2585). The choice is
  saved and restored across launches.

## Controls

| Screen    | Key         | Action                                  |
|-----------|-------------|-----------------------------------------|
| Today     | OK          | Open menu                               |
| Today     | Back        | Exit app                                |
| Menu      | Up / Down   | Select item                             |
| Menu      | OK          | Open item                               |
| Editor    | Left / Right| Select field (day / month / year)       |
| Editor    | Up / Down   | Change value (hold to repeat)           |
| Editor    | OK          | Convert / set date                      |
| Settings  | Left / Right / OK | Toggle Solar Hijri / Shahanshahi  |
| Settings  | Back        | Save and return                         |

## Accuracy

Conversion uses the arithmetic 33-year-cycle (jdf) algorithm. The implementation is
verified by an exhaustive host-side test that walks every day from 1800 to 2200
(146,462 days) in both directions, plus known anchors such as Nowruz and leap-year
Esfand 30.

## Building

Requires [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
ufbt        # build; output in dist/persian_calendar.fap
ufbt launch # build, install and run on a connected Flipper
```

## Hardware

No external hardware required. Works on any Flipper Zero.

## License

MIT — see [LICENSE](LICENSE).
