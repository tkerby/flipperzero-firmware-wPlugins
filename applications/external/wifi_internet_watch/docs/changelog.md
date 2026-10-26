v1.3:
- Added support for multiple devboard firmwares: ESP-AT, GhostESP, Marauder, and Bruce
- Added automatic devboard firmware detection at startup with a manual override in the settings menu
- Added a GhostESP serial console driver for scanning, joining, and internet checks
- Reports Marauder and Bruce as recognized but unsupported for monitoring instead of failing silently
- Added a Black Magic devboard mode that explains the factory debug-probe firmware has no Wi-Fi support
- Moved the devboard serial protocol into a portable module with host-side unit tests
- Changed the default internet check target to 8.8.8.8
- Fixed a false positive internet check when ESP-AT answers AT+PING with an error
- Rejected malformed IPv4 addresses such as 999.1.1.1 when reading devboard status
- Migrated saved settings from the previous version so the configured ping IP and timer survive the upgrade

v1.2:
- Added a settings menu with editable ping IP and internet check timer
- Added reset behavior that restores the default IP and timer and clears remembered Wi-Fi networks
- Added return from the monitoring screen back to Wi-Fi scanning with the Back button
- Updated LED behavior: steady green when internet is online, 1-second red blinking while offline
- Polished the settings screen header and selection visuals
- Added validation for saved ping IP settings with fallback to the default target

v1.1:
- Added persistent storage for up to 16 Wi-Fi credentials
- Added one-button connection to saved networks
- Added password editing and saved-network deletion
- Added a full lowercase, uppercase, numeric, and symbol keyboard
- Added waiting, reconnect, and recovery notifications for internet monitoring
- Added 1.1.1.1 as the default ping target

v1.0:
- Added ESP-AT Wi-Fi scanning and network selection
- Added internet availability checks every 60 seconds
- Added visual status and notification when connectivity returns
