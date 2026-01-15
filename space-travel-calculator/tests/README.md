# Core Function Tests

This directory contains unit tests for the space-travel-calculator's core atomic functions.

## What is Tested

The test suite validates the following core calculation functions:

1. **`calculate_orbital_position()`** - Planetary position calculations
   - Tests orbital mechanics at different times
   - Verifies angle wrapping behavior
   - Tests starting position offsets

2. **`calculate_hohmann_delta_v()`** - Delta-v for Hohmann transfers
   - Tests Earth-to-Mars transfers (~5.6 km/s)
   - Tests Earth-to-Venus transfers (~5.2 km/s)
   - Verifies symmetric property

3. **`calculate_transfer_time()`** - Transfer time calculations
   - Tests Earth-to-Mars transfers (~259 days)
   - Tests Earth-to-Venus transfers (~146 days)
   - Verifies positive time values

4. **`calculate_transfer()`** - Complete transfer calculations
   - Tests delta-v calculations
   - Tests flight time calculations
   - Tests departure angle calculations
   - Verifies transfer feasibility logic

## Running the Tests Locally

### Prerequisites
- GCC compiler
- Make
- Math library (libm)

### Build and Run

```bash
# Build the tests
make

# Run the tests
make test

# Clean up
make clean
```

## Test Results

All tests must pass for the build to succeed. The test suite includes:
- 14 total tests
- Tests for known values from orbital mechanics
- Edge case testing
- Integration tests

## Continuous Integration

These tests run automatically on every push and pull request via GitHub Actions.
See `.github/workflows/test-core-functions.yml` for the CI configuration.
