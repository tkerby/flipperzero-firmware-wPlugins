# Flipper Zero Govee BLE Controller

A native Flipper Zero application for comprehensive control of Govee Bluetooth-enabled LED devices, providing advanced lighting automation, scene management, and multi-device orchestration capabilities.

## Features

### Core Functionality
- **Multi-Device Control** - Manage 5+ Govee LED devices simultaneously
- **Device Discovery** - Automatic BLE scanning with RSSI monitoring
- **Color Management** - Full RGB/HSV color control with temperature adjustment
- **Effects Library** - Built-in Govee effects with custom animation support
- **Scene Engine** - Pre-configured and custom scenes with scheduling
- **Group Control** - Organize devices into logical groups with synchronized commands
- **Automation** - Time-based schedules, triggers, and macro recording

### Advanced Capabilities
- **Protocol Analysis** - BLE packet capture and command logging tools
- **Offline Operation** - No cloud dependency for core functionality
- **Persistent Storage** - Device configurations and scene library backup
- **Low Latency** - Sub-100ms command execution
- **Battery Optimized** - Less than 10% battery impact per hour of active use

## Supported Devices

| Model | Device Type | Features | Status |
|-------|------------|----------|--------|
| H6006 | Smart A19 LED Bulb | RGBWW, 2000K-9000K | Priority |
| H6160 | LED Strip Lights | RGB, Effects | Planned |
| H6163 | LED Strip Lights Pro | RGB, Segments, Gradient | Planned |
| H6104 | LED TV Backlight | RGB, Segments | Planned |
| H6110 | Smart Bulb | RGBWW | Planned |
| H6135 | Smart Light Bar | RGB, Effects | Planned |
| H6159 | Gaming Light Panels | RGB, Segments | Planned |
| H6195 | Immersion Light Strip | RGB, Effects | Planned |

## Requirements

- Flipper Zero device
- Firmware version: Latest stable or development build
- Bluetooth enabled
- At least 1MB free storage

## Installation

### Method 1: Flipper Application Catalog
Coming soon - The application will be available through the official Flipper catalog.

### Method 2: Build from Source

1. Clone the repository:
```bash
git clone https://github.com/yourusername/flipper-govee-controller.git
cd flipper-govee-controller
```

2. Set up the Flipper Zero SDK:
```bash
# Install the SDK
git clone --recursive https://github.com/flipperdevices/flipperzero-firmware.git
cd flipperzero-firmware
```

3. Build the application:
```bash
./fbt fap_govee_control
```

4. Deploy to Flipper Zero:
```bash
./fbt launch APPSRC=applications_user/govee_control
```

### Method 3: Pre-built FAP
Download the latest `.fap` file from the releases page and copy it to your Flipper Zero's SD card under `/apps/Misc/`.

## Usage

### Quick Start

1. Launch the application from the Flipper Zero menu
2. Select "Quick Control" for immediate access to nearby devices
3. Choose a device from the discovered list
4. Use the D-pad to adjust colors and brightness

### Navigation Controls

| Button | Action |
|--------|--------|
| Up/Down | Navigate menu items |
| Left/Right | Adjust values |
| OK | Select/Apply |
| Back | Return to previous menu |
| Hold OK | Quick action menu |
| Hold Back | Exit application |

### Main Menu Options

- **Quick Control** - Fast access to last used device
- **Devices** - Manage and configure connected devices
- **Scenes** - Create and activate lighting scenes
- **Schedule** - Set up automated lighting schedules
- **Settings** - Configure application preferences

## Technical Architecture

### System Components

```
┌─────────────────────────────────────────┐
│            Application Layer             │
├─────────────────────────────────────────┤
│     Scene Engine    │    UI Manager      │
├────────────┬────────┴────────────────────┤
│  Device Abstraction Layer (DAL)          │
├───────────────────────────────────────────┤
│         BLE Manager                       │
│  ┌──────────┬──────────┬──────────┐     │
│  │Discovery │Connection│ Command  │     │
│  │ Service  │   Pool   │  Queue   │     │
│  └──────────┴──────────┴──────────┘     │
├───────────────────────────────────────────┤
│         Flipper Zero BLE Stack           │
└───────────────────────────────────────────┘
```

### BLE Protocol

The application communicates with Govee devices using the following protocol:

- **Service UUID**: `0000180a-0000-1000-8000-00805f9b34fb`
- **Control Characteristic**: `00010203-0405-0607-0809-0a0b0c0d1910`
- **Status Characteristic**: `00010203-0405-0607-0809-0a0b0c0d1911`

#### Command Structure
All commands follow a 20-byte packet format:
- Byte 0: Header (0x33)
- Byte 1: Command type
- Bytes 2-18: Payload
- Byte 19: XOR checksum

## Development

### Prerequisites

- Flipper Zero SDK or ufbt
- ARM GCC toolchain
- Python 3.8+
- Git

### Project Structure

```
flipper-govee-controller/
├── src/
│   ├── ble/              # BLE communication layer
│   ├── devices/          # Device-specific drivers
│   ├── scenes/           # Scene management
│   ├── ui/               # User interface
│   └── main.c            # Application entry point
├── assets/               # Icons and resources
├── docs/                 # Documentation
└── tests/               # Unit tests
```

### Building

```bash
# Debug build
./fbt debug

# Release build
./fbt fap_govee_control

# Run tests
./fbt test
```

### Debugging

```bash
# Start debugging session
./fbt debug

# View logs
./fbt cli
> log
```

## Contributing

Contributions are welcome! Please follow these guidelines:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Code Standards

- Follow the existing code style
- Add unit tests for new functionality
- Update documentation as needed
- Ensure all tests pass before submitting

## Roadmap

### Phase 1: Foundation
- [x] Project setup and documentation
- [ ] BLE scanner implementation
- [ ] Basic device connection
- [ ] Simple on/off control

### Phase 2: Core Features
- [ ] Color and brightness control
- [ ] Multi-device support
- [ ] Group management
- [ ] Basic scenes

### Phase 3: Advanced Control
- [ ] Effects library
- [ ] Custom animations
- [ ] Scene sequencing
- [ ] Scheduling system

### Phase 4: Polish
- [ ] Performance optimization
- [ ] Battery usage improvements
- [ ] Enhanced error handling
- [ ] User documentation

## Performance Metrics

| Metric | Target | Current |
|--------|--------|---------|
| Device Discovery | < 3 seconds | TBD |
| Command Latency | < 100ms | TBD |
| Multi-device Sync | < 50ms deviation | TBD |
| Memory Usage | < 256KB RAM | TBD |
| Storage | < 1MB | TBD |
| Battery Impact | < 10% per hour | TBD |

## Security

- All device credentials are stored encrypted on the Flipper Zero
- No cloud connectivity required for operation
- BLE pairing follows standard security protocols
- Local-first architecture ensures privacy

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Flipper Zero team for the excellent SDK and documentation
- Govee community for protocol reverse engineering efforts
- Contributors and testers who help improve this application

## Support

For bugs and feature requests, please open an issue on GitHub.

## Disclaimer

This is an unofficial application and is not affiliated with, endorsed by, or sponsored by Govee or Flipper Devices Inc. All product names, logos, and brands are property of their respective owners.