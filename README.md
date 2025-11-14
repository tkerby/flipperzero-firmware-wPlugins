# Switch Controller - Flipper Zero App

A Flipper Zero application that emulates a Nintendo Switch Pro Controller over USB with macro recording and playback capabilities.

## Features

- **Manual Controller Mode**: Use your Flipper Zero as a Nintendo Switch controller
  - D-Pad mapped to directional buttons
  - OK button → A button
  - Back button → B button
  - Long press OK → Menu for additional buttons (X, Y, L, R, ZL, ZR, +, -, Home)

- **Macro Recording**: Record button sequences with precise timing
  - Records D-Pad movements
  - Records button presses and releases
  - Timestamps for accurate playback

- **Macro Playback**: Play back recorded macros
  - Loop or one-time playback
  - Accurate timing reproduction
  - Can be stopped at any time

- **Macro Management**: Save and load macros from SD card
  - Persistent storage
  - Named macros for easy identification
  - Browse and select from saved macros

## Installation

### Prerequisites

- Flipper Zero with latest firmware
- [uFBT (micro Flipper Build Tool)](https://github.com/flipperdevices/flipperzero-ufbt) installed

### Building

1. Clone this repository:
   ```bash
   git clone https://github.com/ccyyturralde/Flipper-Zero-Joycon.git
   cd Flipper-Zero-Joycon
   ```

2. Build the app:
   ```bash
   ufbt
   ```

3. Install to Flipper Zero:
   ```bash
   ufbt launch
   ```

## Usage

### Manual Controller Mode

1. Connect your Flipper Zero to the Nintendo Switch via USB
2. Launch the Switch Controller app
3. Select "Manual Controller"
4. Control the Switch using:
   - **D-Pad**: Up/Down/Left/Right buttons
   - **A Button**: OK button
   - **B Button**: Back button
   - **Other buttons**: Long press OK to open menu

### Recording a Macro

1. Launch the Switch Controller app
2. Select "Record Macro"
3. Enter a name for your macro
4. Press OK to start recording
5. Perform the button sequence you want to record
6. Press Back to stop and save the recording

### Playing a Macro

1. Launch the Switch Controller app
2. Select "Play Macro"
3. Choose a saved macro from the list
4. The macro will play automatically (loops by default)
5. Press Back to stop playback

## Technical Details

### USB HID Implementation

The app implements a custom USB HID device that mimics the Nintendo Switch Pro Controller:
- **Vendor ID**: 0x057E (Nintendo Co., Ltd)
- **Product ID**: 0x2009 (Pro Controller)
- **HID Report**: 12-byte report format
  - 2 bytes: Button state (14 buttons)
  - 1 byte: D-Pad (HAT switch)
  - 8 bytes: Analog sticks (4 axes, 16-bit each)

### Macro File Format

Macros are stored in `/ext/apps_data/switch_controller/` with `.scm` extension:
- Magic number: `0x4D414353` ("MACR")
- Macro name (32 bytes)
- Event count (4 bytes)
- Total duration (4 bytes)
- Event array (variable length)

### Limitations

- Flipper Zero has only 6 physical buttons, so not all Switch buttons are directly accessible
- Analog stick movements are not supported in macro recording (sticks are centered)
- Some advanced controller features (rumble, gyro, NFC) are not implemented

## Button Mapping

| Flipper Button | Switch Button |
|---------------|---------------|
| Up | D-Pad Up |
| Down | D-Pad Down |
| Left | D-Pad Left |
| Right | D-Pad Right |
| OK | A Button |
| Back | B Button |
| OK (long press) | Button Menu → X, Y, L, R, ZL, ZR, +, -, Home |

## Credits

- Based on research from:
  - [dekuNukem's Nintendo Switch Reverse Engineering](https://github.com/dekuNukem/Nintendo_Switch_Reverse_Engineering)
  - [Flipper Zero Firmware](https://github.com/flipperdevices/flipperzero-firmware)
  - [Arduino JoyCon Library](https://github.com/squirelo/Arduino-JoyCon-Library-for-Nintendo-Switch)

## License

This project is provided as-is for educational purposes.

## Disclaimer

This software is for educational and research purposes only. Use responsibly and in accordance with Nintendo's terms of service and local laws.
