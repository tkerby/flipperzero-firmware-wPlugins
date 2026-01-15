#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <math.h>
#include "../calculations.h"

#define EPSILON 0.001

int test_count = 0;
int test_passed = 0;
int test_failed = 0;

// Assert helper: compares two doubles with a tolerance
void assert_double_equals(const char* test_name, double expected, double actual, double tolerance) {
    test_count++;
    double diff = fabs(expected - actual);
    if (diff <= tolerance) {
        test_passed++;
        printf("✓ PASS: %s (expected: %.3f, actual: %.3f)\n", test_name, expected, actual);
    } else {
        test_failed++;
        printf("✗ FAIL: %s (expected: %.3f, actual: %.3f, diff: %.3f)\n", test_name, expected, actual, diff);
    }
}

// Assert helper: compares two boolean values
void assert_bool_equals(const char* test_name, bool expected, bool actual) {
    test_count++;
    if (expected == actual) {
        test_passed++;
        printf("✓ PASS: %s (expected: %s, actual: %s)\n", test_name, expected ? "true" : "false", actual ? "true" : "false");
    } else {
        test_failed++;
        printf("✗ FAIL: %s (expected: %s, actual: %s)\n", test_name, expected ? "true" : "false", actual ? "true" : "false");
    }
}

void test_calculate_orbital_position() {
    printf("\n=== Testing calculate_orbital_position ===\n");
    
    // Test case: Earth at day 0 should be at start angle (0 degrees = 0 radians)
    CelestialBody earth = {"EARTH", 365.0, 1.0, 0.0167, 0.0, -1, false};
    double pos = calculate_orbital_position(&earth, 0);
    assert_double_equals("Earth at day 0", 0.0, pos, 0.1);
    
    // Test case: Earth at day 91.25 (quarter year) should be at ~π/2
    pos = calculate_orbital_position(&earth, 91);
    assert_double_equals("Earth at day 91 (~quarter year)", PI_CONST / 2.0, pos, 0.2);
    
    // Test case: Mars starting at 30 degrees
    CelestialBody mars = {"MARS", 687.0, 1.524, 0.0934, 30.0, -1, false};
    pos = calculate_orbital_position(&mars, 0);
    double expected = 30.0 * PI_OVER_180;
    assert_double_equals("Mars at day 0 with 30deg start", expected, pos, 0.1);
    
    // Test case: Position should wrap around 2π
    pos = calculate_orbital_position(&earth, 365);
    assert_double_equals("Earth at day 365 wraps to start", 0.0, pos, 0.2);
}

void test_calculate_hohmann_delta_v() {
    printf("\n=== Testing calculate_hohmann_delta_v ===\n");
    
    // Test case: Earth to Mars transfer (well-known value)
    // Expected delta-v is approximately 5.7 km/s
    double delta_v = calculate_hohmann_delta_v(1.0, 1.524);
    assert_double_equals("Earth to Mars delta-v", 5.7, delta_v, 0.5);
    
    // Test case: Earth to Venus transfer
    // Expected delta-v is approximately 5.2 km/s
    delta_v = calculate_hohmann_delta_v(1.0, 0.723);
    assert_double_equals("Earth to Venus delta-v", 5.2, delta_v, 0.5);
    
    // Test case: Symmetric property - same delta-v in both directions
    double delta_v_forward = calculate_hohmann_delta_v(1.0, 1.524);
    double delta_v_backward = calculate_hohmann_delta_v(1.524, 1.0);
    assert_double_equals("Symmetric delta-v (forward)", delta_v_forward, delta_v_backward, 0.01);
}

void test_calculate_transfer_time() {
    printf("\n=== Testing calculate_transfer_time ===\n");
    
    // Test case: Earth to Mars transfer time
    // Expected: approximately 259 days
    double time = calculate_transfer_time(1.0, 1.524);
    assert_double_equals("Earth to Mars transfer time", 259.0, time, 10.0);
    
    // Test case: Earth to Venus transfer time
    // Expected: approximately 146 days
    time = calculate_transfer_time(1.0, 0.723);
    assert_double_equals("Earth to Venus transfer time", 146.0, time, 10.0);
    
    // Test case: Transfer time should be positive
    time = calculate_transfer_time(1.0, 5.203); // Earth to Jupiter
    if (time > 0) {
        test_passed++;
        test_count++;
        printf("✓ PASS: Transfer time is positive (%.1f days)\n", time);
    } else {
        test_failed++;
        test_count++;
        printf("✗ FAIL: Transfer time should be positive\n");
    }
}

void test_calculate_transfer() {
    printf("\n=== Testing calculate_transfer ===\n");
    
    CelestialBody earth = {"EARTH", 365.0, 1.0, 0.0167, 0.0, -1, false};
    CelestialBody mars = {"MARS", 687.0, 1.524, 0.0934, 30.0, -1, false};
    
    // Test case: Calculate transfer at optimal day
    TransferResult result = calculate_transfer(&mars, 147, &earth);
    
    // Should have positive delta-v
    if (result.delta_v_total > 0) {
        test_passed++;
        test_count++;
        printf("✓ PASS: Transfer has positive delta-v (%.2f km/s)\n", result.delta_v_total);
    } else {
        test_failed++;
        test_count++;
        printf("✗ FAIL: Transfer should have positive delta-v\n");
    }
    
    // Should have positive flight time
    if (result.flight_time_days > 0) {
        test_passed++;
        test_count++;
        printf("✓ PASS: Transfer has positive flight time (%.1f days)\n", result.flight_time_days);
    } else {
        test_failed++;
        test_count++;
        printf("✗ FAIL: Transfer should have positive flight time\n");
    }
    
    // Departure angle should be in valid range (0-360)
    if (result.departure_angle >= 0 && result.departure_angle < 360) {
        test_passed++;
        test_count++;
        printf("✓ PASS: Departure angle in valid range (%.1f degrees)\n", result.departure_angle);
    } else {
        test_failed++;
        test_count++;
        printf("✗ FAIL: Departure angle should be 0-360, got %.1f\n", result.departure_angle);
    }
    
    // Test with Venus
    CelestialBody venus = {"VENUS", 225.0, 0.723, 0.0068, 45.0, -1, false};
    result = calculate_transfer(&venus, 100, &earth);
    
    // Venus transfer should also be possible
    assert_bool_equals("Venus transfer is possible", true, result.is_possible);
}

int main(void) {
    printf("===========================================\n");
    printf("Space Travel Calculator - Core Function Tests\n");
    printf("===========================================\n");
    
    test_calculate_orbital_position();
    test_calculate_hohmann_delta_v();
    test_calculate_transfer_time();
    test_calculate_transfer();
    
    printf("\n===========================================\n");
    printf("Test Summary\n");
    printf("===========================================\n");
    printf("Total tests: %d\n", test_count);
    printf("Passed: %d\n", test_passed);
    printf("Failed: %d\n", test_failed);
    printf("===========================================\n");
    
    return test_failed > 0 ? 1 : 0;
}
