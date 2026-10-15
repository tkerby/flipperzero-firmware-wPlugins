# UID Brute Smarter 🔐

> NFC UID brute-force and access-control audit tool for the Flipper Zero — pattern detection, reader hit detection, 4- and 7-byte UID emulation.

[![Build Status](https://github.com/fbettag/uid_brute_smarter/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/fbettag/uid_brute_smarter/actions/workflows/test.yml)
[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)
[![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-Compatible-green.svg)](https://flipperzero.one/)
[![Tests](https://img.shields.io/badge/tests-84%2F84%20passing-brightgreen.svg)](#-testing)

**Keywords:** Flipper Zero app, NFC UID brute force, ISO14443-3A UID emulation, UID-only access control audit, MIFARE UID testing, card serial number pattern analysis, Momentum Firmware FAP, authorized security testing.

## 🎯 Overview

UID Brute Smarter is a security research tool for **authorized penetration testing and access-control auditing** on systems that authenticate by card UID alone. It loads real `.nfc` card dumps, detects the numbering pattern across them (sequential batches, fixed-stride grids, counters, varying-bit envelopes), and emulates candidate UIDs through the Flipper Zero's NFC listener — watching the reader back so an accepted UID is reported instead of guessed.

> ⚠️ **IMPORTANT**: This tool is intended for **authorized security testing only**. Users must obtain proper authorization before testing any systems they do not own or have explicit permission to test.

## ✨ Features

### 🔍 Key Management
- **NFC File Loading**: Load `.nfc` dumps with metadata extraction (UID, ATQA, SAK).
- **Key Tracking**: Per-key metadata including filename, UID, and load time.
- **Key Browser**: Browse and manage loaded keys with full UID display.
- **Bulk Operations**: Load multiple keys or unload all at once.
- **Memory Safety**: Proper cleanup and memory management throughout.

### 🧠 Pattern Detection
- **Advanced Algorithms**: Detects +1, +K, 16-bit counter, and bitmask patterns.
- **Generalized +K**: Any uniform stride is accepted (not just 16/32/64/100/256), and mixed power-of-two strides (16, 32, 16, ...) collapse to their GCD grid; non-power-of-two envelopes stay in the bounded bitmask branch.
- **Range Generation**: Automatically creates test ranges based on detected patterns.
- **Pattern Validation**: Ensures generated ranges are within safe bounds.
- **Configurable Limits**: Prevents excessive range generation.

### 📡 Hit Detection
- **Reader Engagement Sensing**: While a candidate UID is emulated, the listener watches for readers that select the UID and issue a CRC-valid command (e.g. an authentication attempt) — the acceptance signal for UID-only access systems.
- **Stop on Hit**: Optionally finishes the run as soon as a reader engages a candidate and reports the accepted UID (on by default, configurable).
- **Live Feedback**: The brute-force screen shows reader command and HALT counters so audits on silent readers are still observable.

### ⚙️ Configuration
- **Adjustable Timing**: Configurable delays between attempts (100–1000 ms).
- **Pause Management**: Set pause intervals to prevent system overload.
- **Progress Tracking**: Real-time progress display during testing.
- **Safe Stopping**: Graceful interruption capabilities.

## 🚀 Quick Start

### Prerequisites
- Flipper Zero with [Momentum Firmware](https://github.com/Next-Flip/Momentum-Firmware)
- USB cable for installation.
- This repository cloned into `Momentum-Firmware/applications_user/uid_brute_smarter`.

### Installation

#### Method 1: Direct Installation (Recommended)
```bash
# Clone Momentum Firmware and place UID Brute Smarter inside applications_user
git clone https://github.com/Next-Flip/Momentum-Firmware.git
cd Momentum-Firmware
git clone https://github.com/fbettag/uid_brute_smarter.git applications_user/uid_brute_smarter

# Build and install directly to a connected Flipper
./fbt launch APPSRC=applications_user/uid_brute_smarter
```

#### Method 2: Manual Build
```bash
git clone https://github.com/Next-Flip/Momentum-Firmware.git
cd Momentum-Firmware
git clone https://github.com/fbettag/uid_brute_smarter.git applications_user/uid_brute_smarter

# Build the application
./fbt fap_uid_brute_smarter
```

#### Method 3: Pre-built Release
1. Download the latest `.fap` file from [Releases](../../releases)
2. Copy to Flipper Zero via USB or qFlipper.
3. Launch from **Apps → NFC → UID Brute Smarter**

## 📖 Usage Guide

1. **Authorization**: Ensure you have **explicit written authorization** for target systems.
2. **Key Collection**: Load authorized test cards (`.nfc` files) from the `/ext/nfc/` folder — 4-byte and 7-byte UID cards are both accepted.
3. **Configuration**: Adjust delay, pause, and Stop-on-Hit settings as needed.
4. **Key Management**: View, manage, or unload keys.
5. **Testing**: Start the run and monitor progress; the reader's engagement counters update live and an accepted UID is reported as a **HIT**.

## 📊 Technical Specifications

### Runtime Limits

| Parameter | Value |
| --- | --- |
| Maximum loaded keys / UIDs | 5 |
| Maximum generated range size | 1000 UIDs |
| Emulated protocol | ISO14443-3A (NFC-A listener) |
| Supported UID lengths | 4 bytes, 7 bytes |
| Default ATQA / SAK (no card loaded) | `00 04` / `08` (4-byte), `00 44` / `08` (7-byte) |
| ATQA / SAK in audit | copied verbatim from the first loaded card |
| 7-byte UID handling | 3-byte manufacturer prefix fixed, variable low 4 bytes searched |
| Mixed UID-length key sets | rejected by the loader |
| Attempt delay | 100–1000 ms in 100 ms steps (default 500 ms) |
| Pause interval | every 0–100 attempts in steps of 10 (default off) |
| Pause duration | 0–10 s (default 3 s) |
| Stack size | 12 KB |

### Pattern Engine

| Pattern | Detection rule (deltas between consecutive UIDs) | Range generated |
| --- | --- | --- |
| **+1 Linear** | all deltas equal 1 | ±100 around the sequence, step 1 |
| **+K Linear** | any uniform stride K > 1 with ≥3 samples | ±5·K around the sequence, step K |
| **+K GCD grid** | mixed strides whose GCD is a power of two, ≥3 samples | grid with the GCD as step |
| **16-bit counter** | all deltas exactly `0x10000` | ±500 around the upper-half counter, step `0x10000`, low half fixed |
| **Bitmask** | varying bits fit a common envelope (preferred for 2-sample sets) | exhaustive enumeration of the varying bits, `2^popcount` capped at 1000 |
| **Unknown** | none of the above | ±100 around min/max of the samples |

All ranges are clamped to the 1000-UID maximum; `1 << popcount` overflow is guarded at high popcounts.

### Hit Detection Semantics

| Signal | Source | Strength | Effect |
| --- | --- | --- | --- |
| CRC-valid command addressed to the emulated UID (e.g. authentication) | `Iso14443_3aListenerEventTypeReceivedData` / `ReceivedStandardFrame` | strong (reader treats the UID as a real card) | marks the HIT; with Stop-on-Hit the run ends and the UID is reported |
| HLTA after selection | `Iso14443_3aListenerEventTypeHalted` | weak (part of routine anti-collision) | counted on the progress screen only |

Counters are written from the NFC worker thread and read by the brute timer; they are single-word lock-free volatiles.

### Supported Formats
- **NFC Files**: standard `.nfc` files with ISO14443-3a data (loaded via `nfc_device`).
- **UID Length**: 4-byte and 7-byte UIDs.

## 🧪 Testing

Host-side unit tests cover the pattern engine, NFC file parsing, key management, config callbacks, menu navigation, brute-force completion, and UI state management.

```bash
make -C tests NO_COVERAGE=1 test
```

7 suites, **84 tests, all passing**. The in-repo Unity harness runs `setUp`/`tearDown` around each test and provides upstream-compatibility macros (`TEST_ASSERT_EQUAL_UINT32`, `UNITY_BEGIN`, `*_MESSAGE` variants).

## 🔧 Compatibility

- **Firmware**: Momentum Firmware (dev), API version 87.1, fully compatible.
- **Category**: `Apps → NFC`
- Pure C application (no JS SDK dependency), uses the stable NFC C APIs (`nfc_listener`, `nfc_device`, `iso14443_3a`).

## 🤝 Support

- **Issues**: [GitHub Issues](../../issues) for bug reports.
- **Discussions**: [GitHub Discussions](../../discussions) for questions.

## 🔖 Suggested repository topics

`flipper-zero` `flipperzero-fap` `nfc` `uid` `iso14443-3a` `access-control` `security-audit` `momentum-firmware` `pentesting` `rfid`

## 🙏 Credits

- **[Momentum Firmware](https://momentum-fw.dev/)**
- **[Unleashed Firmware](https://github.com/DarkFlippers/unleashed)**
- **[Flipper Zero Community](https://github.com/flipperdevices)**

## 📄 License

This project is licensed under the **BSD 3-Clause License** — see [LICENSE](LICENSE).
