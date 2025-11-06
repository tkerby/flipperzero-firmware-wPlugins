# Dilate

Time dilation calculator for Flipper Zero. One mode. No frills. Physics only.

## Overview

Calculate time dilation effects from special relativity. Answer the critical question: "If I travel at relativistic speeds, how much time difference will there be between me and Earth?"

## Features

- **Velocity dilation**: Calculate time differences at relativistic speeds (up to 0.999c)
- **Human-centered**: Always compares YOU vs EARTH
- **Auto-scaling units**: Displays differences in meaningful units (seconds to years)
- **Experience-focused**: Rounded values that answer "what will it feel like?"

## User Interface

### Main Display
```
SPEED: 0.90c
TIME:  1.0y
──────────────
YOU:   1.0y
EARTH: 2.3y
DIFF:  +1.3y
```

## Controls

- **UP/DOWN**: Adjust velocity (adaptive precision based on current speed)
- **LEFT/RIGHT**: Adjust time duration
- **OK**: Cycle time units (h → d → y)
- **BACK**: Exit application

## Use Cases

Perfect for answering questions like:
- "I'm traveling to Proxima Centauri at 0.9c - how much will I age compared to Earth?"
- "At what speed do I need to travel for my twin to be noticeably older when I return?"
- "If I spend a year traveling at near light speed, how much time passes on Earth?"

## Technical Details

- Uses Lorentz factor: γ = 1/√(1 - v²/c²)
- Adaptive precision: 0.05c steps below 0.9c, 0.001c steps above 0.99c
- Maximum velocity: 0.999c
- Always compares traveler's proper time vs Earth coordinate time

## Building

```bash
# From flipper firmware root
./fbt fap_dilate
```

## Installation

Copy `dilate.fap` to your Flipper Zero's SD card under `/apps/Tools/`

## Test Values

- 0.866c for 1 year → Earth experiences 2.0 years (exactly)
- 0.90c for 1 year → Earth experiences 2.3 years
- 0.999c for 1 year → Earth experiences 22.4 years

---

*"The universe doesn't approximate."*