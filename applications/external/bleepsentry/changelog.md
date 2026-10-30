## 1.1

- Calibration screens no longer prompt "No baseline (press OK)" while a
  calibration is already running. During Watch or Sweep calibration the
  line now says what the device is doing, so it cannot invite a key press
  that would restart the calibration.
- The RSSI readout and the level bar now share one range (-115 to -25 dBm),
  so the number can no longer print a value outside the meter's scale with
  an empty bar underneath it.
- The status line moved from the last pixel row to one row above it, so
  descenders no longer sit on the screen edge.

## 1.0

- First catalog-ready release. Screenshots captured on the device running
  official firmware 1.5.1 (API 88.2), one frame per UI state: idle, Watch
  mode after calibration, Sweep mode calibrating, and Sweep mode actively
  watching. All four are 512x256 two-colour frames in the catalog's
  screenshot format; capture details are in screenshots/README.md.
- Switched on-device input testing to the proper RPC input path after
  finding the plain debug CLI's input-send command doesn't reliably reach
  a running app; RPC input and a physical button press both confirmed
  working, including clean app exit.

## 0.7

- Verified on real hardware (Flipper Zero, official firmware 1.5.1, API
  88.2): clean install/launch, Watch-mode calibration, full Sweep-mode
  cycle across all 4 frequencies, and clean exit via physical Back press,
  all confirmed with no crash, via on-device thread/loader inspection.
- Documented a known non-defect quirk: remote "close current app" RPC
  requests report "has to be closed manually," expected for the plain
  ViewPort app pattern (vs. ViewDispatcher); physical Back press is
  unaffected.

## 0.6

- Fixed stale baseline/threshold display: starting a sweep calibration now
  clears the on-screen baseline/threshold immediately instead of showing
  numbers left over from a previous manual (Watch mode) calibration while
  the sweep calibration is still in progress.

## 0.5

- Fixed a concurrency bug found by code review: the alert-onset vibration
  pulse did a blocking 50ms delay while holding the mutex the GUI thread's
  draw callback also needs, stalling screen updates on every single alert.
  Moved the vibro pulse outside the mutex-held section.

## 0.4

- Extracted pure logic (threshold math, alert state transition, frequency
  index wrap, CSV line formatting) into a host-testable module, free of
  any Flipper SDK dependency, with a host-side unit test suite that runs
  on a normal machine (no device needed) and is wired into CI. This does
  not substitute for on-device testing of GUI/radio/timing behavior, but
  it does prove the math and formatting are correct.
- Explicitly excluded the test suite from the embedded build after
  finding the default source glob is recursive and was pulling the host
  test file into the actual app binary. Caught via a clean rebuild, fixed
  before any hardware testing could have surfaced it.
- Ran the SDK's linter and formatter for the first time; fixed all
  violations.

## 0.3

- Added sweep mode: long-press OK calibrates a baseline/threshold across all
  four frequencies in sequence, then continuously cycles through them
  (dwell-watch), alerting and logging independently per frequency. This is
  the actual multi-band TSCM sweep workflow; the single-frequency Watch mode
  (short-press OK) remains for red-team monitoring of one known frequency.
- Up/Down now preserves any previously-calibrated baseline per frequency
  instead of discarding it on every switch.
- Builds clean against SDK API 88.2. Not yet tested on physical hardware.

## 0.2

- Pivoted from BLE passive scanning to Sub-GHz RF anomaly sentry. The public
  Flipper SDK's BLE stack exposes peripheral/advertiser functions only; there
  is no central/observer scan API available to apps, so the original concept
  was not buildable. Verified against the SDK's Bluetooth headers before
  pivoting.
- Implemented core RSSI-threshold alert engine: frequency selection (300,
  433.92, 868.35, 915 MHz), baseline calibration, live RSSI bar, vibration
  alert on threshold breach.
- Builds clean against SDK API 88.2. Not yet tested on physical hardware.

## 0.1

- Initial scaffold.
