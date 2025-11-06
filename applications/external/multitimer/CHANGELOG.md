# Changelog

All notable changes to the Flipper Zero MultiTimer project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.0.0] - 2025-01-26

### Added
- ✨ **Initial release of MultiTimer app**
- 🐬 **Welcome popup** with custom 45×45 pixel dolphin mascot featuring beard and clock
- ⏱️ **Multiple timer support** - run up to 10 simultaneous timers
- 🎯 **Preset timers**: 1, 5, 10, 15, 20, 30 minutes, and 1 hour
- 🛠️ **Custom timer setup** with precise hours, minutes, seconds control
- 💾 **Persistent storage** - timers survive app restarts
- 🎮 **Intuitive controls**: pause, resume, stop functionality
- 📊 **Progress bars** showing timer completion status
- 🔊 **Audio-visual notifications** when timers complete
- 🎨 **Custom pixel art icons**:
  - Clock icon (10×10) for timer setup screens
  - Hourglass icon (10×10) for running timers
  - Detailed dolphin mascot (45×45) for welcome screen
- 📱 **Clean UI** with clear state indicators (running, paused, finished)
- 🐍 **Python icon generator** script for creating custom graphics

### Technical Details
- Built with uFBT for Flipper Zero firmware 0.86.0+
- Efficient memory usage with proper cleanup
- Robust timer management with RTC integration
- Modular code structure for easy maintenance

### Files Added
- `multitimer.c` - Main application logic
- `application.fam` - App manifest
- `create_timer_icon.py` - Icon generation script
- Custom PNG icons in `images/` directory
- Comprehensive documentation and README
