# UID Brute Smarter 🔐

> **Advanced NFC Key Management & Brute-Force Testing Tool for Flipper Zero**

[![Build Status](https://github.com/fbettag/uid_brute_smarter/workflows/Build%20and%20Test/badge.svg)](https://github.com/fbettag/uid_brute_smarter/actions)
[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD%203--Clause-blue.svg)](https://opensource.org/licenses/BSD-3-Clause)
[![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-Compatible-green.svg)](https://flipperzero.one/)

## 🎯 Overview

**UID Brute Smarter** is a professional-grade security research tool designed for **authorized penetration testing** and **access control system auditing**. Built on the Flipper Zero platform, it provides advanced NFC key management capabilities with intelligent pattern detection for comprehensive security assessments.

> ⚠️ **IMPORTANT**: This tool is intended for **authorized security testing only**. Users must obtain proper authorization before testing any systems they do not own or have explicit permission to test.

## ✨ Key Features

### 🔍 Enhanced Key Management
- **Smart Key Loading**: Load NFC files with detailed metadata extraction
- **Key Tracking**: Per-key metadata including filename, UID, and load time
- **Visual Key Browser**: Browse and manage loaded keys with UID display
- **Bulk Operations**: Load multiple keys or unload all with one tap
- **Memory Safety**: Proper cleanup and memory management

### 🧠 Intelligent Pattern Detection
- **Advanced Algorithms**: Detects +1, +K, and 16-bit counter patterns
- **Range Generation**: Automatically creates comprehensive test ranges
- **Pattern Validation**: Ensures generated ranges are within safe bounds
- **Configurable Limits**: Prevent excessive range generation

### ⚙️ Professional Configuration
- **Adjustable Timing**: Configurable delays between attempts (100ms-1000ms)
- **Pause Management**: Set pause intervals to prevent system overload
- **Progress Tracking**: Real-time progress display during testing
- **Safe Stopping**: Graceful interruption capabilities

### 🎛️ User Experience
- **Intuitive Interface**: Clean, professional GUI following Flipper design
- **Detailed Feedback**: Clear notifications for all operations
- **Error Handling**: Comprehensive validation and user feedback
- **Splash Screen**: Professional introduction and feature overview

## 🚀 Quick Start

### Prerequisites
- Flipper Zero with [Momentum Firmware](https://github.com/Next-Flip/Momentum-Firmware)
- USB cable for installation
- Basic familiarity with Flipper Zero operation

### Installation Methods

#### Method 1: Direct Installation (Recommended)
```bash
# Clone the repository
git clone https://github.com/fbettag/uid_brute_smarter.git
cd uid_brute_smarter

# Build and install directly to connected Flipper
./fbt launch APPSRC=uid_brute_smarter
```

#### Method 2: Manual Build
```bash
# Build the application
./fbt fap_uid_brute_smarter

# Install via USB
./fbt flash_usb

# Or create distribution package
./fbt updater_package
```

#### Method 3: Pre-built Release
1. Download the latest `.fap` file from [Releases](../../releases)
2. Copy to Flipper Zero via USB or qFlipper
3. Launch from **Apps → NFC Tools → UID Brute Smarter**

## 📖 Usage Guide

### Security Testing Workflow

#### 1. **Authorization**
- Ensure you have **explicit written authorization** for target systems
- Document scope and testing parameters
- Follow responsible disclosure practices

#### 2. **Key Collection**
```
Main Menu → Load Cards → Select NFC Files
```
- Browse to `/ext/nfc/` folder
- Select authorized test cards (.nfc files)
- Review key details popup before loading
- Monitor "Loaded X keys" confirmation

#### 3. **Configuration**
```
Main Menu → Configure
```
- **Delay**: 100-1000ms between attempts
- **Pause Every**: 0-100 attempts
- **Pause Duration**: 0-10 seconds

#### 4. **Key Management**
```
Main Menu → View Keys
```
- See loaded keys with UIDs
- Remove individual keys
- Bulk unload all keys
- Real-time count updates

#### 5. **Testing**
```
Main Menu → Start Brute Force
```
- Review generated range before starting
- Monitor progress in real-time
- Stop safely if needed
- Review results responsibly

### Example Security Test

```bash
# 1. Load test keys
Load Cards → /ext/nfc/test_cards/
# Shows: "Key 1 (A0B1C2D3) from test_card_01.nfc"

# 2. Configure for authorized testing
Configure → Delay: 500ms, Pause: 50 attempts

# 3. Review key list
View Keys → "Loaded Keys (3)"

# 4. Execute authorized test
Start Brute Force → Monitor progress
```

## 🛡️ Security & Ethics

### Authorized Use Only
- **Explicit Permission Required**: Test only systems you own or have written authorization
- **Scope Definition**: Clearly define testing boundaries with stakeholders
- **Responsible Disclosure**: Report findings through appropriate channels

### Built-in Safeguards
- **Range Limiting**: Prevents excessive brute-force attempts
- **Configurable Delays**: Protects against system overload
- **Safe Interruption**: Graceful stopping mechanisms
- **Memory Safety**: Proper cleanup prevents system issues

### Legal Compliance
- **Follow Local Laws**: Comply with all applicable cybersecurity regulations
- **Document Authorization**: Maintain records of testing permissions
- **Professional Ethics**: Adhere to security research best practices

## 🔧 Development

### Build Environment Setup
```bash
# Install Flipper Zero toolchain
git clone https://github.com/Next-Flip/Momentum-Firmware.git
cd Momentum-Firmware

# Add your app
cp -r uid_brute_smarter applications_user/

# Build
cd Momentum-Firmware
./fbt fap_uid_brute_smarter
```

### Testing
```bash
# Run all tests
./fbt test_uid_brute_smarter

# Run specific test suites
./fbt test_pattern_engine
./fbt test_key_management
./fbt test_nfc_parsing
```

### Code Structure
```
uid_brute_smarter/
├── uid_brute_smarter.c      # Main application logic
├── pattern_engine.c/h       # Pattern detection algorithms
├── application.fam          # Build configuration
├── assets/                  # UI assets and icons
├── tests/                   # Unit tests (planned)
└── docs/                    # Additional documentation
```

### Contributing
1. **Fork** the repository
2. **Create** feature branch: `git checkout -b feature/amazing-feature`
3. **Commit** changes: `git commit -m 'Add amazing feature'`
4. **Push** to branch: `git push origin feature/amazing-feature`
5. **Open** Pull Request

Please read [CONTRIBUTING.md](CONTRIBUTING.md) for detailed guidelines.

## 📊 Technical Specifications

### Memory Usage
- **Stack Size**: 8KB (increased for enhanced functionality)
- **Max Keys**: 5 simultaneous keys with full metadata
- **Range Size**: Up to 1000 UIDs per pattern
- **Memory Safety**: Full malloc/free tracking

### Pattern Detection
- **+1 Linear**: Sequential incrementing patterns
- **+K Linear**: Fixed step patterns (16, 32, 64, 100, 256)
- **16-bit Counter**: Little-endian counter patterns
- **Unknown**: Safe range expansion around provided keys

### Supported Formats
- **NFC Files**: Standard `.nfc` files with ISO14443-3a data
- **UID Length**: 4-byte UIDs (standard MIFARE)
- **Validation**: Comprehensive file format and data validation

## 📋 Troubleshooting

### Common Issues

**"Load failed! Check file"**
- Ensure file has `.nfc` extension
- Verify file contains valid ISO14443-3a data
- Check file permissions and path

**"Maximum UIDs reached"**
- Maximum 5 keys can be loaded simultaneously
- Use "Unload All" to clear keys
- Load keys in smaller batches

**Build Issues**
```bash
# Clean build
./fbt clean
./fbt fap_uid_brute_smarter

# Check firmware compatibility
./fbt sdk_tree
```

## 🤝 Support

### Getting Help
- **Issues**: [GitHub Issues](../../issues) for bug reports
- **Discussions**: [GitHub Discussions](../../discussions) for questions
- **Security**: See [SECURITY.md](SECURITY.md) for security issues

### Professional Services
For commercial security testing services, contact the maintainers for:
- Custom feature development
- Professional training programs
- Security assessment services

## 🙏 Credits

Built with love and appreciation for the amazing Flipper Zero community:

- **[Momentum Firmware](https://momentum-fw.dev/)** - The incredible firmware that makes this all possible
- **[Unleashed Firmware](https://github.com/DarkFlippers/unleashed)** - Pioneers of advanced Flipper features
- **[Flipper Zero Community](https://github.com/flipperdevices)** - For the amazing hardware and base firmware

Special thanks to all the contributors who make the Flipper ecosystem thrive! 🐬

## 📄 License

This project is licensed under the **BSD 3-Clause License** - see the [LICENSE](LICENSE) file for details.

```
BSD 3-Clause License
Copyright (c) 2024, UID Brute Smarter Contributors
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

---

<div align="center">

**🔒 Built for security researchers, by security researchers.**

*Use responsibly. Test ethically. Disclose professionally.*

</div>