#include "input.h"
#include "config.h"
#include <furi.h>
#include <input/input.h>

static volatile uint8_t held;
static volatile uint8_t ignore;
static FuriMessageQueue* queue;
static FuriPubSub* pubsub;
static FuriPubSubSubscription* sub;

static uint8_t key_bit(InputKey k) {
    switch(k) {
    case InputKeyUp: return KEY_UP;
    case InputKeyDown: return KEY_DOWN;
    case InputKeyLeft: return KEY_LEFT;
    case InputKeyRight: return KEY_RIGHT;
    case InputKeyOk: return KEY_OK;
    case InputKeyBack: return KEY_BACK;
    default: return 0;
    }
}

static int type_of(InputType t) {
    switch(t) {
    case InputTypePress: return EV_PRESS;
    case InputTypeRelease: return EV_RELEASE;
    case InputTypeShort: return EV_SHORT;
    case InputTypeLong: return EV_LONG;
    case InputTypeRepeat: return EV_REPEAT;
    default: return -1;
    }
}

static void input_cb(const void* value, void* ctx) {
    UNUSED(ctx);
    const InputEvent* e = value;
    uint8_t bit = key_bit(e->key);
    int type = type_of(e->type);
    if(!bit || type < 0) return;

    FURI_CRITICAL_ENTER();
    uint8_t ign = ignore;
    if(type == EV_PRESS) held |= bit;
    if(type == EV_RELEASE) {
        held &= (uint8_t)~bit;
        ignore &= (uint8_t)~bit;
    }
    FURI_CRITICAL_EXIT();

    if(type != EV_RELEASE && (ign & bit)) return; // key held across a scene change
    InputEv ev = {.key = bit, .type = (uint8_t)type};
    furi_message_queue_put(queue, &ev, 0); // drop when full; `held` stays correct
}

void input_init(void) {
    held = 0;
    ignore = 0;
    queue = furi_message_queue_alloc(INPUT_QUEUE_LEN, sizeof(InputEv));
    pubsub = furi_record_open(RECORD_INPUT_EVENTS);
    sub = furi_pubsub_subscribe(pubsub, input_cb, NULL);
}

void input_deinit(void) {
    furi_pubsub_unsubscribe(pubsub, sub);
    furi_record_close(RECORD_INPUT_EVENTS);
    furi_message_queue_free(queue);
    queue = NULL;
}

uint8_t input_held(void) {
    return held;
}

uint8_t input_held_effective(void) {
    return (uint8_t)(held & ~ignore);
}

void input_flush(void) {
    FURI_CRITICAL_ENTER();
    ignore |= held;
    FURI_CRITICAL_EXIT();
    furi_message_queue_reset(queue);
}

bool input_poll(InputEv* ev, uint32_t timeout_ms) {
    return furi_message_queue_get(queue, ev, timeout_ms) == FuriStatusOk;
}
