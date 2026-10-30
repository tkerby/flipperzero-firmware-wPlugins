// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#pragma once

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>

#define COUNT_OF(array)       (sizeof(array) / sizeof((array)[0]))
#define furi_check(condition) assert(condition)

typedef struct FuriTimer FuriTimer;
typedef void (*FuriTimerCallback)(void* context);
typedef enum {
    FuriTimerTypeOnce
} FuriTimerType;
typedef enum {
    FuriStatusOk
} FuriStatus;

FuriTimer* furi_timer_alloc(FuriTimerCallback callback, FuriTimerType type, void* context);
FuriStatus furi_timer_start(FuriTimer* timer, uint32_t ticks);
FuriStatus furi_timer_stop(FuriTimer* timer);
void furi_timer_flush(void);
void furi_timer_free(FuriTimer* timer);
uint32_t furi_ms_to_ticks(uint32_t ms);
