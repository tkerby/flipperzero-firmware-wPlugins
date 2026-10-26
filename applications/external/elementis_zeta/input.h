#pragma once
// Input: held bitmask + edge-event queue fed by the RECORD_INPUT_EVENTS pubsub.
#include <stdint.h>
#include <stdbool.h>

// Key bits (held mask and InputEv.key)
enum {
    KEY_UP = 1,
    KEY_DOWN = 2,
    KEY_LEFT = 4,
    KEY_RIGHT = 8,
    KEY_OK = 16,
    KEY_BACK = 32,
};
#define KEY_DPAD (KEY_UP | KEY_DOWN | KEY_LEFT | KEY_RIGHT)

typedef enum {
    EV_PRESS,
    EV_RELEASE,
    EV_SHORT,
    EV_LONG,
    EV_REPEAT
} EvType;

typedef struct {
    uint8_t key; // one KEY_* bit
    uint8_t type; // EvType
} InputEv;

void input_init(void);
void input_deinit(void);

// Raw physical state (updated on Press/Release).
uint8_t input_held(void);
// Held keys minus the ones that must be ignored until released (set by input_flush()).
uint8_t input_held_effective(void);

// Scene change: drop queued events and ignore every key currently held until it is released
// (its Press/Short/Long/Repeat events are dropped; its Release clears the ignore).
void input_flush(void);

// Waits up to timeout_ms (furi_wait semantics: 0 = poll, 0xFFFFFFFF = forever) for the next
// edge event. Returns true and fills *ev, or false on timeout.
bool input_poll(InputEv* ev, uint32_t timeout_ms);
