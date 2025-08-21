# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview
Flipper Zero Bluetooth Govee Smart LED Controller - A native Flipper Zero application for controlling Govee BLE-enabled LED devices.

## Development Setup

### Build System
The project will use the Flipper Zero SDK. Common commands will include:
- Build: `./fbt` or `ufbt` 
- Flash to device: `./fbt flash` or `ufbt flash`
- Debug: `./fbt debug` or `ufbt debug`
- Run specific app: `./fbt launch APPSRC=applications_user/govee_control`

### Testing
- Unit tests: `./fbt test`
- Run on Flipper emulator: `./fbt qflipper`

## Architecture

### Core Components
1. **BLE Manager** - Handles device discovery, connections, and command queuing
2. **Device Abstraction Layer** - Model-specific drivers for different Govee devices
3. **Scene Engine** - Manages scenes, effects, and transitions
4. **Storage System** - Persistent storage for device configs and scenes

### Govee BLE Protocol
- Service UUID: `0000180a-0000-1000-8000-00805f9b34fb`
- Control Characteristic: `00010203-0405-0607-0809-0a0b0c0d1910`
- Status Characteristic: `00010203-0405-0607-0809-0a0b0c0d1911`
- Commands use 20-byte packets starting with 0x33 header, ending with XOR checksum

### Key Protocol Commands
- Power On: `0x33 0x01 0x01 ... [XOR]`
- Power Off: `0x33 0x01 0x00 ... [XOR]`
- Set Color: `0x33 0x05 0x02 [R] [G] [B] ... [XOR]`
- Set Brightness: `0x33 0x04 [VALUE] ... [XOR]`

## Implementation Status
Currently in planning phase. Next steps:
1. Set up Flipper Zero SDK and development environment
2. Implement Phase 1: BLE scanner, basic connection, on/off control
3. Target performance: <100ms latency, <256KB RAM, support 5+ devices

## Supported Devices
Primary targets: H6160, H6163, H6104, H6110, H6135, H6159, H6195

## Important Constraints
- Memory budget: <256KB RAM, <1MB storage
- Battery impact target: <10% per hour active use
- Must support 5+ simultaneous BLE connections
- No cloud dependencies for core functionality