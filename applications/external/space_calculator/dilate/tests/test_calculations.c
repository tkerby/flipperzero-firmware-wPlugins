#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "../calculations.h"

#define EPSILON 0.0001

int test_count = 0;
int test_passed = 0;
int test_failed = 0;

void assert_double_equals(const char* test_name, double expected, double actual, double tolerance) {
    test_count++;
    double diff = fabs(expected - actual);
    if(diff <= tolerance) {
        test_passed++;
        printf("✓ PASS: %s (expected: %.6f, actual: %.6f)\n", test_name, expected, actual);
    } else {
        test_failed++;
        printf(
            "✗ FAIL: %s (expected: %.6f, actual: %.6f, diff: %.6f)\n",
            test_name,
            expected,
            actual,
            diff);
    }
}

void assert_infinite(const char* test_name, double actual) {
    test_count++;
    if(isinf(actual)) {
        test_passed++;
        printf("✓ PASS: %s (value is infinite as expected)\n", test_name);
    } else {
        test_failed++;
        printf("✗ FAIL: %s (expected infinite, got %.6f)\n", test_name, actual);
    }
}

void test_lorentz_factor() {
    printf("\n=== Testing lorentz_factor ===\n");

    // Test case: v = 0 should give γ = 1
    double gamma = lorentz_factor(0.0);
    assert_double_equals("Lorentz factor at v=0", 1.0, gamma, EPSILON);

    // Test case: v = 0.866c should give γ ≈ 2.0 (exact value)
    // This is a well-known special relativity test case
    gamma = lorentz_factor(0.866);
    assert_double_equals("Lorentz factor at v=0.866c", 2.0, gamma, 0.01);

    // Test case: v = 0.9c should give γ ≈ 2.294
    gamma = lorentz_factor(0.9);
    assert_double_equals("Lorentz factor at v=0.9c", 2.294, gamma, 0.01);

    // Test case: v = 0.99c should give γ ≈ 7.089
    gamma = lorentz_factor(0.99);
    assert_double_equals("Lorentz factor at v=0.99c", 7.089, gamma, 0.01);

    // Test case: v = 0.999c should give γ ≈ 22.366
    gamma = lorentz_factor(0.999);
    assert_double_equals("Lorentz factor at v=0.999c", 22.366, gamma, 0.1);

    // Test case: v >= c should return infinity
    gamma = lorentz_factor(1.0);
    assert_infinite("Lorentz factor at v=c (should be infinite)", gamma);

    gamma = lorentz_factor(1.5);
    assert_infinite("Lorentz factor at v>c (should be infinite)", gamma);

    // Test case: negative velocity should return 1
    gamma = lorentz_factor(-0.5);
    assert_double_equals("Lorentz factor at negative v", 1.0, gamma, EPSILON);
}

void test_time_to_seconds() {
    printf("\n=== Testing time_to_seconds ===\n");

    // Test case: 1 hour = 3600 seconds
    double seconds = time_to_seconds(1.0, UNIT_HOURS);
    assert_double_equals("1 hour to seconds", 3600.0, seconds, EPSILON);

    // Test case: 2.5 hours
    seconds = time_to_seconds(2.5, UNIT_HOURS);
    assert_double_equals("2.5 hours to seconds", 9000.0, seconds, EPSILON);

    // Test case: 1 day = 86400 seconds
    seconds = time_to_seconds(1.0, UNIT_DAYS);
    assert_double_equals("1 day to seconds", 86400.0, seconds, EPSILON);

    // Test case: 1 year = 31536000 seconds (365 days)
    seconds = time_to_seconds(1.0, UNIT_YEARS);
    assert_double_equals("1 year to seconds", 31536000.0, seconds, EPSILON);

    // Test case: 0.5 years
    seconds = time_to_seconds(0.5, UNIT_YEARS);
    assert_double_equals("0.5 years to seconds", 15768000.0, seconds, EPSILON);
}

void test_time_dilation_calculation() {
    printf("\n=== Testing time dilation calculations ===\n");

    // Integration test: Calculate actual time dilation scenario
    // Scenario: 1 year traveling at 0.9c
    double v = 0.9;
    double traveler_time = 1.0; // years

    // Convert to seconds
    double traveler_seconds = time_to_seconds(traveler_time, UNIT_YEARS);

    // Calculate Lorentz factor
    double gamma = lorentz_factor(v);

    // Calculate Earth time
    double earth_seconds = traveler_seconds * gamma;
    double earth_years = earth_seconds / time_to_seconds(1.0, UNIT_YEARS);

    // Expected: ~2.29 years on Earth
    assert_double_equals("1 year at 0.9c -> Earth time", 2.29, earth_years, 0.05);

    // Scenario 2: 1 year traveling at 0.866c
    v = 0.866;
    gamma = lorentz_factor(v);
    earth_seconds = traveler_seconds * gamma;
    earth_years = earth_seconds / time_to_seconds(1.0, UNIT_YEARS);

    // Expected: ~2.0 years on Earth
    assert_double_equals("1 year at 0.866c -> Earth time", 2.0, earth_years, 0.05);
}

int main(void) {
    printf("===========================================\n");
    printf("Dilate - Core Function Tests\n");
    printf("===========================================\n");

    test_lorentz_factor();
    test_time_to_seconds();
    test_time_dilation_calculation();

    printf("\n===========================================\n");
    printf("Test Summary\n");
    printf("===========================================\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);
    printf("===========================================\n");

    return test_failed > 0 ? 1 : 0;
}
