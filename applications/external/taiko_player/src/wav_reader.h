// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 ZeroNinx

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t sample_rate;
    uint32_t data_offset;
    uint32_t sample_count;
} WavInfo;

typedef bool (*WavReadAt)(void* context, uint32_t offset, void* data, size_t size);

/* Accepts RIFF/WAVE PCM, unsigned 8-bit mono, 8..48 kHz. Skips optional chunks. */
bool wav_read_header(WavReadAt read, void* context, uint32_t size, WavInfo* info);
