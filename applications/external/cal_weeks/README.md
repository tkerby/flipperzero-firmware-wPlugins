# mitzi-cal-weeks
This **Calendar of week** is an application for the Flipper Zero that displays a weekly view of dates &mdash;  the current week along with the previous and next week. Today's date is highlighted with inverted colors. Monday is the first day of the week. On the left bottom, you can also see the week number of the current week.

<img alt="Flipper Zero screenshot"  src="screenshots/mitzi-cal-weeks-2025-12-07.png" width="40%" />

## Usage

- **OK Button** exits the application (for now)
- **Back Button (Long Press)** exits the application

## Background
We are using [Zeller's congruence algorithm](https://en.wikipedia.org/wiki/Zeller%27s_congruence) to calculate the day of the week for any given date. It properly handles:
- Month and year boundaries
- Leap years (including century rules)
- ISO 8601 week numbering

## Version history
See [changelog.md](changelog.md)
