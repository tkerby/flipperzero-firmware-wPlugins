# 🐬⏰ Flipper Zero MultiTimer

<div align="center">

![Flipper Zero](https://img.shields.io/badge/Flipper%20Zero-Compatible-orange?style=for-the-badge)
![Build Status](https://img.shields.io/badge/Build-Passing-brightgreen?style=for-the-badge)
![License](https://img.shields.io/badge/License-MIT-blue?style=for-the-badge)

*A feature-rich multi-timer application for Flipper Zero with a charming dolphin mascot!*

[Features](#-features) • [Installation](#-installation) • [Usage](#-usage) • [Development](#-development) • [Screenshots](#-screenshots)

</div>

## 🌟 Features

### ⏱️ **Multiple Timer Management**
- **Run up to 10 simultaneous timers** - Perfect for cooking, workouts, or productivity sessions
- **Preset quick timers**: 1, 5, 10, 15, 20, 30 minutes, and 1 hour
- **Custom timer setup** with hours, minutes, and seconds precision
- **Persistent storage** - Your timers survive app restarts and device reboots

### 🎮 **Intuitive Interface**
- **Welcome screen** featuring our bearded dolphin mascot with a clock
- **Visual progress bars** showing timer completion status
- **Clear state indicators**: Running (▶), Paused (⏸), Finished (!)
- **Easy navigation** with Flipper Zero's directional pad and buttons

### 🔊 **Smart Notifications**
- **Audio-visual alerts** when timers complete
- **Pause/Resume functionality** for flexible timer management
- **Auto-dismiss** finished timers or manual acknowledgment

## 🚀 Installation

Using uFBT

```bash
# Clone the repository
git clone https://github.com/yourusername/flipper-multitimer.git
cd flipper-multitimer

# Build the application
ufbt

# Install to your Flipper Zero
ufbt launch
```

## 🎯 Usage

### Starting the App
1. Navigate to **Apps → Tools → MultiTimer** on your Flipper Zero
2. Enjoy the welcome screen with our friendly dolphin mascot
3. The main menu will appear after 3 seconds (or press any key)

### Quick Timer Setup
1. Select a preset timer from the main menu (1 min, 5 min, etc.)
2. Timer starts immediately and shows progress
3. Use **OK** to pause/resume, **Back** to stop

### Custom Timer Setup
1. Select **"Custom Timer"** from the main menu
2. Use **Left/Right** to adjust time values
3. Use **Up/Down** to switch between hours, minutes, seconds
4. Press **OK** to start the timer

### Managing Multiple Timers
1. Select **"View Active Timers"** to see all running timers
2. Each timer shows its state and remaining time
3. Navigate back to start additional timers
4. Up to 10 timers can run simultaneously

### Timer Controls
- **OK Button**: Pause/Resume active timer
- **Back Button**: Stop timer and return to menu
- **Navigation**: Switch between time fields in setup

## 🛠️ Development

### Prerequisites
- [uFBT](https://github.com/flipperdevices/flipperzero-ufbt) installed
- Flipper Zero firmware 0.86.0 or later

### Building from Source

```bash
# Clone repository
git clone https://github.com/yourusername/flipper-multitimer.git
cd flipper-multitimer

# Generate custom icons (optional)
python -m venv .venv
source .venv/bin/activate  # On Windows: .venv\Scripts\activate
pip install pillow
python create_timer_icon.py

# Build application
ufbt

# Flash to Flipper Zero
ufbt launch
```

## 📝 License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

🐬

---
