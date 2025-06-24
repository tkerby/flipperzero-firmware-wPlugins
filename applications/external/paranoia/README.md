# Paranoia Mode for Flipper Zero

<p align="center">
  <img src="images/paranoia.png" alt="Paranoia Mode Logo" width="100"/>
</p>

<div align="center">
  
  [![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
  ![Version](https://img.shields.io/badge/version-0.1-blue)
  [![FlipC.org](https://flipc.org/badge?name=flipper-paranoia)](https://flipc.org)
  
</div>

## Overview

Paranoia Mode is an anti-surveillance tool for Flipper Zero that scans for hidden wireless cameras, RFID skimmers, and infrared monitoring devices. Perfect for travelers, privacy enthusiasts, or anyone concerned about electronic eavesdropping.

## Features

- **RF Scanning**: Detect hidden wireless cameras and transmitters
- **NFC Scanning**: Find RFID skimmers and unauthorized NFC devices
- **IR Scanning**: Identify infrared surveillance equipment
- **Adjustable Sensitivity**: Customize detection thresholds (Low/Medium/High)
- **Real-time Alerts**: Visual and haptic notifications when threats are detected
- **Threat Level Assessment**: Quantifies the severity of detected surveillance

## Usage

1. **Launch the app** from the "Apps" menu under "Tools"
2. **Configure settings** by pressing the left button to access the Options menu
   - Enable/disable RF, NFC, and IR scanning
   - Adjust sensitivity level
3. **Start scanning** by pressing the center button
   - The screen will display current scan progress
   - Detection results appear at the bottom of the screen
4. **Stop scanning** by pressing the center button again
5. **View information** about the app by pressing the right button

### Controls

| Button | Action |
|--------|--------|
| Center | Start/Stop Scan |
| Left   | Options Menu |  
| Right  | Info Screen |
| Up     | Return to Idle |
| Back   | Exit App |

## How It Works

Paranoia Mode leverages the Flipper Zero's hardware capabilities:

- **RF Detection**: Uses SubGHz module to detect unusual radio frequency activity
- **NFC Detection**: Monitors for NFC field presence that might indicate skimmers
- **IR Detection**: Employs the IR receiver to detect infrared signals from surveillance equipment

The app calculates a threat level based on detected anomalies and their intensity.

## Technical Details

- Written in C for the Flipper Zero platform
- Uses Flipper's hardware APIs including SubGHz, NFC, and IR modules
- Memory-efficient design for optimal performance
- User-friendly interface with real-time feedback

## Limitations

- False positives may occur in areas with high RF activity
- Detection range varies based on device power and environmental factors
- Not guaranteed to detect all surveillance equipment

## License

This project is licensed under the MIT License - see the LICENSE file for details.

## Credits

- Developed by [C0d3-5t3w](https://github.com/C0d3-5t3w)

