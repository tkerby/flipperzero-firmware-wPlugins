#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Минимальный Arduino-совместимый интерфейс
typedef struct {
    void (*play)(const uint8_t* song);
    void (*stop)(void);
    void (*playPause)(void);
    void (*muteChannel)(uint8_t ch);
    void (*unMuteChannel)(uint8_t ch);
} ATMsynth;

// Глобальный объект, который ожидает твой код: ATM.play(...), ATM.stop(), ...
extern ATMsynth ATM;

#ifdef __cplusplus
}
#endif
