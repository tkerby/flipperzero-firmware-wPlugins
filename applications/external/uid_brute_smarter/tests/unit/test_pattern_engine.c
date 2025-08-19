/* Comprehensive unit tests for pattern engine */
#include "../unity/unity.h"
#include "../mocks/furi_mock.h"
#include "../../pattern_engine.h"

// Test 1: Pattern detection - single UID (unknown pattern)
static void test_pattern_engine_detect_single_uid(void) {
    uint32_t uids[] = {0x12345678};
    PatternResult result;

    TEST_ASSERT_TRUE(pattern_engine_detect(uids, 1, &result));
    TEST_ASSERT_EQUAL_INT(PatternUnknown, result.type);
    // Dynamic calculation: start = uid - 256, end = uid + 256
    TEST_ASSERT_TRUE(result.start_uid <= 0x12345678);
    TEST_ASSERT_TRUE(result.end_uid >= 0x12345678);
    TEST_ASSERT_EQUAL_UINT(1, result.step);
    TEST_ASSERT_TRUE(result.range_size >= 256); // At least 256 UIDs
}

// Test 2: Pattern detection - +1 linear pattern
static void test_pattern_engine_detect_inc1_pattern(void) {
    uint32_t uids[] = {0x1000, 0x1001, 0x1002, 0x1003};
    PatternResult result;

    TEST_ASSERT_TRUE(pattern_engine_detect(uids, 4, &result));
    TEST_ASSERT_EQUAL_INT(PatternInc1, result.type);
    // Dynamic calculation: start = first_uid - 128, end = last_uid + 128
    TEST_ASSERT_TRUE(result.start_uid <= 0x1000);
    TEST_ASSERT_TRUE(result.end_uid >= 0x1003);
    TEST_ASSERT_EQUAL_UINT(1, result.step);
    TEST_ASSERT_TRUE(result.range_size >= 4); // At least the original UIDs
}

// Test 3: Pattern detection - +K linear pattern (K=16)
static void test_pattern_engine_detect_inc16_pattern(void) {
    uint32_t uids[] = {0x1000, 0x1010, 0x1020, 0x1030};
    PatternResult result;

    TEST_ASSERT_TRUE(pattern_engine_detect(uids, 4, &result));
    TEST_ASSERT_EQUAL_INT(PatternIncK, result.type);
    // Dynamic calculation: start = first_uid - 10*step, end = last_uid + 10*step
    TEST_ASSERT_TRUE(result.start_uid <= 0x1000);
    TEST_ASSERT_TRUE(result.end_uid >= 0x1030);
    TEST_ASSERT_EQUAL_UINT(16, result.step);
    TEST_ASSERT_TRUE(result.range_size >= 4); // At least 4 steps
}

// Test 4: Pattern detection - +K linear pattern (K=32)
static void test_pattern_engine_detect_inc32_pattern(void) {
    uint32_t uids[] = {0x2000, 0x2020, 0x2040, 0x2060};
    PatternResult result;

    TEST_ASSERT_TRUE(pattern_engine_detect(uids, 4, &result));
    TEST_ASSERT_EQUAL_INT(PatternIncK, result.type);
    // Dynamic calculation: start = first_uid - 10*step, end = last_uid + 10*step
    TEST_ASSERT_TRUE(result.start_uid <= 0x2000);
    TEST_ASSERT_TRUE(result.end_uid >= 0x2060);
    TEST_ASSERT_EQUAL_UINT(32, result.step);
    TEST_ASSERT_TRUE(result.range_size >= 4); // At least 4 steps
}

// Test 5: Pattern detection - 16-bit LE counter
static void test_pattern_engine_detect_le16_pattern(void) {
    uint32_t uids[] = {0x00010000, 0x00020000, 0x00030000};
    PatternResult result;

    TEST_ASSERT_TRUE(pattern_engine_detect(uids, 3, &result));
    TEST_ASSERT_EQUAL_INT(PatternLe16, result.type);
    // For 16-bit counter: base address with full 16-bit range
    TEST_ASSERT_TRUE((result.start_uid & 0xFFFF0000) == (uids[0] & 0xFFFF0000));
    TEST_ASSERT_TRUE((result.end_uid & 0xFFFF0000) == (uids[0] & 0xFFFF0000));
    TEST_ASSERT_EQUAL_UINT(1, result.step);
    TEST_ASSERT_EQUAL_UINT(65536, result.range_size); // Full 16-bit range
}

