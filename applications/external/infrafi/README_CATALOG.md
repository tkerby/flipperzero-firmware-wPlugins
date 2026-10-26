# InfraFi

Transmit WiFi credentials from a **Flipper Zero** to a **Linux server** using infrared. Point, press Send, connected.

Built for headless servers (NAS boxes, Intel NUCs, etc.) where typing WiFi passwords is painful or impossible.

## How It Works

The Flipper encodes WiFi credentials as a sequence of **IR messages** (RC-6 or NEC) and blasts them at the server's IR receiver. The daemon decodes the transmission and connects to the network automatically. No pairing, no Bluetooth, no network required. Just line-of-sight IR.

With **ACK enabled**, the daemon transmits a response back via IR, and the Flipper shows whether the connection succeeded (with IP address) or failed.

## Features

- **Manual entry**: on-screen keyboard for SSID and password, security type selector (Open/WPA/WEP/SAE)
- **NFC WiFi tags**: scan an NTAG213/215/216 tag with WiFi credentials and transmit instantly
- **Saved networks**: credentials auto-save to the SD card after transmit. Browse, resend, or delete
- **Two IR protocols**: RC-6 (36kHz, for CIR receivers like the ITE8708 in Intel NUCs) or NEC (38kHz, for many consumer IR receivers), selectable in Settings
- **Fast transmission**: full credentials sent in under a second via RC-6, or a few seconds via NEC
- **Live progress**: animated transmit screen with a progress bar. Press Back to cancel a send
- **Hidden network support**: toggle the hidden SSID flag
- **ACK feedback**: optional. When enabled in Settings, the Flipper waits for a response from the server and shows "Connected!" with the IP address, or "Failed", with a one-press Retry

## Requirements

- A Linux server with an IR receiver (tested with the ITE8708 CIR in Intel NUCs and the Squeezebox Touch)
- The **infrafid** daemon running on the server. See [GitHub](https://github.com/amd989/InfraFi) for install instructions
- Pre-built .deb and .rpm packages are available from [GitHub Releases](https://github.com/amd989/InfraFi/releases)

## Usage

**Manual Entry:**
1. Open **InfraFi** on your Flipper
2. Select **Send Credentials**
3. Enter SSID, password, and security type
4. Review on the confirm screen, press **Send**
5. Point the Flipper at the server's IR receiver

**NFC Tag:**
1. Write WiFi credentials to an NFC tag using a phone app (e.g., **NFC Tools**)
2. Open **InfraFi** and select **Scan NFC Tag**
3. Hold the tag to the back of the Flipper
4. Review credentials, press **Send**

**Saved Networks:**
- Previously transmitted networks are auto-saved to the SD card
- Open **InfraFi** and select **Saved Networks** to browse and resend

**Settings:**
- **IR Protocol**: RC-6 (default) or NEC. Must match what the server's receiver is set up for
- **Wait for ACK**: turn on if the server has an IR transmitter for replies

## Tips

- The Flipper keyboard has no backslash, pipe, braces, brackets, angle brackets, or tilde. For passwords with these characters, use an NFC tag instead

## Credits

Some sprites are adapted from [Pixelarticons](https://github.com/halfmage/pixelarticons) (MIT License).

## Author

**Alejandro Mora** ([github.com/amd989](https://github.com/amd989))
