# Flipper Keylogger

A Flipper Zero application for keystroke logging and data retrieval using BadUSB (HID) payloads and DuckyScript.

## Overview

Flipper Keylogger deploys a PowerShell-based keylogger onto a target Windows PC via HID keyboard emulation, then retrieves the captured keystrokes back to the Flipper. The app includes a built-in keylog viewer for reading captured data directly on the device.

## Features

- **Two-phase operation** — deploy the keylogger, come back later to retrieve
- **Persistent installation** — survives reboots via Startup folder batch file
- **Clean keylog capture** — 5-second startup delay and ordered execution prevent payload commands from polluting the log
- **Multiple payload variants** — standard, CMD-launched, and tester versions
- **Built-in keylog browser** — browse and read captured files on the Flipper screen with line wrapping
- **UFBT file transfer** — scheduled task transfers keylogs back to the Flipper via serial
- **Self-cleaning** — retrieve payload removes the keylogger, persistence, and signal files

## Build & Install

Compile with [ufbt](https://github.com/flipperdevices/flipperzero-ufbt):

```bash
ufbt
```

Deploy to a connected Flipper:

```bash
ufbt launch
```

Upload payload files to the Flipper's SD card

## Directory Structure

```
/ext/apps_data/keylogger/
├── payloads/
│   ├── deploy_keylogger.txt          # Standard deploy
│   ├── deploy_keylogger_cmd.txt      # CMD-launched deploy (if direct PS is blocked)
│   ├── retrieve_keylog.txt           # Standard retrieve
│   └── retrieve_keylog_cmd.txt       # CMD-launched retrieve  
└── keylogs/                          # Retrieved keylog files appear here
```

## Usage

### 1. Deploy

1. Open **Apps > USB > Keylogger** on the Flipper
2. Select a deploy payload (or use the default `deploy_keylogger.txt`)
3. Plug the Flipper into the target PC — the payload executes automatically
4. The payload:
   - Opens PowerShell via Win+R
   - Writes `logger.ps1` (GetAsyncKeyState-based keylogger) to `%TEMP%`
   - Creates startup persistence (`svchost.bat` in the Startup folder)
   - Writes a completion signal (`badusb_status.txt`)
   - Launches the keylogger as an independent hidden process (`Start-Process`)
   - Exits PowerShell — the keylogger keeps running
5. Disconnect the Flipper. The keylogger runs silently in the background.

### 2. Retrieve

1. Plug the Flipper back into the target PC
2. Select a retrieve payload and execute
3. The payload:
   - Opens PowerShell and immediately kills the keylogger process
   - Copies the keylog with a timestamp (`keylog_YYYYMMDD_HHMMSS.log`)
   - Creates a transfer batch file and schedules it via `schtasks` (2-minute delay)
   - Cleans up: removes `logger.ps1`, `svchost.log`, `badusb_status.txt`, and the startup entry
4. Disconnect the Flipper and switch to serial/normal USB mode
5. The scheduled task runs the transfer batch, which uses UFBT's `storage.py` to send the keylog to `/ext/apps_data/keylogger/keylogs/` on the Flipper

### 3. View Keylogs

1. Select **View Keylogs** from the main menu
2. Browse the list of captured keylog files
3. Press OK to view — content is displayed with automatic line wrapping
4. Use left/right to page through longer files

## Payload Variants

| Variant | When to use |
|---------|-------------|
| `deploy_keylogger.txt` | Standard deploy — opens PowerShell directly via Win+R |
| `deploy_keylogger_cmd.txt` | Target blocks direct PowerShell launch — goes through `cmd /k` first |
| `retrieve_keylog.txt` | Standard retrieve |
| `retrieve_keylog_cmd.txt` | CMD-launched retrieve |

## How the Keylogger Works

The deployed `logger.ps1` script:

1. Loads `user32.dll` via `Add-Type` for `GetAsyncKeyState`
2. Waits 5 seconds before starting (so deploy payload commands aren't captured)
3. Polls all virtual key codes (8–255) every millisecond
4. Maps keys to readable output — letters respect Shift/CapsLock, special keys become `[BS]`, `[TAB]`, `[ENTER]`, etc.
5. Buffers keystrokes and flushes to `%TEMP%\svchost.log` every 3 seconds or every 30 characters
6. Each line is timestamped: `[HH:MM:SS] captured text here`

## Troubleshooting

**No keylog file found**
- Ensure the deploy payload ran successfully first
- Check that enough time passed for keystrokes to be captured
- Verify PowerShell is available on the target

**Transfer fails**
- UFBT must be installed on the target (`pip install ufbt`)
- The Flipper must be connected via USB serial when the scheduled task runs (2 minutes after retrieve)
- If transfer fails, the keylog is saved at `%TEMP%\keylog_*.log` on the target

**Payload doesn't type correctly**
- Verify the target uses a US keyboard layout — HID keycodes assume US QWERTY
- Check that the target has focus on the Run dialog / PowerShell window
- Increase `DELAY` values if the target PC is slow

**App crashes when viewing keylogs**
- Ensure keylogs exist in `/ext/apps_data/keylogger/keylogs/` on the SD card
- Very large files may take a moment to load (background thread)

## Legal

This application is for authorized security testing and educational purposes only. Only use on systems you own or have explicit written permission to test. Comply with all applicable laws. Always clean up after testing.

## Credits

Developed by dag nazty for Flipper Zero.