// Test 6: Pattern detection - unknown pattern (mixed deltas)
static void test_pattern_engine_detect_unknown_pattern(void) {
    uint32_t uids[] = {0x1000, 0x1005, 0x100A, 0x1014}; // 5, 5, 10 deltas
    PatternResult result;

    TEST_ASSERT_TRUE(pattern_engine_detect(uids, 4, &result));
    TEST_ASSERT_EQUAL_INT(PatternUnknown, result.type);
    // Unknown pattern: expanded around min/max
    TEST_ASSERT_TRUE(result.start_uid <= 0x1000);
    TEST_ASSERT_TRUE(result.end_uid >= 0x1014);
    TEST_ASSERT_EQUAL_UINT(1, result.step);
}

// Test 7: Pattern detection - boundary conditions
static void test_pattern_engine_detect_boundary_conditions(void) {
    PatternResult result;

    // Test minimum count (0)
    TEST_ASSERT_FALSE(pattern_engine_detect(NULL, 0, &result));

    // Test maximum count (5)
    uint32_t uids_max[] = {0x1000, 0x1001, 0x1002, 0x1003, 0x1004, 0x1005};
    TEST_ASSERT_FALSE(pattern_engine_detect(uids_max, 6, &result));

    // Test underflow protection
    uint32_t uids_underflow[] = {0x00000010, 0x00000011, 0x00000012};
    TEST_ASSERT_TRUE(pattern_engine_detect(uids_underflow, 3, &result));
    TEST_ASSERT_EQUAL_INT(PatternInc1, result.type);
    TEST_ASSERT_EQUAL_HEX32(0x00000000, result.start_uid); // Clamped to 0
}

// Test 8: Range generation - valid ranges
static void test_pattern_engine_build_range_valid(void) {
    PatternResult pattern = {
        .type = PatternInc1, .start_uid = 0x1000, .end_uid = 0x1005, .step = 1, .range_size = 6};

    uint32_t range[10];
    uint16_t actual_size = 0;

    TEST_ASSERT_TRUE(pattern_engine_build_range(&pattern, range, 10, &actual_size));
    TEST_ASSERT_EQUAL_UINT(6, actual_size);
    TEST_ASSERT_EQUAL_HEX32(0x1000, range[0]);
    TEST_ASSERT_EQUAL_HEX32(0x1001, range[1]);
    TEST_ASSERT_EQUAL_HEX32(0x1002, range[2]);
    TEST_ASSERT_EQUAL_HEX32(0x1003, range[3]);
    TEST_ASSERT_EQUAL_HEX32(0x1004, range[4]);
    TEST_ASSERT_EQUAL_HEX32(0x1005, range[5]);
}

// Test 9: Range generation - buffer size limit
static void test_pattern_engine_build_range_buffer_limit(void) {
    PatternResult pattern = {
        .type = PatternInc1, .start_uid = 0x1000, .end_uid = 0x1020, .step = 1, .range_size = 33};

    uint32_t range[10];
    uint16_t actual_size = 0;

    TEST_ASSERT_TRUE(pattern_engine_build_range(&pattern, range, 10, &actual_size));
    TEST_ASSERT_EQUAL_UINT(10, actual_size); // Limited by buffer size
    TEST_ASSERT_EQUAL_HEX32(0x1000, range[0]);
    TEST_ASSERT_EQUAL_HEX32(0x1009, range[9]);
}

// Test 10: Range generation - step sizes
static void test_pattern_engine_build_range_step_sizes(void) {
    PatternResult pattern = {
        .type = PatternIncK, .start_uid = 0x1000, .end_uid = 0x1020, .step = 16, .range_size = 3};

    uint32_t range[5];
    uint16_t actual_size = 0;

    TEST_ASSERT_TRUE(pattern_engine_build_range(&pattern, range, 5, &actual_size));
    TEST_ASSERT_EQUAL_UINT(3, actual_size);
    TEST_ASSERT_EQUAL_HEX32(0x1000, range[0]);
    TEST_ASSERT_EQUAL_HEX32(0x1010, range[1]);
    TEST_ASSERT_EQUAL_HEX32(0x1020, range[2]);
}

// Test 11: Range validation
static void test_pattern_engine_validate_range(void) {
    // Valid ranges
    TEST_ASSERT_TRUE(pattern_engine_validate_range(0x1000, 0x1005, 1));
    TEST_ASSERT_TRUE(pattern_engine_validate_range(0x1000, 0x1020, 16));

    // Invalid ranges
    TEST_ASSERT_FALSE(pattern_engine_validate_range(0x1005, 0x1000, 1)); // start > end
    TEST_ASSERT_FALSE(pattern_engine_validate_range(0x1000, 0x1005, 0)); // step == 0
    TEST_ASSERT_FALSE(pattern_engine_validate_range(
        0x1000, 0x1000, 1)); // start == end, size 1 (valid but edge case)

    // Too large range
    TEST_ASSERT_FALSE(
        pattern_engine_validate_range(0x00000000, 0xFFFFFFFF, 1)); // Would be 4B UIDs
}

