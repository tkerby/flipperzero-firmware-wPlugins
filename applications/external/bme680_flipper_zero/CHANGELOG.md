# Changelog

All notable changes to the BME680 Gas Sensor project will be documented in this file.

## [1.2] - 2025-11-09

### Added / Changed
- Commented out the test option in the UI and configuration (code retained, not removed).
- About screen is now scrollable — shows 4 lines per page; navigation: → opens, ↑/↓ scroll pages.
- Set the application author to "Dr.Mosfet" (header and package metadata).
- Simplified code comments and translated them to concise English.
- Added a key press click sound — short A5 (~880 Hz) tone for ~50 ms; louder and more audible.
- Verification: project built and packaged locally (fap) after the changes.

### Notes
- When adding persistent settings (e.g., click volume), it's recommended to bump the configuration version and add migration logic.

## [1.2] - 2025-11-02

### Added
- **Gas alarm system:** Blinking red LED + sound alert when dangerous gases detected
- **Automatic stabilization:** 30-second stabilization period after gas sensor startup
- **Cooldown period:** 60-second pause between measurement sessions
- **Configurable thresholds:** User can set gas alarm threshold (1-1000 kΩ)
- **Safety status:** Card showing current gas safety state
- **Green LED indicator:** Solid green LED for safe gas levels
- **Scrollable interface:** Ability to scroll through measurement list with ↑/↓ arrows
- **Scroll bar:** Visual indicator of position in the list
- **Icon legend:** Screen with explanations of all icons and author information
- **Legend panning:** 2D scrolling capability in legend screen
- **Dark mode:** Toggle between light and dark themes
- **Altitude configuration:** Set altitude above sea level for pressure correction
- **Dew point:** Automatic calculation of dew point from temperature and humidity
- **Sea level pressure:** Convert local pressure to sea level pressure
- **Bitmap icons:** Custom 10x10px icons for each measurement type
- **Persistent settings:** Save configuration to file on SD card
- **RTC support:** Use real-time clock for precise timing measurements

### Fixed
- **Compilation stability:** Fixed function declaration ordering
- **Memory management:** Improved mutex and resource management
- **Safety logic:** Corrected dangerous gas detection logic
- **LED cleanup:** Proper LED notification shutdown when closing application
- **Error handling:** Better I2C communication error handling
- **User interface:** Cleaner, more intuitive navigation

### Changed
- **Application structure:** Rewrote architecture for better modularity
- **Notification system:** Integrated with Flipper Zero notification system
- **Application category:** Changed from "GPIO" to "Sensors" for better classification
- **Stack size:** Increased to 4KB to handle additional features

## [1.0] - 2024-XX-XX

### Added
- Basic BME680 sensor readings (temperature, pressure, humidity, gas)
- I2C communication with sensor
- Basic user interface
- I2C address configuration
- Gas sensor enable/disable