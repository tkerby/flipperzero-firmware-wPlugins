# BunnyConnect

A Flipper Zero application named after the desire to connct to a BashBunny, that bridges serial communication with USB HID keyboard functionality, enabling seamless text input and command execution across connected devices.

## Overview

BunnyConnect transforms your Flipper Zero into a versatile communication hub that can:
- Establish serial connections with external devices
- Capture and display terminal output in real-time
- Send keyboard input via USB HID to connected computers
- Provide a custom on-screen keyboard for text input
- Configure serial communication parameters

## Features

### 🔗 Serial Communication
- **Multiple Baud Rates**: Support for standard baud rates including 115200, 9600, 38400, and more
- **Flow Control**: Optional hardware flow control (RTS/CTS) support
- **Configurable Parameters**: Customizable data bits, stop bits, and parity settings
- **Real-time Display**: Live terminal output with scrollable text buffer

### ⌨️ USB HID Keyboard
- **Custom Keyboard Layout**: Intuitive on-screen keyboard with QWERTY and symbol layouts
- **Instant Text Transmission**: Send typed text directly to connected computers via USB HID
- **Special Characters**: Support for symbols, numbers, and special key combinations
- **Smart Input Validation**: Built-in text validation and error handling

### 🛠️ Configuration Options
- **Device Naming**: Custom device identification
- **Connection Settings**: Flexible serial port configuration
- **Auto-connect**: Optional automatic connection on startup
- **Session Management**: Save and restore connection preferences

### 🎯 User Interface
- **Intuitive Menu System**: Easy-to-navigate interface with clear options
- **Terminal View**: Full-screen terminal output display
- **Status Indicators**: Real-time connection and data transfer status
- **Error Handling**: Comprehensive error messages and recovery options

## Installation

### Prerequisites
- Flipper Zero device with updated firmware
- ufbt (Unofficial Flipper Build Tool) installed
- USB cable or dongle for connection to target devices

### Building from Source
```bash
# Clone the repository
git clone https://github.com/yourusername/flipper-bunnyconnect.git
cd flipper-bunnyconnect

# Build the application
ufbt

# Install to Flipper Zero
ufbt launch
```

## Usage Guide

### Getting Started

1. **Launch BunnyConnect** from your Flipper Zero's Applications menu
2. **Configure Connection** - Set up serial parameters in the Config menu
3. **Connect Device** - Use the Connect option to establish serial communication
4. **Monitor Output** - View real-time data in the Terminal view
5. **Send Input** - Use the custom keyboard to send text via USB HID

### Menu Navigation

#### Main Menu
- **Connect**: Establish serial connection with configured parameters
- **Terminal**: View real-time serial output and connection status
- **Keyboard**: Access the custom on-screen keyboard for text input
- **Config**: Modify serial communication and device settings
- **Exit**: Close the application

#### Configuration Options
- **Baud Rate**: Select communication speed (115200 default)
- **Flow Control**: Enable/disable hardware flow control
- **Data Bits**: Configure data frame size (8-bit default)
- **Stop Bits**: Set stop bit configuration
- **Parity**: Choose parity checking method

### Keyboard Input

The custom keyboard provides:
- **QWERTY Layout**: Standard keyboard arrangement
- **Symbol Mode**: Access to special characters and numbers
- **Backspace**: Text editing capability
- **Enter**: Send commands or newlines
- **Layout Toggle**: Switch between character sets

### Serial Communication

BunnyConnect supports various serial devices:
- **Microcontrollers**: Arduino, Raspberry Pi, ESP32/ESP8266
- **Development Boards**: STM32, Nordic nRF series
- **Debug Interfaces**: UART debugging sessions
- **IoT Devices**: Smart sensors and modules

## Technical Details

### Architecture
- **Modular Design**: Separate components for UI, communication, and HID functionality
- **Event-Driven**: Asynchronous handling of serial data and user input
- **Memory Efficient**: Optimized buffer management for resource-constrained environment
- **Error Resilient**: Comprehensive error handling and recovery mechanisms

### Communication Protocols
- **UART**: Universal Asynchronous Receiver-Transmitter support
- **USB HID**: Human Interface Device for keyboard emulation
- **Flow Control**: Hardware and software flow control options

### Performance
- **Real-time Processing**: Minimal latency for data transmission
- **Buffer Management**: Efficient handling of incoming and outgoing data
- **Resource Usage**: Optimized for Flipper Zero's hardware constraints

## Troubleshooting

### Common Issues

#### Connection Problems
- **No Data Received**: Check baud rate and wiring connections
- **Garbled Output**: Verify serial parameters match target device
- **Connection Timeouts**: Ensure proper flow control settings

#### USB HID Issues
- **Text Not Appearing**: Verify USB HID is enabled on Flipper Zero
- **Wrong Characters**: Check keyboard layout compatibility
- **Connection Drops**: Ensure stable USB connection

#### Application Errors
- **Memory Errors**: Restart application if buffer overflow occurs
- **UI Freezing**: Check for proper event handling in custom views
- **Config Issues**: Reset to default settings if configuration becomes corrupted

### Debug Mode
Enable verbose logging by modifying the `TAG` definition in the source code for detailed troubleshooting information.

## Development

### Building
```bash
# Development build with debug symbols
ufbt DEBUG=1

# Release build optimized for size
ufbt COMPACT=1

# Clean build artifacts
ufbt clean
```

### Contributing
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

**Disclaimer**: This software is provided for educational and legitimate testing purposes only. Users are responsible for ensuring compliance with applicable laws and regulations in their jurisdiction.
