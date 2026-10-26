#include "wanbo_ir.h"

#include <furi.h>
#include <infrared_transmit.h>

#define WANBO_NEC_ADDRESS 0x3B

#define NEC_FREQUENCY  38000
#define NEC_DUTY_CYCLE 0.33f

#define NEC_HEADER_MARK  9000
#define NEC_HEADER_SPACE 4500
#define NEC_BIT_MARK     560
#define NEC_ZERO_SPACE   560
#define NEC_ONE_SPACE    1690
#define NEC_STOP_MARK    560
#define NEC_END_SPACE    40000

static const uint8_t wanbo_command_table[] = {
    [WanboCommandPower] = 0x00,
    [WanboCommandMouse] = 0x41,
    [WanboCommandSettings] = 0x0C,
    [WanboCommandOk] = 0x11,
    [WanboCommandUp] = 0x0D,
    [WanboCommandDown] = 0x15,
    [WanboCommandLeft] = 0x10,
    [WanboCommandRight] = 0x12,
    [WanboCommandMenu] = 0x16,
    [WanboCommandMute] = 0x1A,
    [WanboCommandReturn] = 0x0E,
    [WanboCommandHome] = 0x14,
    [WanboCommandFocusPlus] = 0x51,
    [WanboCommandFocusMinus] = 0x50,
    [WanboCommandVolumePlus] = 0x0A,
    [WanboCommandVolumeMinus] = 0x08,
};

static const char* wanbo_command_name_table[] = {
    [WanboCommandPower] = "Power",
    [WanboCommandMouse] = "Mouse",
    [WanboCommandSettings] = "Settings",
    [WanboCommandOk] = "OK",
    [WanboCommandUp] = "Up",
    [WanboCommandDown] = "Down",
    [WanboCommandLeft] = "Left",
    [WanboCommandRight] = "Right",
    [WanboCommandMenu] = "Menu",
    [WanboCommandMute] = "Mute",
    [WanboCommandReturn] = "Return",
    [WanboCommandHome] = "Home",
    [WanboCommandFocusPlus] = "Focus+",
    [WanboCommandFocusMinus] = "Focus-",
    [WanboCommandVolumePlus] = "Vol+",
    [WanboCommandVolumeMinus] = "Vol-",
};

static void wanbo_ir_add_byte(uint32_t* timings, size_t* index, uint8_t value) {
    for(uint8_t mask = 1; mask > 0; mask <<= 1) {
        timings[(*index)++] = NEC_BIT_MARK;
        timings[(*index)++] = (value & mask) ? NEC_ONE_SPACE : NEC_ZERO_SPACE;
    }
}

const char* wanbo_ir_get_name(WanboCommand command) {
    return wanbo_command_name_table[command];
}

void wanbo_ir_send(WanboCommand command) {
    uint8_t address = WANBO_NEC_ADDRESS;
    uint8_t command_value = wanbo_command_table[command];

    uint32_t timings[68];
    size_t index = 0;

    timings[index++] = NEC_HEADER_MARK;
    timings[index++] = NEC_HEADER_SPACE;

    wanbo_ir_add_byte(timings, &index, address);
    wanbo_ir_add_byte(timings, &index, (uint8_t)~address);
    wanbo_ir_add_byte(timings, &index, command_value);
    wanbo_ir_add_byte(timings, &index, (uint8_t)~command_value);

    timings[index++] = NEC_STOP_MARK;
    timings[index++] = NEC_END_SPACE;

    infrared_send_raw_ext(timings, index, true, NEC_FREQUENCY, NEC_DUTY_CYCLE);
}
