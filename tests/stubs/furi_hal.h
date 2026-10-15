#pragma once

#include <stdbool.h>

typedef struct {
    unsigned index;
} GpioPin;
typedef enum {
    GpioModeInput,
    GpioModeOutputPushPull
} GpioMode;
typedef enum {
    GpioPullDown
} GpioPull;
typedef enum {
    GpioSpeedLow
} GpioSpeed;

extern const GpioPin gpio_ext_pa7;
extern const GpioPin gpio_ext_pa6;
extern const GpioPin gpio_ext_pa4;
extern const GpioPin gpio_ext_pb3;
extern const GpioPin gpio_ext_pb2;
extern const GpioPin gpio_ext_pc3;
extern const GpioPin gpio_ext_pc1;
extern const GpioPin gpio_ext_pc0;

void furi_hal_gpio_write(const GpioPin* pin, bool high);
void furi_hal_gpio_init(const GpioPin* pin, GpioMode mode, GpioPull pull, GpioSpeed speed);
