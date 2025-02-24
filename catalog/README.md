# Blinker

A Flipper Zero application that blinks LEDs with a decreasing frequency over time. Unlike traditional Pomodoro timers, it provides visual feedback through LED blinks that gradually slow down.

## Application Features

### Main screen
* **Menu:** available options
    * **Int.** - configuration of max and min intervals using the number picker screen
    * **Dur.** - configuration of duration of execution using the number picker screen
    * **Flash** - start blinking the LED light and move to execution screen

### Number picker screen
* **Header:** selected mode, options are max interval, min interval and duration. Header also includes unit, beats per minute or BPM for intervals and minutes for duration.
* **Number picker:** keyboard with buffer to choose a number in correct constrains. 1 - 200 for intervals and 1-60 for duration.

### Execution screen

### Configuration

The following parameters can be configured:
* `Max interval`: Starting cadence in beats per minute (BPM)
* `Min interval`: Target ending cadence in BPM
* `Duration`: Time period (in minutes) over which the cadence gradually changes from max to min

### Operation

When running, all three LEDs (red, green, and blue) blink simultaneously, creating a white flash. The display shows the current BPM, which gradually decreases from max to min interval over the set duration.