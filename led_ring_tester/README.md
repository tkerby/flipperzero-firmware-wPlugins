# LED Ring Tester for Flipper Zero

A comprehensive testing application for addressable LED rings (WS2812B/NeoPixel compatible) on the Flipper Zero.

## Features

- **Individual LED Test**: Cycles through each LED one at a time in white to identify dead or stuck LEDs
- **Color Sweep**: Beautiful rainbow animation that cycles through all colors
- **All On/Off Test**: Tests all LEDs simultaneously cycling through Red, Green, Blue, White, and Off states
- **Brightness Test**: Smooth fade in/out effect to test LED dimming capabilities
- **Quick Test**: Automated test that runs through all test modes in sequence
- **Configuration**: Customize GPIO pin, LED count, and brightness settings

## Wiring Instructions

### Basic Wiring

Connect your LED ring to the Flipper Zero GPIO pins as follows:

```
LED Ring          Flipper Zero
--------          ------------
VCC/5V     --->   Pin 1 (5V)
GND        --->   Pin 8 or 18 (GND)
DIN/Data   --->   Configurable GPIO (default: Pin 7/PC3)
```

### GPIO Pin Layout

Available GPIO pins for LED data:
- **Pin 2** - PC0
- **Pin 3** - PC1
- **Pin 4** - PB3
- **Pin 5** - PA4
- **Pin 6** - PB2
- **Pin 7** - PC3 (Default)
- **Pin 9** - PA6

Refer to the Flipper Zero GPIO pinout diagram for exact pin locations.

### Important Notes

1. **Power Considerations**:
   - Small LED rings (8-12 LEDs) can usually be powered directly from Flipper's 5V pin
   - Larger rings (16+ LEDs) or high brightness settings may require external power
   - If using external power, connect grounds together (Flipper GND to LED GND)

2. **LED Compatibility**:
   - Works with WS2812, WS2812B, WS2813, and most NeoPixel-compatible LEDs
   - Default timing is optimized for WS2812B

3. **Safety**:
   - Automatic 2-minute timeout on all test modes
   - LEDs turn off when exiting any test mode
   - Start with lower brightness settings to avoid overloading the Flipper's power supply

## Usage

1. **Configuration**:
   - Select "Configuration" from the main menu
   - Choose your GPIO pin (where LED data wire is connected)
   - Select LED count (8, 12, 16, 24, or 32 LEDs)
   - Adjust brightness (10% to 100%)

2. **Running Tests**:
   - **Individual LED Test**: Watch each LED light up sequentially
   - **Color Sweep**: View a smooth rainbow animation
   - **All On/Off Test**: Check if all LEDs can display each color
   - **Brightness Test**: Verify smooth dimming capability
   - **Quick Test**: Run all tests automatically (about 15 seconds)

3. **Controls**:
   - **Back Button**: Exit current test or return to previous menu
   - Tests run continuously until you press Back

## Building

### For Official Firmware

```bash
./fbt fap_led_ring_tester
```

### For Unleashed/RogueMaster

```bash
./fbt fap_led_ring_tester
```

The compiled `.fap` file will be in `build/f7-firmware-D/.extapps/led_ring_tester.fap`

## Installation

1. Copy the compiled `.fap` file to your Flipper Zero:
   - SD Card path: `/ext/apps/GPIO/led_ring_tester.fap`

2. Or use qFlipper to install the app

## Troubleshooting

### LEDs don't light up
- Check wiring connections
- Verify GPIO pin selection matches your wiring
- Try a different GPIO pin
- Check LED count setting matches your hardware
- Ensure your LED ring is compatible (WS2812B/NeoPixel)

### LEDs show wrong colors
- This is normal - color order can vary by LED model
- The test patterns should still be visible

### LEDs flicker or behave erratically
- Try lowering the brightness setting
- Use external power for larger LED rings
- Check for loose connections

### Power issues
- Reduce brightness in Configuration menu
- Use external 5V power supply for rings with 16+ LEDs
- Ensure good quality USB cable if powering Flipper via USB

## Technical Details

- **LED Protocol**: WS2812B (GRB color order)
- **Timing**: Bit-banging implementation with critical sections
- **Update Rate**: Varies by test mode (30-500ms)
- **Safety Timeout**: 2 minutes per test session
- **Maximum LEDs**: Configured up to 32 (can be extended in code)

## License

This application is open source and free to use.

## Contributing

Issues and pull requests are welcome!

## Credits

Created by Claude for the Flipper Zero community.
