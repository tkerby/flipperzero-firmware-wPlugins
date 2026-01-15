# Core Function Tests

This directory contains unit tests for the dilate calculator's core atomic functions.

## What is Tested

The test suite validates the following core calculation functions:

1. **`lorentz_factor()`** - Special relativity Lorentz factor calculation
   - Tests at v=0 (γ=1)
   - Tests at v=0.866c (γ=2.0, exact value)
   - Tests at v=0.9c (γ≈2.29)
   - Tests at v=0.99c (γ≈7.09)
   - Tests at v=0.999c (γ≈22.37)
   - Tests at v≥c (should return infinity)
   - Tests negative velocity handling

2. **`time_to_seconds()`** - Time unit conversion
   - Tests hour to second conversion
   - Tests day to second conversion
   - Tests year to second conversion
   - Tests fractional values

3. **Integration tests** - Complete time dilation scenarios
   - Tests 1 year at 0.9c → ~2.29 Earth years
   - Tests 1 year at 0.866c → 2.0 Earth years

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
- 15 total tests
- Tests verified against known special relativity values
- Edge case testing (infinity, negative velocities)
- Integration tests matching real-world scenarios

## Continuous Integration

These tests run automatically on every push and pull request via GitHub Actions.
See `.github/workflows/test-core-functions.yml` for the CI configuration.
