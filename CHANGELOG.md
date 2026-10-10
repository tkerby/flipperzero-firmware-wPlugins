# Changelog

## 2.0

Home Assistant is now the required smart-home backend.

- Replaced the legacy companion-relay/Shelly architecture with direct Home Assistant REST API communication through FlipperHTTP
- Added generic Home Assistant `switch.*` support
- Retained and moved `climate.*` thermostat control to the direct Home Assistant path
- Added Home Assistant `light.*` support with Status, On, Off, Toggle, and brightness presets
- Added an RGB Solid submenu with expanded color presets
- Added a full WLED Effects submenu; effect availability ultimately depends on the selected light entity
- Added non-blocking Home Assistant light requests so network waits do not run on the GUI thread
- Added a dedicated light Status view showing state, brightness, and effect
- Fixed large Home Assistant light-state responses that span multiple FlipperHTTP UART chunks
- Added a custom Home Assistant URL keyboard
- Added editable Home Assistant long-lived access-token configuration
- Added optional SD-card Home Assistant configuration import
- Added safer missing-configuration and result-dialog navigation
- Simplified device storage to Home Assistant entities only
- Redacted authenticated request headers from FlipperHTTP logs
- Fixed FlipperHTTP `last_response` ownership when the Wi-Fi board is inactive

## 1.1

- Added Home Assistant climate/thermostat support

## 1.0

- Initial public release
