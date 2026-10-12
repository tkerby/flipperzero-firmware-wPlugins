## v2.16.1

- HW3 now keeps the car's own FSD max-speed offset and driving profile instead of overwriting them every frame, so the scroll wheel and follow distance work again. The old behaviour is available as an opt-in override, off by default.
- The Summon-enable bit (0x3FD) is set only when Summon EU Unlock is on, matching across HW3, HW4 and the Flipper. Four Extras toggles that never sent a frame were removed.
- Autopilot-state is read from the documented DBC position on HW4, and all injection pauses while the car runs in-car Autopark.
- HW4 speed profile is written to the right bits again. The old write cleared the frame's valid flag, which stopped the speed offset from working.
- OTA detection no longer mistakes the rolling counter in 0x318 for an update in progress, which could silently stop all transmitting on newer cars.
- Signal Map presets for cars with a non-standard DAS layout, and a hardware selector: Auto, Force HW4, Force HW3, Force Legacy.
- Telemetry Off (experimental), opt-in and off by default.
- Steer-jerk work: Soft Engage, Instant Engage and Minimal Inject (experimental).
- CAN Capture to the SD card and Send Test profiles with parked-only transmit.
- The ESP32 companion build also gained an opt-in Acceleration Mode, a read-only performance panel, a Wi-Fi captive portal and a Simplified/Traditional Chinese dashboard.

## v2.15

- Nag killer fires a grip pulse as soon as the car asks for hands on the wheel.
- Warning for Tesla firmware 2026.14 and newer, where transmitting can drop Autopilot.
- Ban Shield renamed to GTW Config Replay to describe what it really does.

## Older versions

Full history: https://github.com/hypery11/flipper-tesla-fsd/blob/main/changelog.md
