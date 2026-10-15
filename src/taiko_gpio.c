#include "taiko_gpio.h"

#include <furi.h>
#include <furi_hal.h>

typedef struct {
    const GpioPin* pin;
    FuriTimer* timer;
} DrumOutput;

struct TaikoGpio {
    DrumOutput outputs[4];
};

static void pulse_end(void* context) {
    DrumOutput* output = context;
    furi_hal_gpio_write(output->pin, false);
}

TaikoGpio* taiko_gpio_alloc(void) {
    TaikoGpio* gpio = malloc(sizeof(TaikoGpio));
    const GpioPin* pins[] = {&gpio_ext_pa7, &gpio_ext_pa6, &gpio_ext_pa4, &gpio_ext_pb3};
    for(size_t i = 0; i < COUNT_OF(gpio->outputs); i++) {
        DrumOutput* output = &gpio->outputs[i];
        output->pin = pins[i];
        output->timer = furi_timer_alloc(pulse_end, FuriTimerTypeOnce, output);
        /* Set the output latch before switching modes to avoid a startup high. */
        furi_hal_gpio_write(output->pin, false);
        furi_hal_gpio_init(output->pin, GpioModeOutputPushPull, GpioPullDown, GpioSpeedLow);
    }
    return gpio;
}

void taiko_gpio_pulse(TaikoGpio* gpio, bool right, bool rim) {
    size_t index = right ? (rim ? 3 : 2) : (rim ? 0 : 1);
    DrumOutput* output = &gpio->outputs[index];
    furi_timer_stop(output->timer);
    /* stop() can leave a command/callback queued in the timer service. Drain it
     * before raising this pin so an old callback cannot cut a new pulse short. */
    furi_timer_flush();
    furi_hal_gpio_write(output->pin, false);
    furi_hal_gpio_write(output->pin, true);
    furi_check(furi_timer_start(output->timer, furi_ms_to_ticks(TAIKO_HIT_MS)) == FuriStatusOk);
}

void taiko_gpio_reset(TaikoGpio* gpio) {
    for(size_t i = 0; i < COUNT_OF(gpio->outputs); i++)
        furi_timer_stop(gpio->outputs[i].timer);
    furi_timer_flush();
    for(size_t i = 0; i < COUNT_OF(gpio->outputs); i++)
        furi_hal_gpio_write(gpio->outputs[i].pin, false);
}

void taiko_gpio_free(TaikoGpio* gpio) {
    taiko_gpio_reset(gpio);
    for(size_t i = 0; i < COUNT_OF(gpio->outputs); i++) {
        DrumOutput* output = &gpio->outputs[i];
        furi_timer_free(output->timer);
        /* Release the pin as an input with a pull-down to keep idle low. */
        furi_hal_gpio_init(output->pin, GpioModeInput, GpioPullDown, GpioSpeedLow);
    }
    free(gpio);
}