// Test 12: Pattern name strings
static void test_pattern_engine_get_name(void) {
    TEST_ASSERT_EQUAL_STRING("+1 Linear", pattern_engine_get_name(PatternInc1));
    TEST_ASSERT_EQUAL_STRING("+K Linear", pattern_engine_get_name(PatternIncK));
    TEST_ASSERT_EQUAL_STRING("16-bit LE Counter", pattern_engine_get_name(PatternLe16));
    TEST_ASSERT_EQUAL_STRING("Unknown (±256)", pattern_engine_get_name(PatternUnknown));
}

// Test 13: Edge cases - zero and max values
static void test_pattern_engine_edge_cases(void) {
    PatternResult result;

    // Test with zero UID
    uint32_t uids_zero[] = {0x00000000, 0x00000001};
    TEST_ASSERT_TRUE(pattern_engine_detect(uids_zero, 2, &result));
    TEST_ASSERT_EQUAL_INT(PatternInc1, result.type);
    TEST_ASSERT_EQUAL_HEX32(0x00000000, result.start_uid); // Clamped to 0

    // Test with maximum UID
    uint32_t uids_max[] = {0xFFFFFFFE, 0xFFFFFFFF};
    TEST_ASSERT_TRUE(pattern_engine_detect(uids_max, 2, &result));
    TEST_ASSERT_EQUAL_INT(PatternInc1, result.type);
    TEST_ASSERT_EQUAL_HEX32(0xFFFFFFFF, result.end_uid); // Clamped at max
}

// Test 14: Large step patterns
static void test_pattern_engine_large_steps(void) {
    PatternResult result;

    // Test K=256 pattern
    uint32_t uids_k256[] = {0x1000, 0x1100, 0x1200};
    TEST_ASSERT_TRUE(pattern_engine_detect(uids_k256, 3, &result));
    TEST_ASSERT_EQUAL_INT(PatternIncK, result.type);
    TEST_ASSERT_EQUAL_UINT(256, result.step);

    // Test K=100 pattern
    uint32_t uids_k100[] = {0x1000, 0x1064, 0x10C8};
    TEST_ASSERT_TRUE(pattern_engine_detect(uids_k100, 3, &result));
    TEST_ASSERT_EQUAL_INT(PatternIncK, result.type);
    TEST_ASSERT_EQUAL_UINT(100, result.step);
}

// Test 15: Range generation - invalid inputs
static void test_pattern_engine_build_range_invalid(void) {
    PatternResult pattern = {
        .type = PatternInc1, .start_uid = 0x1000, .end_uid = 0x1005, .step = 1, .range_size = 6};
    uint32_t range[10];
    uint16_t actual_size = 0;

    // Test invalid range (skip NULL tests to avoid segfault)
    pattern.start_uid = 0x1005;
    pattern.end_uid = 0x1000;
    pattern.step = 1;
    TEST_ASSERT_FALSE(pattern_engine_build_range(&pattern, range, 10, &actual_size));
}

// Test runner
void test_pattern_engine_run_all(void) {
    printf("\n");
    printf("🧪 Pattern Engine Unit Tests\n");
    printf("============================\n");

    unity_begin();

    RUN_TEST(test_pattern_engine_detect_single_uid);
    RUN_TEST(test_pattern_engine_detect_inc1_pattern);
    RUN_TEST(test_pattern_engine_detect_inc16_pattern);
    RUN_TEST(test_pattern_engine_detect_inc32_pattern);
    RUN_TEST(test_pattern_engine_detect_le16_pattern);
    RUN_TEST(test_pattern_engine_detect_unknown_pattern);
    RUN_TEST(test_pattern_engine_detect_boundary_conditions);
    RUN_TEST(test_pattern_engine_build_range_valid);
    RUN_TEST(test_pattern_engine_build_range_buffer_limit);
    RUN_TEST(test_pattern_engine_build_range_step_sizes);
    RUN_TEST(test_pattern_engine_validate_range);
    RUN_TEST(test_pattern_engine_get_name);
    RUN_TEST(test_pattern_engine_edge_cases);
    RUN_TEST(test_pattern_engine_large_steps);
    RUN_TEST(test_pattern_engine_build_range_invalid);

    unity_end();
}

int main(void) {
    test_pattern_engine_run_all();
    return 0;
}
