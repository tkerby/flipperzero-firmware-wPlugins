#include "taiko_gpio.h"
#include <furi.h>
#include <furi_hal.h>
#include <stdio.h>

const GpioPin gpio_ext_pa7 = {.index = 0};
const GpioPin gpio_ext_pa6 = {.index = 1};
const GpioPin gpio_ext_pa4 = {.index = 2};
const GpioPin gpio_ext_pb3 = {.index = 3};

struct FuriTimer {
    FuriTimerCallback callback;
    void* context;
    uint32_t deadline;
    bool running;
    bool stop_pending;
};

static FuriTimer* timers[4];
static bool pins[4];
static GpioMode modes[4];
static unsigned rising_edges[4];
static uint32_t now;
static FuriTimer* queued_callback;

FuriTimer* furi_timer_alloc(FuriTimerCallback callback, FuriTimerType type, void* context) {
    assert(type == FuriTimerTypeOnce);
    FuriTimer* timer = calloc(1, sizeof(FuriTimer));
    assert(timer);
    timer->callback = callback;
    timer->context = context;
    for(unsigned i = 0; i < 4; i++) {
        if(!timers[i]) {
            timers[i] = timer;
            return timer;
        }
    }
    abort();
}

FuriStatus furi_timer_start(FuriTimer* timer, uint32_t ticks) {
    assert(ticks == 70);
    timer->deadline = now + ticks;
    timer->running = true;
    return FuriStatusOk;
}

FuriStatus furi_timer_stop(FuriTimer* timer) {
    timer->stop_pending = true;
    return FuriStatusOk;
}

void furi_timer_flush(void) {
    /* Simulate an expired callback ahead of the stop command in the daemon queue. */
    if(queued_callback) {
        FuriTimer* timer = queued_callback;
        queued_callback = NULL;
        timer->callback(timer->context);
    }
    for(unsigned i = 0; i < 4; i++) {
        if(timers[i] && timers[i]->stop_pending) {
            timers[i]->running = false;
            timers[i]->stop_pending = false;
        }
    }
}

void furi_timer_free(FuriTimer* timer) {
    assert(!timer->running);
    for(unsigned i = 0; i < 4; i++) {
        if(timers[i] == timer) timers[i] = NULL;
    }
    free(timer);
}

uint32_t furi_ms_to_ticks(uint32_t ms) {
    return ms;
}

void furi_hal_gpio_write(const GpioPin* pin, bool high) {
    if(high && !pins[pin->index]) rising_edges[pin->index]++;
    pins[pin->index] = high;
}

void furi_hal_gpio_init(const GpioPin* pin, GpioMode mode, GpioPull pull, GpioSpeed speed) {
    assert(pull == GpioPullDown && speed == GpioSpeedLow);
    modes[pin->index] = mode;
}

static void advance(uint32_t ms) {
    now += ms;
    for(unsigned i = 0; i < 4; i++) {
        FuriTimer* timer = timers[i];
        if(timer && timer->running && timer->deadline <= now) {
            timer->running = false;
            timer->callback(timer->context);
        }
    }
}

static void assert_low(void) {
    for(unsigned i = 0; i < 4; i++)
        assert(!pins[i]);
}

int main(void) {
    TaikoGpio* gpio = taiko_gpio_alloc();
    assert_low();
    for(unsigned i = 0; i < 4; i++) {
        assert(modes[i] == GpioModeOutputPushPull);
        bool right = i >= 2;
        bool rim = i == 0 || i == 3;
        taiko_gpio_pulse(gpio, right, rim);
        for(unsigned j = 0; j < 4; j++)
            assert(pins[j] == (i == j));
        advance(69);
        assert(pins[i]);
        advance(1);
        assert_low();
    }

    /* Different outputs retain their full pulse even when notes overlap. */
    taiko_gpio_pulse(gpio, true, false);
    advance(20);
    taiko_gpio_pulse(gpio, false, true);
    advance(50);
    assert(!pins[2] && pins[0]);
    advance(20);
    assert_low();

    /* Same output retriggers with another rising edge and a fresh 70 ms. */
    taiko_gpio_pulse(gpio, true, false);
    advance(50);
    unsigned edges = rising_edges[2];
    queued_callback = timers[2];
    taiko_gpio_pulse(gpio, true, false);
    assert(pins[2] && rising_edges[2] == edges + 1);
    advance(69);
    assert(pins[2]);
    advance(1);
    assert_low();

    /* Stop/exit cancels pending callbacks before lowering/releasing pins. */
    taiko_gpio_pulse(gpio, false, false);
    taiko_gpio_pulse(gpio, true, true);
    queued_callback = timers[1];
    taiko_gpio_reset(gpio);
    assert_low();
    advance(100);
    assert_low();
    taiko_gpio_pulse(gpio, true, true);
    taiko_gpio_free(gpio);
    assert_low();
    for(unsigned i = 0; i < 4; i++) {
        assert(modes[i] == GpioModeInput);
        assert(timers[i] == NULL);
    }
    puts("GPIO mapping, pulse timing, retrigger and cleanup tests passed");
    return 0;
}
