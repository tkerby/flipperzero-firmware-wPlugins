# Andalucía Card Support Integration

## Overview
This document describes the integration of support for Consorcio de Andalucía transit cards into the Metroflip application.

## Implementation Details

### Card Specifications
- **Card Type**: MIFARE Classic
- **Value Block Location**: Sector 9
- **Authentication**: Uses default MIFARE Classic keys (currently FFFFFFFFFFFF)
- **Balance Format**: 4-byte value block following MIFARE Classic specification

### Files Modified/Created

#### New Files
- `scenes/plugins/andalucia.c` - Main plugin implementation

#### Modified Files
- `scenes/keys.c` - Added Andalucía card verification and key management
- `scenes/keys.h` - Added CARD_TYPE_ANDALUCIA enum value
- `scenes/metroflip_scene_load.c` - Added Andalucía card recognition
- `scenes/metroflip_scene_auto.c` - Added Andalucía card recognition
- `scenes/metroflip_scene_supported.c` - Added Andalucía to supported cards list
- `application.fam` - Registered Andalucía plugin entry point

### Plugin Architecture
The Andalucía plugin follows the standard Metroflip plugin pattern:

1. **Card Detection**: `andalucia_verify()` function checks if a card is an Andalucía card
2. **Data Parsing**: `andalucia_parse()` extracts balance and card information
3. **UI Rendering**: `andalucia_render()` displays card information
4. **Entry Point**: `andalucia_plugin_ep()` serves as the plugin interface

### Key Features
- Balance reading from sector 9 value block
- Card verification using sector structure analysis
- Proper error handling for read failures
- Integration with Metroflip's plugin system

### Balance Calculation
The balance is read from the value block in sector 9 and divided by 100 to convert from centimos to euros. The current implementation assumes the standard MIFARE Classic value block format.

### Testing Notes
- The implementation is based on analysis of a card dump with balance 103.48€
- Uses default MIFARE Classic keys (may need updating with actual Andalucía keys)
- Should be tested with multiple cards to verify the balance calculation logic

### Future Improvements
1. **Key Management**: Replace default keys with actual Andalucía-specific keys if available
2. **Enhanced Parsing**: Add more detailed transaction history if accessible
3. **Card Validation**: Improve card detection logic based on more card samples
4. **UI Enhancements**: Add Andalucía-specific branding or logos

## Building and Testing
To test this integration:

1. Compile the Metroflip application with Flipper Zero SDK
2. Load the application onto a Flipper Zero device
3. Test with Andalucía transit cards
4. Verify balance reading accuracy

## Compatibility
This integration maintains compatibility with all existing Metroflip features and plugins.
