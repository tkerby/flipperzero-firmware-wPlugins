#include "taiko_gpio.h"

#include <furi.h>
#include <furi_hal.h>

typedef struct {
    const GpioPin* pin;
    FuriTimer* timer;
} DrumOutput;

struct TaikoGpio {
    DrumOutput outputs[TaikoGpioOutputCount];
    TaikoGpioConfig config;
};

static const GpioPin* const pins[] = {
    &gpio_ext_pa7,
    &gpio_ext_pa6,
    &gpio_ext_pa4,
    &gpio_ext_pb3,
    &gpio_ext_pb2,
    &gpio_ext_pc3,
    &gpio_ext_pc1,
    &gpio_ext_pc0};

static void pulse_end(void* context) {
    DrumOutput* output = context;
    furi_hal_gpio_write(output->pin, false);
}

TaikoGpio* taiko_gpio_alloc(void) {
    TaikoGpio* gpio = malloc(sizeof(TaikoGpio));
    gpio->config = taiko_gpio_default_config();
    for(size_t i = 0; i < COUNT_OF(gpio->outputs); i++) {
        DrumOutput* output = &gpio->outputs[i];
        output->pin = pins[gpio->config.pins[i]];
        output->timer = furi_timer_alloc(pulse_end, FuriTimerTypeOnce, output);
    }
    return gpio;
}

bool taiko_gpio_configure(TaikoGpio* gpio, const TaikoGpioConfig* config) {
    if(!taiko_gpio_config_valid(config)) return false;
    bool changed = config->enabled != gpio->config.enabled;
    for(size_t i = 0; i < COUNT_OF(gpio->outputs); i++)
        if(config->pins[i] != gpio->config.pins[i]) changed = true;
    if(!changed) return true;

    taiko_gpio_reset(gpio);
    if(gpio->config.enabled) {
        for(size_t i = 0; i < COUNT_OF(gpio->outputs); i++)
            furi_hal_gpio_init(gpio->outputs[i].pin, GpioModeInput, GpioPullDown, GpioSpeedLow);
    }
    gpio->config = *config;
    for(size_t i = 0; i < COUNT_OF(gpio->outputs); i++) {
        DrumOutput* output = &gpio->outputs[i];
        output->pin = pins[config->pins[i]];
        if(config->enabled) {
            /* Set the latch before switching modes to avoid a startup high. */
            furi_hal_gpio_write(output->pin, false);
            furi_hal_gpio_init(output->pin, GpioModeOutputPushPull, GpioPullDown, GpioSpeedLow);
        }
    }
    return true;
}

void taiko_gpio_pulse(TaikoGpio* gpio, bool right, bool rim) {
    if(!gpio->config.enabled) return;
    size_t index = right ? (rim ? TaikoGpioRightKa : TaikoGpioRightDon) :
                           (rim ? TaikoGpioLeftKa : TaikoGpioLeftDon);
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
    if(!gpio->config.enabled) return;
    for(size_t i = 0; i < COUNT_OF(gpio->outputs); i++)
        furi_timer_stop(gpio->outputs[i].timer);
    furi_timer_flush();
    for(size_t i = 0; i < COUNT_OF(gpio->outputs); i++)
        furi_hal_gpio_write(gpio->outputs[i].pin, false);
}

void taiko_gpio_free(TaikoGpio* gpio) {
    TaikoGpioConfig config = gpio->config;
    config.enabled = false;
    taiko_gpio_configure(gpio, &config);
    for(size_t i = 0; i < COUNT_OF(gpio->outputs); i++) {
        DrumOutput* output = &gpio->outputs[i];
        furi_timer_free(output->timer);
    }
    free(gpio);
}
