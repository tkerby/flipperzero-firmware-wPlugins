# Flipper Space Calculators

A collection of minimalist space and physics calculators for the Flipper Zero. No games, no gimmicks - just the essential calculations for when you need them most.

## Applications

### [Space Travel Calculator](./space-travel-calculator/)
Interplanetary trajectory calculator for planning transfers between planets. Calculates:
- Hohmann transfer orbits
- Delta-v requirements
- Launch windows
- Phase angles

*For when you need to know if that Mars window is actually worth taking.*

### [Dilate](./dilate/)
Time dilation calculator with two modes:
- **Special Relativity**: Time dilation at relativistic velocities
- **General Relativity**: Time dilation in gravitational fields

*Because sometimes you need to know exactly how much younger you'll be.*

## Design Philosophy

These calculators follow strict minimalist principles:
- Boot directly to functionality - no splash screens
- Response time <50ms for all interactions
- Numbers only - no unnecessary explanations
- Default system font - no fancy typography
- Metric units - this is space

## Building

Each application can be built independently from the Flipper firmware root:

```bash
# Space Travel Calculator
./fbt fap_space_travel_calculator

# Dilate
./fbt fap_dilate
```

## Installation

Copy the `.fap` files to your Flipper Zero's SD card under `/apps/Tools/`

## Testing

Both calculators include automated unit tests for their core atomic functions. These tests run in GitHub Actions on every push and pull request, ensuring the physics calculations work correctly without needing a physical Flipper device.

**Test Coverage:**
- Space Travel Calculator: 14 tests covering orbital mechanics, Hohmann transfers, and transfer calculations
- Dilate: 15 tests covering Lorentz factor calculations and time dilation formulas

See the `tests/` directory in each app for details.

## Project Structure

```
flipper-space-calculators/
├── README.md                           # This file
├── .github/workflows/                  # GitHub Actions CI
│   └── test-core-functions.yml        # Automated tests
├── space-travel-calculator/            # Orbital transfer calculator
│   ├── README.md
│   ├── CLAUDE.md                       # Development guide
│   ├── calculations.h/c                # Core calculation functions
│   ├── tests/                          # Unit tests
│   └── [source files]
└── dilate/                             # Time dilation calculator
    ├── README.md
    ├── CLAUDE.md                       # Development guide
    ├── calculations.h/c                # Core calculation functions
    ├── tests/                          # Unit tests
    └── [source files]
```

## Contributing

These tools are designed to do one thing perfectly. If you're thinking of adding features, ask yourself:
1. Is it essential to the core calculation?
2. Does it maintain <50ms response time?
3. Does it follow the minimalist aesthetic?

If any answer is no, it doesn't belong here.

## License

These calculators are tools, not toys. Use them wisely.

---

*"The universe is under no obligation to make sense to you. But your calculator is."*