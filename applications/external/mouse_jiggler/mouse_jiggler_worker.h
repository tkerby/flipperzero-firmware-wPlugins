#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <furi.h>
#include <furi_hal.h>
#include <gui/gui.h>
#include <input/input.h>
#include <lib/flipper_format/flipper_format.h>

#define MOUSE_JIGGLER_MOUSE_MOVE_DX 5
#define MOUSE_JIGGLER_DELAY_MS      50

static const uint32_t mouse_jiggler_interval_values[] =
    {250, 500, 1000, 2000, 5000, 7500, 15000, 30000, 45000, 60000, 150000, 300000};
static const uint8_t mouse_jiggler_interval_count = 12;

typedef struct MouseJigglerWorker {
    FuriThread* thread;
    bool is_running;
    bool is_jiggle;
    uint8_t delay_index;
    uint32_t delay_value;
    int8_t mouse_move_dx;
} MouseJigglerWorker;

MouseJigglerWorker* mouse_jiggler_worker_alloc();

void mouse_jiggler_worker_free(MouseJigglerWorker* instance);

void mouse_jiggler_worker_start(MouseJigglerWorker* instance);

void mouse_jiggler_worker_stop(MouseJigglerWorker* instance);

void mouse_jiggler_worker_set_delay(MouseJigglerWorker* instance);

void mouse_jiggler_worker_toggle_jiggle(MouseJigglerWorker* instance);
