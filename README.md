# UID Brute Smarter 🔐

> An advanced NFC UID analysis and testing tool for the Flipper Zero.

[![Build Status](https://github.com/fbettag/uid_brute_smarter/actions/workflows/test.yml/badge.svg?branch=main)](https://github.com/fbettag/uid_brute_smarter/actions/workflows/test.yml)
[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)
[![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-Compatible-green.svg)](https://flipperzero.one/)

## 🎯 Overview

UID Brute Smarter is a security research tool for authorized penetration testing and access control system auditing. It provides advanced NFC key management and intelligent pattern detection to build and execute comprehensive security assessments.

> ⚠️ **IMPORTANT**: This tool is intended for **authorized security testing only**. Users must obtain proper authorization before testing any systems they do not own or have explicit permission to test.

## ✨ Features

### 🔍 Key Management
- **NFC File Loading**: Load NFC files with metadata extraction.
- **Key Tracking**: Per-key metadata including filename, UID, and load time.
- **Key Browser**: Browse and manage loaded keys with UID display.
- **Bulk Operations**: Load multiple keys or unload all at once.
- **Memory Safety**: Proper cleanup and memory management.

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
- **Adjustable Timing**: Configurable delays between attempts (100ms-1000ms).
- **Pause Management**: Set pause intervals to prevent system overload.
- **Progress Tracking**: Real-time progress display during testing.
- **Safe Stopping**: Graceful interruption capabilities.

### 🎛️ User Interface
- **Intuitive Interface**: Clean GUI following Flipper design principles.
- **Detailed Feedback**: Clear notifications for all operations.
- **Error Handling**: Comprehensive validation and user feedback.

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
2. **Key Collection**: Load authorized test cards (`.nfc` files) from the `/ext/nfc/` folder.
3. **Configuration**: Adjust delay and pause settings as needed.
4. **Key Management**: View, manage, or unload keys.
5. **Testing**: Start the brute-force attack and monitor progress.

## 📊 Technical Specifications

### Pattern Detection
- **+1 Linear**: Sequential incrementing patterns.
- **+K Linear**: Any uniform step, plus power-of-two GCD grids for mixed strides.
- **16-bit Counter**: Upper-half counter incrementing with step 0x10000.
- **Bitmask**: Patterns where only a few bits are changing (bounded enumeration).
- **Unknown**: Safe range expansion around provided keys.

### Supported Formats
- **NFC Files**: Standard `.nfc` files with ISO14443-3a data.
- **UID Length**: 4-byte and 7-byte UIDs (7-byte UIDs keep the 3-byte manufacturer prefix fixed and search the variable low 4 bytes; ATQA/SAK are copied from the loaded card).

## 🤝 Support

- **Issues**: [GitHub Issues](../../issues) for bug reports.
- **Discussions**: [GitHub Discussions](../../discussions) for questions.

## 🙏 Credits

- **[Momentum Firmware](https://momentum-fw.dev/)**
- **[Unleashed Firmware](https://github.com/DarkFlippers/unleashed)**
- **[Flipper Zero Community](https://github.com/flipperdevices)**

## 📄 License

This project is licensed under the **BSD 3-Clause License**.
