# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
**UID Brute Smarter** is an enhanced NFC key management and brute-force application for Flipper Zero, built as a completely separate app from the original UID Brute Smart with advanced key tracking and management features.

## Build and Launch Commands

### Quick Launch (Single App)
```bash
# Launch directly on connected Flipper
./fbt launch APPSRC=uid_brute_smarter

# Build and flash single app
./fbt fap_uid_brute_smarter
```

### Development Workflow
```bash
# From Momentum-Firmware root directory
cd /Users/fbettag/src/flipperzero/Momentum-Firmware

# Generate version file (shows git version on about screen)
cd applications_user/uid_brute_smarter
git describe --tags --always --dirty > VERSION
echo "const char* APP_VERSION_STR = \"$(cat VERSION)\";" > version.c
echo "extern const char* APP_VERSION_STR;" > version.h
cd ../..

# Build the app
./fbt fap_uid_brute_smarter

# Install to Flipper (USB required)
./fbt flash_usb

# Build TGZ package for distribution
./fbt updater_package
```

### Testing Commands
```bash
# Run tests (when implemented)
./fbt test_uid_brute_smarter

# Run pattern engine tests
./fbt test_pattern_engine

# Clean build
./fbt clean
./fbt fap_uid_brute_smarter
```

## Code Architecture

### Core Components
- **uid_brute_smarter.c**: Main application logic (26KB+), handles UI state machine, key management, and brute-force execution
- **pattern_engine.c/h**: Pattern detection algorithms for identifying UID sequences (+1, +K, 16-bit counter)
- **views/uid_brute_smart_views.h**: UI component definitions and view structures

### Key Data Structures
```c
typedef struct {
    uint32_t uid;           // Key UID value
    char* name;            // Generated key name ("Key 1", "Key 2", etc.)
    char* file_path;       // Original NFC filename
    uint32_t loaded_time;  // Tick count when loaded
    bool is_active;        // Active status flag
} KeyInfo;

typedef struct {
    PatternType type;       // Pattern type enum
    uint32_t start_uid;     // Range start
    uint32_t end_uid;       // Range end
    uint32_t step;          // Step increment
    uint16_t range_size;    // Total UIDs in range
} PatternResult;
```

### State Machine Flow
1. **Splash Screen** → Main Menu
2. **Main Menu** → Load Cards | View Keys | Configure | Start Brute Force
3. **Load Cards** → File Browser → Key Details Popup → Update Counter
4. **View Keys** → Key List → Remove/Unload Options
5. **Configure** → Settings Submenu → Parameter Adjustment
6. **Start Brute Force** → Pattern Detection → Range Generation → Attack Execution

## File Structure
```
applications_user/uid_brute_smarter/
├── uid_brute_smarter.c      # Main application logic
├── pattern_engine.c         # Pattern detection implementation
├── pattern_engine.h         # Pattern detection interface
├── application.fam          # Build configuration
├── test_application.fam     # Test build configuration
├── test_uid_brute_smarter.c # Test suite implementation
├── assets/                  # Application assets
│   └── icon_10x10.txt      # Icon definition
├── views/                   # UI headers
│   └── uid_brute_smart_views.h
└── .github/workflows/       # CI/CD pipelines
    ├── test.yml            # Build and test workflow
    └── release.yml         # Release build workflow
```

## Memory Management
- **Stack Size**: 8KB (increased for enhanced functionality)
- **Max Keys**: 5 simultaneous keys with full metadata
- **Dynamic Allocation**: All key names and file paths use malloc/free
- **Cleanup**: Proper memory cleanup in `uid_brute_smarter_free()`

## Pattern Detection Capabilities
- **+1 Linear**: Sequential incrementing (0x1000, 0x1001, 0x1002...)
- **+K Linear**: Any uniform step (count >= 3); mixed power-of-two strides collapse to their GCD grid; non-power-of-two envelopes fall through to the bounded bitmask branch
- **16-bit Counter**: Upper-half counter incrementing with step 0x10000, lower 16 bits fixed
- **Bitmask**: Bounded enumeration of the varying bits (preferred for 2-sample sets)
- **Unknown**: Safe range expansion ±100 around single UID

## Hit Detection
- The NFC listener callback (`uid_brute_smarter_listener_callback`) counts reader engagement: CRC-valid commands addressed to the emulated UID are the strong acceptance signal, HLTA is a weak counter
- `Stop on Hit` (config, default on) ends the run and reports the accepted UID
- Counters are plain volatiles written from the NFC thread and read from the timer thread; keep them single-word and lock-free

## UID Handling
- 4-byte and 7-byte UIDs are supported; the pattern engine always plans over the low 4 bytes, the 3-byte manufacturer prefix and the card's ATQA/SAK are replayed verbatim
- All cards in one audit must share the UID size (loader rejects mixed sets)

## Test Suite
- Run on the host: `make -C tests NO_COVERAGE=1 test` (all 7 suites, 84 tests)
- The custom Unity in `tests/unity/` calls `setUp`/`tearDown` around each `RUN_TEST` and provides upstream-compat macros (`TEST_ASSERT_EQUAL_UINT32`, `UNITY_BEGIN`, `*_MESSAGE` variants)

## NFC File Parsing
- Supports standard `.nfc` files with ISO14443-3a data
- Extracts 4-byte UIDs from "UID:" field
- Validates file format and structure
- Handles various line endings and formats

## GitHub Actions Workflows
- **test.yml**: Builds app with Momentum Firmware, runs code quality checks
- **release.yml**: Creates release builds and packages for distribution
- Workflows automatically download and use Momentum Firmware toolchain

## Development Guidelines
- Always run from Momentum-Firmware root directory
- Use `./fbt` commands, not direct compilation
- Test memory allocation/deallocation thoroughly
- Follow Flipper Zero UI conventions for consistency

## Tested Firmware Versions

### Latest Tested: Momentum Firmware (Dev Branch)
- **Commit**: `98a1a3de6d42e0735d1cb3fe837f55e3b54ba815`
- **Date**: 2025-12-10
- **API Version**: 87.1
- **Status**: ✅ Fully compatible
- **Features Tested**:
  - About screen with morphing ASCII animation and particle system
  - NFC card loading and management
  - Pattern detection and brute force functionality
  - All menu navigation and UI interactions

### Compatibility Notes
- This is a **C application** (not JavaScript)
- **Not affected** by JS SDK 1.0 breaking changes (gui/submenu, gui/widget)
- Uses stable NFC C APIs: `nfc_listener`, `nfc_device`, `iso14443_3a`
- No backward compatibility code required for recent Momentum updates
- Clean build confirmed with latest dev branch