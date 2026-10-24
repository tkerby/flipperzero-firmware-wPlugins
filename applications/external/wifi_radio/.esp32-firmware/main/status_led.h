#pragma once
// Front-panel status LED.
//
// The board spends its life plugged into a Flipper with no screen and no
// console of its own, so the LED is the only thing that says it is alive. It
// is not decoration: a board that shows nothing is indistinguishable from a
// board that is broken.
void status_led_start(void);
