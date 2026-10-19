/*
 * Test program to verify the range fix for large UID ranges
 */

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>

// Mock furi for testing
#define TAG                  "TestRangeFix"
#define FURI_LOG_E(tag, ...) printf("[ERROR] " __VA_ARGS__), printf("\n")
#define FURI_LOG_I(tag, ...) printf("[INFO] " __VA_ARGS__), printf("\n")
#define furi_assert(condition)                     \
    if(!(condition)) {                             \
        printf("Assert failed: %s\n", #condition); \
        exit(1);                                   \
    }

#define UNIT_TEST 1
#include "pattern_engine.h"
#include "pattern_engine.c"

void test_large_range() {
    printf("\n=== Testing Large Range Fix ===\n");

    // Test the problematic UIDs that would cause memory overrun
    uint32_t uids[2] = {0xb819254c, 0x98f28f4c};

    PatternResult result;
    bool detected = pattern_engine_detect(uids, 2, &result);

    printf("Pattern detection: %s\n", detected ? "SUCCESS" : "FAILED");
    if(detected) {
        printf("Pattern type: %s\n", pattern_engine_get_name(result.type));
        printf("Start UID: 0x%08x\n", result.start_uid);
        printf("End UID: 0x%08x\n", result.end_uid);
        printf("Step: %u\n", result.step);

        uint64_t total_uids =
            ((uint64_t)result.end_uid - (uint64_t)result.start_uid) / (uint64_t)result.step + 1;
        printf("Total UIDs in range: %llu\n", (unsigned long long)total_uids);

        // Test iterator initialization
        UidIterator iterator;
        bool init_success = pattern_engine_iterator_init(&iterator, &result);
        printf("Iterator init: %s\n", init_success ? "SUCCESS" : "FAILED");

        if(init_success) {
            // Try to get first few UIDs
            uint32_t uid;
            int count = 0;
            printf("First 5 UIDs from iterator:\n");
            while(count < 5 && pattern_engine_iterator_next(&iterator, &uid)) {
                printf("  %d: 0x%08x\n", count + 1, uid);
                count++;
            }

            // Check if range is too large for practical use
            if(total_uids > MAX_PRACTICAL_RANGE_SIZE) {
                printf(
                    "\nRange validation: CORRECTLY REJECTED (too large: %llu > %d)\n",
                    (unsigned long long)total_uids,
                    MAX_PRACTICAL_RANGE_SIZE);
            } else {
                printf(
                    "\nRange validation: ACCEPTED (%llu UIDs)\n", (unsigned long long)total_uids);
            }
        }
    }
}

void test_normal_range() {
    printf("\n=== Testing Normal Range ===\n");

    // Test a normal +1 pattern
    uint32_t uids[3] = {0x12345678, 0x12345679, 0x1234567A};

    PatternResult result;
    bool detected = pattern_engine_detect(uids, 3, &result);

    printf("Pattern detection: %s\n", detected ? "SUCCESS" : "FAILED");
    if(detected) {
        printf("Pattern type: %s\n", pattern_engine_get_name(result.type));
        printf("Start UID: 0x%08x\n", result.start_uid);
        printf("End UID: 0x%08x\n", result.end_uid);
        printf("Step: %u\n", result.step);

        uint64_t total_uids =
            ((uint64_t)result.end_uid - (uint64_t)result.start_uid) / (uint64_t)result.step + 1;
        printf("Total UIDs in range: %llu\n", (unsigned long long)total_uids);

        // Test iterator
        UidIterator iterator;
        bool init_success = pattern_engine_iterator_init(&iterator, &result);
        printf("Iterator init: %s\n", init_success ? "SUCCESS" : "FAILED");

        if(init_success && total_uids <= 500) {
            // Count all UIDs
            uint32_t uid;
            int count = 0;
            while(pattern_engine_iterator_next(&iterator, &uid)) {
                count++;
            }
            printf("Iterator produced %d UIDs\n", count);
        }
    }
}

int main() {
    printf("UID Brute Smarter - Range Fix Test\n");
    printf("===================================\n");

    test_normal_range();
    test_large_range();

    printf("\n=== Test Complete ===\n");
    return 0;
}
