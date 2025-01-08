#pragma once

#include <stddef.h>

#include <input/input.h>

#include "src/engine/game_engine.h"

typedef struct InputConverter InputConverter;

InputConverter*
input_converter_alloc(void);

void
input_converter_free(InputConverter* input_converter);

void
input_converter_reset(InputConverter* input_converter);

void
input_converter_process_state(InputConverter* input_converter,
                              InputState* input_state);

FuriStatus
input_converter_get_event(InputConverter* input_converter, InputEvent* event);
