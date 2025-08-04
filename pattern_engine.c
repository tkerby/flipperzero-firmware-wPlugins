/*
 * Copyright (c) 2024, UID Brute Smarter Contributors
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this
 *    list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "pattern_engine.h"

// Include furi.h only when not in unit test mode
#ifndef UNIT_TEST
#include <furi.h>
#else
// Mock furi functionality for unit tests
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#define TAG "PatternEngine"
#define FURI_LOG_E(tag, ...) (void)(tag)
#define FURI_LOG_D(tag, ...) (void)(tag)
#define FURI_LOG_I(tag, ...) (void)(tag)
#define furi_assert(condition) (void)(condition)
#endif

#define TAG "PatternEngine"
#define MAX_RANGE_SIZE 1000
#define DEFAULT_RANGE_EXPANSION 256

static const uint32_t common_k_values[] = {16, 32, 64, 100, 256};
static const uint8_t common_k_count = 5;

bool pattern_engine_detect(const uint32_t* uids, uint8_t count, PatternResult* result) {
    furi_assert(uids);
    furi_assert(result);
    
    if(count == 0 || count > 5) {
        FURI_LOG_E(TAG, "Invalid UID count: %d", count);
        return false;
    }
    
    if(count == 1) {
        // Single card - use default range expansion
        result->type = PatternUnknown;
        result->start_uid = (uids[0] > 256) ? uids[0] - 256 : 0;
        result->end_uid = (uids[0] < 0xFFFFFF00) ? uids[0] + 256 : 0xFFFFFFFF;
        result->step = 1;
        result->range_size = (result->end_uid - result->start_uid) + 1;
        return true;
    }
    
    // Calculate deltas between consecutive UIDs
    int32_t deltas[4];
    for(uint8_t i = 0; i < count - 1; i++) {
        deltas[i] = (int32_t)(uids[i + 1] - uids[i]);
    }
    
    // Check for +1 linear pattern
    bool is_inc1 = true;
    for(uint8_t i = 0; i < count - 1; i++) {
        if(deltas[i] != 1) {
            is_inc1 = false;
            break;
        }
    }
    
    if(is_inc1) {
        result->type = PatternInc1;
        // Expand range by 128 in each direction
        result->start_uid = (uids[0] > 128) ? uids[0] - 128 : 0;
        result->end_uid = (uids[count - 1] < 0xFFFFFF80) ? uids[count - 1] + 128 : 0xFFFFFFFF;
        result->step = 1;
        result->range_size = (result->end_uid - result->start_uid) + 1;
        return true;
    }
    
    // Check for +k linear pattern
    for(uint8_t k_idx = 0; k_idx < common_k_count; k_idx++) {
        uint32_t k = common_k_values[k_idx];
        bool is_inc_k = true;
        
        for(uint8_t i = 0; i < count - 1; i++) {
            if(deltas[i] != (int32_t)k) {
                is_inc_k = false;
                break;
            }
        }
        
        if(is_inc_k) {
            result->type = PatternIncK;
            // Expand range by 10 steps in each direction
            uint32_t expand = 10 * k;
            result->start_uid = (uids[0] > expand) ? uids[0] - expand : 0;
            result->end_uid = (uids[count - 1] < (0xFFFFFFFF - expand)) ? uids[count - 1] + expand : 0xFFFFFFFF;
            result->step = k;
            result->range_size = ((result->end_uid - result->start_uid) / k) + 1;
            return true;
        }
    }
    
    // Check for 16-bit little-endian counter pattern
    bool is_le16 = true;
    for(uint8_t i = 0; i < count - 1; i++) {
        if(deltas[i] != 0x10000) {
            is_le16 = false;
            break;
        }
    }
    
    if(is_le16) {
        result->type = PatternLe16;
        // For 16-bit counter, generate full 16-bit range
        uint32_t base = uids[0] & 0xFFFF0000;
        result->start_uid = base;
        result->end_uid = base | 0x0000FFFF;
        result->step = 1;
        result->range_size = 65536; // Full 16-bit range
        return true;
    }
    
    // Unknown pattern - expand around min/max values
    result->type = PatternUnknown;
    
    uint32_t min_uid = uids[0];
    uint32_t max_uid = uids[0];
    
    for(uint8_t i = 1; i < count; i++) {
        if(uids[i] < min_uid) min_uid = uids[i];
        if(uids[i] > max_uid) max_uid = uids[i];
    }
    
    // Expand by 256 in each direction
    result->start_uid = (min_uid > 256) ? min_uid - 256 : 0;
    result->end_uid = (max_uid < 0xFFFFFF00) ? max_uid + 256 : 0xFFFFFFFF;
    result->step = 1;
    result->range_size = (result->end_uid - result->start_uid) + 1;
    
    return true;
}

bool pattern_engine_build_range(
    const PatternResult* result, 
    uint32_t* range, 
    uint16_t range_size, 
    uint16_t* actual_size) {
    furi_assert(result);
    furi_assert(range);
    furi_assert(actual_size);
    
    if(result->start_uid > result->end_uid) {
        FURI_LOG_E(TAG, "Invalid range: start=%lu > end=%lu", 
                   result->start_uid, result->end_uid);
        return false;
    }
    
    if(result->step == 0) {
        FURI_LOG_E(TAG, "Invalid step: %lu", result->step);
        return false;
    }
    
    if(!pattern_engine_validate_range(result->start_uid, result->end_uid, result->step)) {
        FURI_LOG_E(TAG, "Invalid range: start=%lu, end=%lu, step=%lu", 
                   result->start_uid, result->end_uid, result->step);
        return false;
    }
    
    *actual_size = 0;
    uint32_t current = result->start_uid;
    
    while(current <= result->end_uid && *actual_size < range_size) {
        range[*actual_size] = current;
        (*actual_size)++;
        current += result->step;
    }
    
    FURI_LOG_I(TAG, "Built range with %d UIDs", *actual_size);
    return true;
}

const char* pattern_engine_get_name(PatternType type) {
    switch(type) {
        case PatternInc1:
            return "+1 Linear";
        case PatternIncK:
            return "+K Linear";
        case PatternLe16:
            return "16-bit LE Counter";
        case PatternUnknown:
        default:
            return "Unknown (±256)";
    }
}

bool pattern_engine_validate_range(uint32_t start, uint32_t end, uint32_t step) {
    if(step == 0) {
        return false;
    }
    
    if(start >= end) {
        return false;
    }
    
    // Check if range is too large
    uint64_t range_size = ((uint64_t)end - (uint64_t)start) / (uint64_t)step + 1;
    if(range_size > MAX_RANGE_SIZE) {
        return false;
    }
    
    return true;
}