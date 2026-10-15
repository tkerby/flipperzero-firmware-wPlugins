#include "taiko_gpio.h"
#include <furi.h>
#include <furi_hal.h>
#include <stdio.h>

const GpioPin gpio_ext_pa7 = {.index = 0};
const GpioPin gpio_ext_pa6 = {.index = 1};
const GpioPin gpio_ext_pa4 = {.index = 2};
const GpioPin gpio_ext_pb3 = {.index = 3};
const GpioPin gpio_ext_pb2 = {.index = 4};
const GpioPin gpio_ext_pc3 = {.index = 5};
const GpioPin gpio_ext_pc1 = {.index = 6};
const GpioPin gpio_ext_pc0 = {.index = 7};

struct FuriTimer {
    FuriTimerCallback callback;
    void* context;
    uint32_t deadline;
    bool running;
    bool stop_pending;
};

static FuriTimer* timers[4];
static bool pins[TaikoGpioPinCount];
static GpioMode modes[TaikoGpioPinCount];
static unsigned rising_edges[TaikoGpioPinCount];
static unsigned writes;
static unsigned inits;
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
    writes++;
    if(high) assert(modes[pin->index] == GpioModeOutputPushPull);
    if(high && !pins[pin->index]) rising_edges[pin->index]++;
    pins[pin->index] = high;
}

void furi_hal_gpio_init(const GpioPin* pin, GpioMode mode, GpioPull pull, GpioSpeed speed) {
    inits++;
    assert(pull == GpioPullDown && speed == GpioSpeedLow);
    assert(!pins[pin->index]);
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
    for(unsigned i = 0; i < TaikoGpioPinCount; i++)
        assert(!pins[i]);
}

int main(void) {
    TaikoGpio* gpio = taiko_gpio_alloc();
    assert_low();
    taiko_gpio_pulse(gpio, true, false);
    taiko_gpio_reset(gpio);
    assert(writes == 0 && inits == 0); /* Disabled startup does not claim pins. */
    taiko_gpio_free(gpio);
    assert(writes == 0 && inits == 0);
    gpio = taiko_gpio_alloc();
    TaikoGpioConfig config = taiko_gpio_default_config();
    assert(!config.enabled);
    config.pins[TaikoGpioLeftKa] = TaikoGpioPinC0;
    assert(taiko_gpio_configure(gpio, &config));
    assert(writes == 0 && inits == 0); /* Can edit mappings while disabled. */
    config = taiko_gpio_default_config();
    config.enabled = true;
    assert(taiko_gpio_configure(gpio, &config));
    unsigned previous_inits = inits, previous_writes = writes;
    assert(taiko_gpio_configure(gpio, &config));
    assert(inits == previous_inits && writes == previous_writes);
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

    /* Remapping drains callbacks while they still refer to the old pin. */
    gpio = taiko_gpio_alloc();
    assert(taiko_gpio_configure(gpio, &config));
    taiko_gpio_pulse(gpio, true, false);
    queued_callback = timers[TaikoGpioRightDon];
    config.pins[TaikoGpioRightDon] = TaikoGpioPinC1;
    assert(taiko_gpio_configure(gpio, &config));
    assert_low();
    assert(modes[TaikoGpioPinA4] == GpioModeInput);
    assert(modes[TaikoGpioPinC1] == GpioModeOutputPushPull);
    taiko_gpio_pulse(gpio, true, false);
    assert(pins[TaikoGpioPinC1] && !pins[TaikoGpioPinA4]);
    advance(70);
    assert_low();

    TaikoGpioConfig invalid = config;
    invalid.pins[TaikoGpioLeftKa] = config.pins[TaikoGpioRightDon];
    assert(!taiko_gpio_configure(gpio, &invalid));
    invalid = config;
    invalid.pins[TaikoGpioRightKa] = TaikoGpioPinCount;
    assert(!taiko_gpio_configure(gpio, &invalid));
    taiko_gpio_pulse(gpio, true, false);
    assert(pins[TaikoGpioPinC1]);

    queued_callback = timers[TaikoGpioRightDon];
    config.enabled = false;
    assert(taiko_gpio_configure(gpio, &config));
    assert_low();
    for(unsigned i = 0; i < TaikoGpioPinCount; i++)
        assert(modes[i] == GpioModeInput);
    previous_writes = writes;
    taiko_gpio_pulse(gpio, true, false);
    advance(100);
    taiko_gpio_free(gpio);
    assert(writes == previous_writes);
    assert_low();

    /* Exercise the physical mapping for every remaining selectable pin. */
    gpio = taiko_gpio_alloc();
    for(unsigned pin = 0; pin < TaikoGpioPinCount; pin++) {
        if(pin == TaikoGpioPinA7 || pin == TaikoGpioPinA6 || pin == TaikoGpioPinB3) continue;
        config.pins[TaikoGpioRightDon] = pin;
        config.enabled = true;
        assert(taiko_gpio_configure(gpio, &config));
        taiko_gpio_pulse(gpio, true, false);
        for(unsigned i = 0; i < TaikoGpioPinCount; i++)
            assert(pins[i] == (i == pin));
        config.enabled = false;
        assert(taiko_gpio_configure(gpio, &config));
        assert_low();
    }
    taiko_gpio_free(gpio);
    puts(
        "GPIO defaults, enable/disable, remapping, pulse timing, retrigger and cleanup tests passed");
    return 0;
}
